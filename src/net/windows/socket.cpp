#include <net/socket.h>
#include <net/address.h>
#include <net/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <tls.h>
#include <algorithm>

namespace net {

socket::socket(net::service& service) noexcept : service_(service) {
}

socket::socket(net::service& service, handle_type value) noexcept : handle(value), service_(service) {
}

void socket::create(net::family family, net::type type) {
  if (!service_.get()) {
    throw exception("create socket", std::errc::bad_file_descriptor);
  }
  const auto handle = WSASocket(to_int(family), to_int(type), 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
  if (handle == INVALID_SOCKET) {
    throw exception("create socket", WSAGetLastError());
  }
  socket socket(service_, handle);
  if (!CreateIoCompletionPort(socket.as<HANDLE>(), service_.get().as<HANDLE>(), 0, 0)) {
    throw exception("create socket completion port", GetLastError());
  }
  if (!SetFileCompletionNotificationModes(socket.as<HANDLE>(), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
    throw exception("set socket completion notification modes", GetLastError());
  }
  reset(socket.release());
}

void socket::accept(const tls& tls) {
  ::tls* client = nullptr;
  tls_data_ = std::make_unique<tls_data>();
  if (::tls_accept_cbs(tls.get(), &client, on_recv, on_send, tls_data_.get()) < 0) {
    throw exception("tls accept", ::tls_error(tls.get()));
  }
  tls_.reset(client);
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

net::task<bool> socket::handshake() {
  while (tls_) {
    switch (::tls_handshake(tls_.get())) {
    case 0: co_return true;
    case TLS_WANT_POLLIN:
      tls_data_->recv = co_await native_recv(tls_data_->buffer.data(), tls_data_->buffer.size());
      if (tls_data_->recv.empty()) {
        co_return false;
      }
      break;
    case TLS_WANT_POLLOUT:
      if (!co_await native_send(tls_data_->send)) {
        co_return false;
      }
      break;
    default: co_return false;
    }
  }
  co_return true;
}

std::error_code socket::close() noexcept {
  if (valid()) {
    tls_.reset();
    ::shutdown(as<SOCKET>(), SD_BOTH);
    if (::closesocket(as<SOCKET>()) == SOCKET_ERROR) {
      return { WSAGetLastError(), std::system_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

net::task<std::string_view> socket::tls_recv(char* data, std::size_t size) {
  while (true) {
    const auto rv = ::tls_read(tls_.get(), tls_data_->buffer.data(), tls_data_->buffer.size());
    if (rv > 0) {
      co_return std::string_view{ tls_data_->buffer.data(), static_cast<std::size_t>(rv) };
    }
    switch (rv) {
    case 0: co_return std::string_view{};
    case TLS_WANT_POLLIN:
      tls_data_->recv = co_await native_recv(data, size);
      if (tls_data_->recv.empty()) {
        co_return std::string_view{};
      }
      break;
    case TLS_WANT_POLLOUT:
      if (!co_await native_send(tls_data_->send)) {
        co_return std::string_view{};
      }
      break;
    default: throw exception("tls recv", ::tls_error(tls_.get()));
    }
  }
  co_return std::string_view{};
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
    case TLS_WANT_POLLOUT:
      if (!co_await native_send(tls_data_->send)) {
        co_return false;
      }
      break;
    case TLS_WANT_POLLIN:
      tls_data_->recv = co_await native_recv(tls_data_->buffer.data(), tls_data_->buffer.size());
      if (tls_data_->recv.empty()) {
        co_return false;
      }
      break;
    default: throw exception("tls send", ::tls_error(tls_.get()));
    }
  }
  co_return true;
}

net::task<std::string_view> socket::native_recv(char* data, std::size_t size) {
  WSABUF buffer = { static_cast<ULONG>(size), data };
  DWORD bytes = 0;
  DWORD flags = 0;
  event event;
  if (WSARecv(handle_, &buffer, 1, &bytes, &flags, &event, nullptr) == SOCKET_ERROR) {
    if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
      throw exception("recv", code);
    }
    bytes = co_await event;
    if (!bytes) {
      WSAGetOverlappedResult(handle_, &event, &bytes, FALSE, &flags);
      if (const auto code = WSAGetLastError()) {
        throw exception("async recv", code);
      }
      co_return std::string_view{};
    }
  } else if (!bytes) {
    co_return std::string_view{};
  }
  co_return std::string_view{ buffer.buf, bytes };
}

net::task<bool> socket::native_send(std::string_view data) {
  WSABUF buffer = { static_cast<ULONG>(data.size()), const_cast<char*>(data.data()) };
  while (buffer.len > 0) {
    DWORD bytes = 0;
    event event;
    if (WSASend(handle_, &buffer, 1, &bytes, 0, &event, nullptr) == SOCKET_ERROR) {
      if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
        throw exception("send", code);
      }
      bytes = co_await event;
      if (!bytes) {
        DWORD flags = 0;
        WSAGetOverlappedResult(handle_, &event, &bytes, FALSE, &flags);
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

long long socket::on_recv(::tls* ctx, void* data, size_t size, void* arg) noexcept {
  auto& tls_data = *reinterpret_cast<socket::tls_data*>(arg);
  if (!tls_data.recv.empty()) {
    size = std::min(tls_data.recv.size(), size);
    std::memcpy(data, tls_data.recv.data(), size);
    tls_data.recv = tls_data.recv.substr(size);
    return static_cast<ssize_t>(size);
  }
  return TLS_WANT_POLLIN;
}

long long socket::on_send(::tls* ctx, const void* data, size_t size, void* arg) noexcept {
  auto& tls_data = *reinterpret_cast<socket::tls_data*>(arg);
  if (tls_data.send.empty()) {
    tls_data.send = { reinterpret_cast<const char*>(data), size };
    return TLS_WANT_POLLOUT;
  }
  const auto ret = static_cast<ssize_t>(tls_data.send.size());
  tls_data.send = {};
  return ret;
}

}  // namespace net
