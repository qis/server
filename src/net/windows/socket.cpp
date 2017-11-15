#include <net/socket.h>
#include <net/address.h>
#include <net/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

namespace net {

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
  co_return true;
}

// clang-format off

net::task<std::string_view> socket::recv(char* data, std::size_t size) {
  WSABUF buffer = { static_cast<ULONG>(size), data };
  DWORD bytes = 0;
  DWORD flags = 0;
  event event;
  if (WSARecv(as<SOCKET>(), &buffer, 1, &bytes, &flags, &event, nullptr) == SOCKET_ERROR) {
    if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
      throw exception("recv", code);
    }
    bytes = co_await event;
    if (!bytes) {
      WSAGetOverlappedResult(as<SOCKET>(), &event, &bytes, FALSE, &flags);
      if (const auto code = WSAGetLastError()) {
        throw exception("async recv", code);
      }
    }
  }
  co_return std::string_view{ buffer.buf, bytes };
}

net::task<bool> socket::send(std::string_view data) {
  WSABUF buffer = { static_cast<ULONG>(data.size()), const_cast<char*>(data.data()) };
  while (buffer.len > 0) {
    DWORD bytes = 0;
    event event;
    if (WSASend(as<SOCKET>(), &buffer, 1, &bytes, 0, &event, nullptr) == SOCKET_ERROR) {
      if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
        throw exception("send", code);
      }
      bytes = co_await event;
      if (!bytes) {
        DWORD flags = 0;
        WSAGetOverlappedResult(as<SOCKET>(), &event, &bytes, FALSE, &flags);
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

// clang-format on

const char* socket::alpn() const noexcept {
  return nullptr;
}

std::error_code socket::close() noexcept {
  if (valid()) {
    ::shutdown(as<SOCKET>(), SD_BOTH);
    if (::closesocket(as<SOCKET>()) == SOCKET_ERROR) {
      return { WSAGetLastError(), std::system_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace net
