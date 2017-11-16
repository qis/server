#pragma once
#include <net/async.h>
#include <net/error.h>
#include <net/service.h>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace net {

enum class family {
  none,
  ipv4,
  ipv6,
};

enum class type {
  none,
  tcp,
  udp,
};

enum class option {
  nodelay,
};

class socket : public handle {
public:
  explicit socket(net::service& service) noexcept : service_(service) {
  }

  explicit socket(net::service& service, handle_type value) noexcept : handle(value), service_(service) {
  }

  socket(socket&& other) noexcept = default;
  socket& operator=(socket&& other) noexcept = default;

  socket(const socket& other) = delete;
  socket& operator=(const socket& other) = delete;

  ~socket() {
    close();
  }

  // Creates socket.
  void create(net::family family, net::type type);

  // Sets socket option.
  std::error_code set(net::option option, bool enable) noexcept;

  // Performs the protocol handshake.
  virtual net::task<bool> handshake();

  // Reads data from socket.
  // Returns on closed connection.
  net::async_generator<std::string_view> recv(std::size_t size = 4096) {
    std::vector<char> buffer;
    buffer.resize(size);
    while (true) {
      auto data = co_await recv(buffer.data(), buffer.size());
      if (data.empty()) {
        break;
      }
      co_yield data;
    }
    co_return;
  }

  // Reads data from socket.
  // Returns empty string_view on closed connection.
  virtual net::task<std::string_view> recv(char* data, std::size_t size);

  // Writes data to the socket.
  // Returns false on closed connection.
  virtual net::task<bool> send(std::string_view data);

  // Returns negotiated application layer protocol or nullptr.
  virtual const char* alpn() const noexcept;

  // Closes socket.
  std::error_code close() noexcept override;

  // Returns associated service.
  net::service& service() const noexcept {
    return service_.get();
  }

private:
  std::reference_wrapper<net::service> service_;
};

}  // namespace net
