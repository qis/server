#include <net/server.h>
#include <net/address.h>
#include <net/event.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>

namespace net {

void server::create(const std::string& host, const std::string& port, net::type type) {
  if (!service_.get()) {
    throw exception("create server", std::errc::bad_file_descriptor);
  }
  const address address(host, port, type, AI_PASSIVE);
  socket socket(service_);
  socket.create(address.family(), address.type());
  auto reuseaddr = 1;
  if (::setsockopt(socket.value(), SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
    throw exception("set server option", errno);
  }
  if (::bind(socket.value(), address.addr(), address.addrlen()) < 0) {
    throw exception("bind", errno);
  }
  reset(socket.release());
}

net::async_generator<net::socket> server::accept(std::size_t backlog) {
  if (!service_.get()) {
    throw exception("accept", std::errc::bad_file_descriptor);
  }
  if (::listen(handle_, backlog > 0 ? static_cast<int>(backlog) : SOMAXCONN) < 0) {
    throw exception("listen", errno);
  }
  struct sockaddr_storage storage;
  auto addr = reinterpret_cast<struct sockaddr*>(&storage);
  while (true) {
    auto socklen = static_cast<socklen_t>(sizeof(storage));
    net::socket socket(service_, ::accept4(handle_, addr, &socklen, SOCK_NONBLOCK));
    if (socket) {
      if (tls_) {
        socket.accept(tls_);
      }
      co_yield socket;
      continue;
    }
    if (errno == EAGAIN) {
      co_await event(service_.get().value(), handle_, NET_TLS_RECV);
    }
    //if (errno == EINTR) {
    //  continue;
    //}
  }
  co_return;
}

std::error_code server::close() noexcept {
  if (valid()) {
    ::shutdown(handle_, SHUT_RDWR);
    while (true) {
      if (::close(handle_) < 0) {
        if (errno == EINTR) {
          continue;
        }
        return { errno, std::system_category() };
      }
      break;
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace net
