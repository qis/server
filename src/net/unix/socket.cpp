#include <net/socket.h>
#include <net/address.h>
#include <net/event.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>

namespace net {

void socket::create(net::family family, net::type type) {
  if (!service_.get()) {
    throw exception("create socket", std::errc::bad_file_descriptor);
  }
  socket socket(service_, ::socket(to_int(family), to_int(type) | SOCK_NONBLOCK, 0));
  if (!socket) {
    throw exception("create socket", errno);
  }
  reset(socket.release());
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
  co_return true;
}

// clang-format off

net::task<std::string_view> socket::recv(char* data, std::size_t size) {
  const auto buffer_data = data;
  const auto buffer_size = static_cast<socklen_t>(size);
  auto rv = ::read(handle_, buffer_data, buffer_size);
  if (rv < 0) {
    if (errno != EAGAIN) {
      throw exception("recv", errno);
    }
    co_await event(service_.get().value(), handle_, NET_TLS_RECV);
    rv = ::read(handle_, buffer_data, buffer_size);
    if (rv < 0) {
      throw exception("async recv", errno);
    }
  }
  co_return std::string_view{ buffer_data, static_cast<std::size_t>(rv) };
}

net::task<bool> socket::send(std::string_view data) {
  auto buffer_data = data.data();
  auto buffer_size = static_cast<socklen_t>(data.size());
  while (buffer_size > 0) {
    auto rv = ::write(handle_, buffer_data, buffer_size);
    if (rv < 0) {
      if (errno != EAGAIN) {
        throw exception("send", errno);
      }
      co_await event(service_.get().value(), handle_, NET_TLS_SEND);
      rv = ::write(handle_, buffer_data, buffer_size);
      if (rv < 0) {
        throw exception("async send", errno);
      }
    }
    if (rv == 0) {
      co_return false;
    }
    const auto bytes = static_cast<socklen_t>(rv);
    buffer_data += bytes;
    buffer_size -= bytes;
  }
  co_return true;
}

// clang-format on

const char* socket::alpn() const noexcept {
  return nullptr;
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

}  // namespace net
