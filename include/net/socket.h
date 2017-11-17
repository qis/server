#pragma once
#include <net/async.h>
#include <net/error.h>
#include <net/service.h>
#include <net/tls.h>
#include <fmt/format.h>
#include <functional>
#include <memory>
#include <string_view>

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
  explicit socket(net::service& service) noexcept;
  explicit socket(net::service& service, handle_type value) noexcept;

  socket(socket&& other) noexcept;
  socket& operator=(socket&& other) noexcept;

  socket(const socket& other) = delete;
  socket& operator=(const socket& other) = delete;

  ~socket();

  // Creates tls/udp socket.
  void create(net::family family, net::type type);

  // Creates tls/dtls socket.
  void create(net::family family, net::type type, const tls& tls);

  // Sets socket option.
  std::error_code set(net::option option, bool enable) noexcept;

  // Performs handshake and returns selected alpn.
  net::task<std::optional<std::string>> handshake();

  // Reads data from socket.
  // Returns on closed connection.
  net::async_generator<std::string_view> recv(std::size_t size = 4096);

  // Reads data from socket.
  // Returns empty string_view on closed connection.
  net::task<std::string_view> recv(char* data, std::size_t size);

  // Writes data to the socket.
  // Returns false on closed connection.
  net::task<bool> send(std::string_view data);

  // Closes socket.
  std::error_code close() noexcept override;

  // Returns associated service.
  net::service& service() const noexcept {
    return service_.get();
  }

private:
  class impl;
  std::unique_ptr<impl> impl_;
  std::reference_wrapper<net::service> service_;
};

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const socket& socket);

}  // namespace net
