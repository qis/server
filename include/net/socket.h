#pragma once
#include <net/async.h>
#include <net/error.h>
#include <net/service.h>
#include <net/tls.h>
#include <fmt/format.h>
#include <string_view>

#ifdef WIN32
#include <array>
#include <functional>
#include <memory>
#endif

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

class socket final : public handle {
public:
#ifdef WIN32
  struct tls_data {
    std::string_view recv;
    std::string_view send;
    std::array<char, 4096> buffer;
  };
#endif

  explicit socket(net::service& service) noexcept;
  explicit socket(net::service& service, handle_type value) noexcept;

  socket(socket&& other) noexcept = default;
  socket& operator=(socket&& other) noexcept = default;

  socket(const socket& other) = delete;
  socket& operator=(const socket& other) = delete;

  ~socket();

  // Creates tls/udp socket.
  void create(net::family family, net::type type);

  // Accepts tls/dtls connection.
  void accept(const tls& tls);

  // Sets socket option.
  std::error_code set(net::option option, bool enable) noexcept;

  // Performs handshake and returns selected alpn.
  net::task<bool> handshake();

  // Reads data from socket.
  // Returns on closed connection.
  net::async_generator<std::string_view> recv(std::size_t size = 4096);

  // Reads data from socket.
  // Returns empty string_view on closed connection.
  net::task<std::string_view> recv(char* data, std::size_t size) {
    return tls_ ? tls_recv(data, size) : native_recv(data, size);
  }

  // Writes data to the socket.
  // Returns false on closed connection.
  net::task<bool> send(std::string_view data) {
    return tls_ ? tls_send(data) : native_send(data);
  }

  // Closes socket.
  std::error_code close() noexcept override;

  // Returns negotiated application layer protocol after a successfully completed handshake.
  std::string_view alpn() noexcept;

private:
  net::tls tls_ = make_tls();

  net::task<std::string_view> tls_recv(char* data, std::size_t size);
  net::task<bool> tls_send(std::string_view data);

  net::task<std::string_view> native_recv(char* data, std::size_t size);
  net::task<bool> native_send(std::string_view data);

#ifdef WIN32
  std::unique_ptr<tls_data> tls_data_;
  std::reference_wrapper<net::service> service_;
#else
  handle_type service_ = invalid_handle_value;
#endif
};

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const socket& socket);

}  // namespace net
