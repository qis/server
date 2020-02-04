#pragma once
#include <net/server.hpp>

namespace net {

class session {
public:
  session(net::server& server, asio::ip::tcp::socket socket);

  auto operator()() noexcept -> asio::awaitable<void>;

  auto handle(const http::request<http::string_body>& request, beast::error_code& ec) -> asio::awaitable<void>;
  void client(const asio::ip::address& address);

  std::string_view client() noexcept
  {
    return client_;
  }

  constexpr net::server& server() noexcept
  {
    return server_;
  }

  constexpr const net::server& server() const noexcept
  {
    return server_;
  }

private:
  bool close_on_error(beast::error_code& ec, const char* what = nullptr);

  net::server& server_;
  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  std::string client_ = "CLIENT";
};

}  // namespace net
