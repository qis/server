#include <net/socket.h>
#include <net/address.h>
#include <net/event.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <tls.h>

namespace net {

socket::socket(net::service& service) noexcept : service_(service.value()) {
}

socket::socket(net::service& service, handle_type value) noexcept : handle(value), service_(service.value()) {
}

void socket::create(net::family family, net::type type) {
  if (service_ == invalid_handle_value) {
    throw exception("create socket", std::errc::bad_file_descriptor);
  }
  const auto handle = ::socket(to_int(family), to_int(type) | SOCK_NONBLOCK, 0);
  if (handle == -1) {
    throw exception("create socket", errno);
  }
  reset(handle);
}

void socket::accept(const tls& tls) {
  ::tls* client = nullptr;
  if (::tls_accept_socket(tls.get(), &client, handle_) < 0) {
    throw exception("tls accept", ::tls_error(tls.get()));
  }
  tls_.reset(client);
}

std::error_code socket::set(net::option option, bool enable) noexcept {
  auto level = 0;
  auto sockopt = 0;
  switch (option) {
  case option::nodelay:
#ifdef __linux__
    level = SOL_TCP;
#else
    level = SOL_SOCKET;
#endif
    sockopt = TCP_NODELAY;
    break;
  }
  auto value = enable ? 1 : 0;
  if (::setsockopt(handle_, level, sockopt, &value, sizeof(value)) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

net::task<bool> socket::handshake() {
  while (tls_) {
    switch (::tls_handshake(tls_.get())) {
    case 0: co_return true;
    case TLS_WANT_POLLIN: co_await event(service_, handle_, NET_TLS_RECV); break;
    case TLS_WANT_POLLOUT: co_await event(service_, handle_, NET_TLS_SEND); break;
    default: co_return false;
    }
  }
  co_return true;
}

std::error_code socket::close() noexcept {
  if (valid()) {
    ::shutdown(handle_, SHUT_RDWR);
    if (::close(handle_) < 0) {
      return { errno, std::system_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

net::task<std::string_view> socket::tls_recv(char* data, std::size_t size) {
  while (true) {
    const auto rv = ::tls_read(tls_.get(), data, size);
    if (rv > 0) {
      co_return std::string_view{ data, static_cast<std::size_t>(rv) };
    }
    switch (rv) {
    case 0: co_return std::string_view{};
    case TLS_WANT_POLLIN: co_await event(service_, handle_, NET_TLS_RECV); break;
    case TLS_WANT_POLLOUT: co_await event(service_, handle_, NET_TLS_SEND); break;
    default: throw exception("tls recv", ::tls_error(tls_.get()));
    }
  }
}

net::task<bool> socket::tls_send(std::string_view data) {
  auto buffer_data = data.data();
  auto buffer_size = data.size();
  while (buffer_size) {
    const auto rv = ::tls_write(tls_.get(), buffer_data, buffer_size);
    if (rv > 0) {
      buffer_data += rv;
      buffer_size -= static_cast<size_t>(rv);
      continue;
    }
    switch (rv) {
    case 0: co_return false;
    case TLS_WANT_POLLOUT: co_await event(service_, handle_, NET_TLS_SEND); break;
    case TLS_WANT_POLLIN: co_await event(service_, handle_, NET_TLS_RECV); break;
    default: throw exception("tls send", ::tls_error(tls_.get()));
    }
  }
  co_return true;
}

net::task<std::string_view> socket::native_recv(char* data, std::size_t size) {
  const auto buffer_data = data;
  const auto buffer_size = static_cast<socklen_t>(size);
  while (true) {
    const auto rv = ::read(handle_, buffer_data, buffer_size);
    if (rv > 0) {
      co_return std::string_view{ buffer_data, static_cast<std::size_t>(rv) };
    }
    if (rv == 0) {
      co_return std::string_view{};
    }
    if (errno != EAGAIN) {
      throw exception("recv", errno);
    }
    co_await event(service_, handle_, NET_TLS_RECV);
  }
}

net::task<bool> socket::native_send(std::string_view data) {
  auto buffer_data = data.data();
  auto buffer_size = static_cast<socklen_t>(data.size());
  while (buffer_size > 0) {
    const auto rv = ::write(handle_, buffer_data, buffer_size);
    if (rv > 0) {
      buffer_data += static_cast<socklen_t>(rv);
      buffer_size -= static_cast<socklen_t>(rv);
      continue;
    }
    if (rv == 0) {
      co_return false;
    }
    if (errno != EAGAIN) {
      throw exception("send", errno);
    }
    co_await event(service_, handle_, NET_TLS_SEND);
  }
  co_return true;
}

}  // namespace net
