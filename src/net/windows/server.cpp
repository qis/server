#include <net/server.h>
#include <net/address.h>
#include <net/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <array>

namespace net {
namespace {

net::family get_family(SOCKET socket) {
  struct sockaddr_storage addr;
  auto addr_data = reinterpret_cast<sockaddr*>(&addr);
  auto addr_size = static_cast<int>(sizeof(addr));
  if (::getsockname(socket, addr_data, &addr_size) == SOCKET_ERROR) {
    throw exception("get socket family", WSAGetLastError());
  }
  return static_cast<net::family>(addr.ss_family);
}

net::type get_type(SOCKET socket) {
  auto type = 0;
  auto type_data = reinterpret_cast<char*>(&type);
  auto type_size = static_cast<int>(sizeof(type));
  if (::getsockopt(socket, SOL_SOCKET, SO_TYPE, type_data, &type_size) == SOCKET_ERROR) {
    throw exception("get socket type", WSAGetLastError());
  }
  return static_cast<net::type>(type);
}

}  // namespace

void server::create(const std::string& host, const std::string& port, net::type type) {
  if (!service_.get()) {
    throw exception("create server", std::errc::bad_file_descriptor);
  }
  const address address(host, port, type, AI_PASSIVE);
  socket socket(service_);
  socket.create(address.family(), address.type());
  BOOL reuseaddr = TRUE;
  const auto reuseaddr_data = reinterpret_cast<const char*>(&reuseaddr);
  const auto reuseaddr_size = static_cast<int>(sizeof(reuseaddr));
  if (::setsockopt(socket.as<SOCKET>(), SOL_SOCKET, SO_REUSEADDR, reuseaddr_data, reuseaddr_size) == SOCKET_ERROR) {
    throw exception("set server option", WSAGetLastError());
  }
  if (::bind(socket.as<SOCKET>(), address.addr(), static_cast<int>(address.addrlen())) == SOCKET_ERROR) {
    throw exception("bind", WSAGetLastError());
  }
  reset(socket.release());
}

net::async_generator<net::socket> server::accept(std::size_t backlog) {
  if (!service_.get()) {
    throw exception("accept", std::errc::bad_file_descriptor);
  }
  constexpr int size = sizeof(struct sockaddr_storage) + 16;
  if (::listen(as<SOCKET>(), backlog > 0 ? static_cast<int>(backlog) : SOMAXCONN) == SOCKET_ERROR) {
    throw exception("listen", WSAGetLastError());
  }
  const auto family = get_family(as<SOCKET>());
  const auto type = get_type(as<SOCKET>());
  std::array<char, size * 2> buffer;
  while (true) {
    net::socket socket(service_);
    socket.create(family, type);
    if (tls_) {
      socket.accept(tls_);
    }
    DWORD bytes = 0;
    event event;
    if (!AcceptEx(as<SOCKET>(), socket.as<SOCKET>(), buffer.data(), 0, size, size, &bytes, &event)) {
      if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
        continue;
      }
      (void)co_await event;
      DWORD flags = 0;
      WSAGetOverlappedResult(as<SOCKET>(), &event, &bytes, FALSE, &flags);
      if (const auto code = WSAGetLastError()) {
        continue;
      }
    }
    co_yield socket;
  }
  co_return;
}

std::error_code server::close() noexcept {
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
