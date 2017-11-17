#include <net/socket.h>
#include <net/address.h>
#include <net/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <tls.h>
#include <array>
#include <vector>

namespace net {
namespace {

net::task<std::string_view> native_recv(handle& handle, char* data, std::size_t size) {
  WSABUF buffer = { static_cast<ULONG>(size), data };
  DWORD bytes = 0;
  DWORD flags = 0;
  event event;
  if (WSARecv(handle.as<SOCKET>(), &buffer, 1, &bytes, &flags, &event, nullptr) == SOCKET_ERROR) {
    if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
      throw exception("recv", code);
    }
    bytes = static_cast<DWORD>(co_await event);
    if (!bytes) {
      WSAGetOverlappedResult(handle.as<SOCKET>(), &event, &bytes, FALSE, &flags);
      if (const auto code = WSAGetLastError()) {
        throw exception("async recv", code);
      }
      co_return std::string_view{};
    }
  }
  co_return std::string_view{ buffer.buf, bytes };
}

net::task<bool> native_send(handle& handle, std::string_view data) {
  WSABUF buffer = { static_cast<ULONG>(data.size()), const_cast<char*>(data.data()) };
  while (buffer.len > 0) {
    DWORD bytes = 0;
    event event;
    if (WSASend(handle.as<SOCKET>(), &buffer, 1, &bytes, 0, &event, nullptr) == SOCKET_ERROR) {
      if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
        throw exception("send", code);
      }
      bytes = static_cast<DWORD>(co_await event);
      if (!bytes) {
        DWORD flags = 0;
        WSAGetOverlappedResult(handle.as<SOCKET>(), &event, &bytes, FALSE, &flags);
        if (const auto code = WSAGetLastError()) {
          throw exception("async send", code);
        }
        co_return false;
      }
    } else if (!bytes) {
      co_return false;
    }
    buffer.buf += bytes;
    buffer.len -= bytes;
  }
  co_return true;
}

}  // namespace

class socket::impl {
public:
  impl(const tls& ctx) {
    ::tls* client = nullptr;
    if (::tls_accept_cbs(ctx.get(), &client, on_recv, on_send, this) < 0) {
      throw exception("tls accept", ::tls_error(ctx.get()));
    }
    tls_.reset(client);
  }

  net::task<std::optional<std::string>> handshake(handle& handle) {
    while (true) {
      const auto rv = ::tls_handshake(tls_.get());
      if (rv == 0) {
        if (const auto alpn = tls_conn_alpn_selected(tls_.get())) {
          co_return alpn;
        }
        break;
      }
      if (rv == TLS_WANT_POLLIN) {
        recv_ = co_await native_recv(handle, buffer_.data(), buffer_.size());
        if (recv_.empty()) {
          co_return std::optional<std::string>{};
        }
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        if (!co_await native_send(handle, send_)) {
          co_return std::optional<std::string>{};
        }
        continue;
      }
      throw exception("tls handshake", ::tls_error(tls_.get()));
    }
    co_return std::string{};
  }

  net::task<std::string_view> recv(handle& handle, char* data, std::size_t size) {
    while (true) {
      const auto rv = ::tls_read(tls_.get(), buffer_.data(), buffer_.size());
      if (rv >= 0) {
        co_return std::string_view{ buffer_.data(), static_cast<std::size_t>(rv) };
      }
      if (rv == TLS_WANT_POLLIN) {
        recv_ = co_await native_recv(handle, data, size);
        if (recv_.empty()) {
          break;
        }
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        if (!co_await native_send(handle, send_)) {
          break;
        }
        continue;
      }
      throw exception("tls recv", ::tls_error(tls_.get()));
    }
    co_return std::string_view{};
  }

  net::task<bool> send(handle& handle, std::string_view data) {
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
        if (!co_await native_send(handle, send_)) {
          co_return false;
        }
        continue;
      }
      if (rv == TLS_WANT_POLLIN) {
        recv_ = co_await native_recv(handle, buffer_.data(), buffer_.size());
        if (recv_.empty()) {
          co_return false;
        }
        continue;
      }
      if (rv == 0) {
        co_return false;
      }
      throw exception("tls send", ::tls_error(tls_.get()));
    }
    co_return true;
  }

  static ssize_t on_recv(::tls* ctx, void* data, size_t size, void* arg) {
    auto& self = *reinterpret_cast<impl*>(arg);
    if (!self.recv_.empty()) {
      size = std::min(self.recv_.size(), size);
      std::memcpy(data, self.recv_.data(), size);
      self.recv_ = self.recv_.substr(size);
      return static_cast<ssize_t>(size);
    }
    return TLS_WANT_POLLIN;
  }

  static ssize_t on_send(::tls* ctx, const void* data, size_t size, void* arg) {
    auto& self = *reinterpret_cast<impl*>(arg);
    if (self.send_.empty()) {
      self.send_ = { reinterpret_cast<const char*>(data), size };
      return TLS_WANT_POLLOUT;
    }
    const auto ret = static_cast<ssize_t>(self.send_.size());
    self.send_ = {};
    return ret;
  }

private:
  std::string_view recv_;
  std::string_view send_;
  std::array<char, 2048> buffer_;
  net::tls tls_ = make_tls();
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
  const auto handle = WSASocket(to_int(family), to_int(type), 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
  if (handle == INVALID_SOCKET) {
    throw exception("create socket", WSAGetLastError());
  }
  *this = socket(service_, handle);
}

void socket::accept(const tls& tls) {
  impl_ = std::make_unique<socket::impl>(tls);
}

std::error_code socket::set(net::option option, bool enable) noexcept {
  auto level = 0;
  auto sockopt = 0;
  switch (option) {
  case option::nodelay:
    level = SOL_SOCKET;
    sockopt = TCP_NODELAY;
    break;
  default: return std::make_error_code(std::errc::invalid_argument);
  }
  BOOL value = enable ? TRUE : FALSE;
  auto value_data = reinterpret_cast<const char*>(&value);
  auto value_size = static_cast<int>(sizeof(value));
  if (::setsockopt(as<SOCKET>(), level, sockopt, value_data, value_size) == SOCKET_ERROR) {
    return { WSAGetLastError(), std::system_category() };
  }
  return {};
}

net::task<std::optional<std::string>> socket::handshake() {
  if (impl_) {
    co_return co_await impl_->handshake(*this);
  }
  co_return std::string{};
}

net::task<std::string_view> socket::recv(char* data, std::size_t size) {
  return impl_ ? impl_->recv(*this, data, size) : native_recv(*this, data, size);
}

net::task<bool> socket::send(std::string_view data) {
  return impl_ ? impl_->send(*this, data) : native_send(*this, data);
}

std::error_code socket::close() noexcept {
  if (valid()) {
    impl_.reset();
    ::shutdown(as<SOCKET>(), SD_BOTH);
    if (::closesocket(as<SOCKET>()) == SOCKET_ERROR) {
      return { WSAGetLastError(), std::system_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace net
