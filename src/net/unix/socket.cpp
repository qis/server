#include <net/socket.h>
#include <net/address.h>
#include <net/event.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <tls.h>

namespace net {
namespace {

net::task<std::string_view> native_recv(net::service& service, int socket, char* data, std::size_t size) {
  const auto buffer_data = data;
  const auto buffer_size = static_cast<socklen_t>(size);
  auto rv = ::read(socket, buffer_data, buffer_size);
  if (rv < 0) {
    if (errno != EAGAIN) {
      throw exception("recv", errno);
    }
    co_await event(service.value(), socket, NET_TLS_RECV);
    rv = ::read(socket, buffer_data, buffer_size);
    if (rv < 0) {
      throw exception("async recv", errno);
    }
  }
  co_return std::string_view{ buffer_data, static_cast<std::size_t>(rv) };
}

net::task<bool> native_send(net::service& service, int socket, std::string_view data) {
  auto buffer_data = data.data();
  auto buffer_size = static_cast<socklen_t>(data.size());
  while (buffer_size > 0) {
    auto rv = ::write(socket, buffer_data, buffer_size);
    if (rv < 0) {
      if (errno != EAGAIN) {
        throw exception("send", errno);
      }
      co_await event(service.value(), socket, NET_TLS_SEND);
      rv = ::write(socket, buffer_data, buffer_size);
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

}  // namespace

class socket::impl {
public:
  impl(const tls& ctx, net::service& service, int handle) : service_(service), handle_(handle) {
  }

  net::task<std::optional<std::string>> handshake() {
    while (true) {
      const auto rv = ::tls_handshake(tls_.get());
      if (rv == 0) {
        if (const auto alpn = tls_conn_alpn_selected(tls_.get())) {
          co_return std::make_optional<std::string>(alpn);
        }
        break;
      }
      if (rv == TLS_WANT_POLLOUT) {
        co_await event(service_.value(), handle_, NET_TLS_SEND);
        continue;
      }
      if (rv == TLS_WANT_POLLIN) {
        co_await event(service_.value(), handle_, NET_TLS_RECV);
        continue;
      }
      throw exception("tls handshake", ::tls_error(tls_.get()));
    }
    co_return std::optional<std::string>{};
  }

  net::task<std::string_view> recv(char* data, std::size_t size) {
    while (true) {
      auto rv = ::tls_read(tls_.get(), data, size);
      if (rv >= 0) {
        co_return std::string_view{ data, static_cast<std::size_t>(rv) };
      }
      if (rv == TLS_WANT_POLLIN) {
        co_await event(service_.value(), handle_, NET_TLS_RECV);
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        co_await event(service_.value(), handle_, NET_TLS_SEND);
        continue;
      }
      throw exception("tls recv", ::tls_error(tls_.get()));
    }
    co_return std::string_view{};
  }

  net::task<bool> send(std::string_view data) {
    auto buffer_data = data.data();
    auto buffer_size = data.size();
    while (buffer_size) {
      const auto rv = ::tls_write(tls_.get(), buffer_data, buffer_size);
      if (rv > 0) {
        buffer_data += rv;
        buffer_size -= static_cast<size_t>(rv);
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        co_await event(service_.value(), handle_, NET_TLS_SEND);
        continue;
      }
      if (rv == TLS_WANT_POLLIN) {
        co_await event(service_.value(), handle_, NET_TLS_RECV);
        continue;
      }
      if (rv == 0) {
        co_return false;
      }
      throw exception("tls send", ::tls_error(tls_.get()));
    }
    co_return true;
  }

private:
  net::tls tls_ = make_tls();
  net::service& service_;
  int handle_ = -1;
};

socket::socket(net::service& service) noexcept : service_(service) {
}

socket::socket(net::service& service, handle_type value) noexcept : handle(value), service_(service) {
}

socket::socket(socket&& other) noexcept : impl_(std::move(other.impl_)), service_(other.service_) {
  reset(other.release());
}

socket& socket::operator=(socket&& other) noexcept {
  impl_ = std::move(other.impl_);
  service_ = std::move(other.service_);
  reset(other.release());
  return *this;
}

socket::~socket() {
  close();
}

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

void socket::accept(const tls& tls) {
  impl_ = std::make_unique<socket::impl>(tls, service_, handle_);
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

net::task<std::optional<std::string>> socket::handshake() {
  if (impl_) {
    co_return co_await impl_->handshake();
  }
  co_return std::make_optional<std::string>();
}

net::task<std::string_view> socket::recv(char* data, std::size_t size) {
  return impl_ ? impl_->recv(data, size) : native_recv(service_, handle_, data, size);
}

net::task<bool> socket::send(std::string_view data) {
  return impl_ ? impl_->send(data) : native_send(service_, handle_, data);
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
