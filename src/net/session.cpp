#include "session.hpp"
#include <version.h>

#define SERVER_VERSION_STRING PROJECT_NAME "/" PROJECT_VERSION

namespace net {
namespace {

constexpr std::string_view mime_type(std::string_view path) noexcept
{
  // clang-format off
  if (const auto i = path.rfind('.'); i != std::string_view::npos) {
    const auto ext = path.substr(i + 1);
    if (ext == "js")   return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "map")  return "application/json";
    if (ext == "txt")  return "text/plain";
    if (ext == "html") return "text/html";
    if (ext == "css")  return "text/css";
    if (ext == "gif")  return "image/gif";
    if (ext == "jpeg") return "image/jpeg";
    if (ext == "jpg")  return "image/jpeg";
    if (ext == "png")  return "image/png";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "ico")  return "image/x-icon";
  }
  // clang-format on
  return "application/octet-stream";
}

}  // namespace

session::session(net::server& server, asio::ip::tcp::socket socket) : server_(server), stream_(std::move(socket))
{
  stream_.expires_after(std::chrono::seconds(30));
}

bool session::close_on_error(beast::error_code& ec, const char* what)
{
  if (!ec) {
    return false;
  }
  if (ec != beast::error::timeout && ec != http::error::end_of_stream) {
    LOGE("[{}] {}: {} ({})", client_, ec.category().name(), what ? what : ec.message().data(), ec.value());
  }
  stream_.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
  return true;
}

auto session::operator()() noexcept -> asio::awaitable<void>
{
  try {
    if (!server_.config().server.proxied) {
      client(stream_.socket().remote_endpoint().address());
    }
    beast::error_code ec;
    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    co_await http::async_read(stream_, buffer, request, asio::redirect_error(asio::use_awaitable, ec));
    if (close_on_error(ec)) {
      co_return;
    }
    if (server_.config().server.proxied) {
      const auto it = request.find("X-Real-IP");
      if (it == request.end()) {
        http::response<http::string_body> response{ http::status::use_proxy, request.version() };
        response.set(http::field::server, SERVER_VERSION_STRING);
        response.set(http::field::content_type, "text/html");
        response.keep_alive(request.keep_alive());
        response.body() = "<code>Reverse proxy required. See server log for details.</code>";
        response.prepare_payload();
        LOGE("[{::^8}] Reverse proxy missing header: 'X-Real-IP'", client_);
        co_await http::async_write(stream_, response, asio::redirect_error(asio::use_awaitable, ec));
        stream_.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        co_return;
      }
      client(asio::ip::address::from_string(std::string{ it->value() }));
    }
    co_await handle(request, ec);
    if (close_on_error(ec)) {
      co_return;
    }
    while (request.version() > 10) {
      request.clear();
      co_await http::async_read(stream_, buffer, request, asio::redirect_error(asio::use_awaitable, ec));
      if (close_on_error(ec)) {
        co_return;
      }
      co_await handle(request, ec);
      if (close_on_error(ec)) {
        co_return;
      }
    }
  }
  catch (const boost::system::system_error& e) {
    if (auto ec = e.code(); ec != beast::error::timeout && ec != http::error::end_of_stream) {
      LOGE("[{}] {}: {} ({})", client_, ec.category().name(), e.what(), ec.value());
    }
  }
  catch (const std::system_error& e) {
    LOGC("[{}] {}: {} ({})", client_, e.code().category().name(), e.what(), e.code().value());
  }
  catch (const std::exception& e) {
    LOGE("[{}] {}", client_, e.what());
  }
  boost::system::error_code ec;
  stream_.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
  co_return;
}

auto session::handle(const http::request<http::string_body>& request, beast::error_code& ec) -> asio::awaitable<void>
{
  // Returns a bad request response.
  const auto bad_request = [&request](beast::string_view why) {
    http::response<http::string_body> response{ http::status::bad_request, request.version() };
    response.set(http::field::server, SERVER_VERSION_STRING);
    response.set(http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "<code>" + std::string(why) + "</code>";
    response.prepare_payload();
    return response;
  };

  // Returns a not found response.
  const auto not_found = [&request](beast::string_view target) {
    http::response<http::string_body> response{ http::status::not_found, request.version() };
    response.set(http::field::server, SERVER_VERSION_STRING);
    response.set(http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "<code>The resource '" + std::string(target) + "' was not found.</code>";
    response.prepare_payload();
    return response;
  };

  // Returns a server error response.
  const auto server_error = [&request](beast::string_view what) {
    http::response<http::string_body> response{ http::status::internal_server_error, request.version() };
    response.set(http::field::server, SERVER_VERSION_STRING);
    response.set(http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "<code>An error occurred: '" + std::string(what) + "'</code>";
    response.prepare_payload();
    return response;
  };

  // Make sure we can handle the method.
  if (request.method() != http::verb::get && request.method() != http::verb::head) {
    const auto response = bad_request("Unknown HTTP-method");
    LOGI("[{::^8}] {:03d} {} {}", client_, response.result(), request.method_string(), request.target());
    co_await http::async_write(stream_, response, asio::use_awaitable);
    co_return;
  }

  // Request path must be absolute and not contain "..".
  if (request.target().empty() || request.target()[0] != '/' || request.target().find("..") != beast::string_view::npos) {
    const auto response = bad_request("Illegal request-target");
    LOGI("[{::^8}] {:03d} {} {}", client_, response.result(), request.method_string(), request.target());
    co_await http::async_write(stream_, response, asio::use_awaitable);
    co_return;
  }

  if (request.target() == "/rest") {
    http::response<http::string_body> response{ http::status::ok, request.version() };
    response.set(http::field::server, SERVER_VERSION_STRING);
    response.set(http::field::content_type, "application/json");
    response.keep_alive(request.keep_alive());
    response.body() = json::to_string(json::object{ { "success", true } });
    response.prepare_payload();
    LOGI("[{::^8}] {:03d} {} {}", client_, response.result(), request.method_string(), request.target());
    co_await http::async_write(stream_, response, asio::use_awaitable);
    co_return;
  }

  // Build the path to the requested file.
  auto path = std::string{ server_.path() };
  path.append(request.target());
  if (request.target().back() == '/') {
    path.append("index.html");
  }

  // Attempt to open the file.
  http::file_body::value_type body;
  body.open(path.data(), beast::file_mode::scan, ec);
  if (ec && ec == beast::errc::permission_denied) {
    auto executor = co_await asio::this_coro::executor;
    auto timer = asio::system_timer{ executor };
    for (std::size_t i = 0; ec && ec == beast::errc::permission_denied && i < 500; i++) {
      timer.expires_after(std::chrono::milliseconds{ 20 });
      co_await timer.async_wait(asio::use_awaitable);
      body.open(path.data(), beast::file_mode::scan, ec);
    }
  }

  // Handle the case where the file doesn't exist.
  if (ec == beast::errc::no_such_file_or_directory) {
    const auto response = not_found(request.target());
    LOGI("[{::^8}] {:03d} {} {}", client_, response.result(), request.method_string(), request.target());
    co_await http::async_write(stream_, response, asio::use_awaitable);
    co_return;
  }

  // Handle an unknown error.
  if (ec) {
    const auto response = server_error(ec.message());
    LOGW("[{::^8}] {:03d} {} {} ({})", client_, response.result(), request.method_string(), request.target(), ec.message());
    co_await http::async_write(stream_, response, asio::use_awaitable);
    co_return;
  }

  // Cache the size since we need it after the move.
  auto const size = body.size();

  // Respond to HEAD request.
  if (request.method() == http::verb::head) {
    http::response<http::empty_body> response{ http::status::ok, request.version() };
    response.set(http::field::server, SERVER_VERSION_STRING);
    response.set(http::field::content_type, mime_type(path));
    response.content_length(size);
    response.keep_alive(request.keep_alive());
    LOGD("[{::^8}] {:03d} {} {}", client_, response.result(), request.method_string(), request.target());
    co_await http::async_write(stream_, response, asio::use_awaitable);
    co_return;
  }

  // Respond to GET request.
  http::response<http::file_body> response{
    std::piecewise_construct,
    std::make_tuple(std::move(body)),
    std::make_tuple(http::status::ok, request.version()),
  };
  response.set(http::field::server, SERVER_VERSION_STRING);
  response.set(http::field::content_type, mime_type(path));
  response.content_length(size);
  response.keep_alive(request.keep_alive());
  LOGI("[{::^8}] {:03d} {} {}", client_, response.result(), request.method_string(), request.target());
  co_await http::async_write(stream_, response, asio::use_awaitable);
  co_return;
}

void session::client(const asio::ip::address& address)
{
  if (address.is_v4()) {
    client_ = fmt::format("{:08X}", address.to_v4().to_ulong());
  } else if (address.is_v6()) {
    // clang-format off
    const auto bytes = address.to_v6().to_bytes();
    client_ = fmt::format(
      "{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
      bytes[0x0], bytes[0x1], bytes[0x2], bytes[0x3], bytes[0x4], bytes[0x5], bytes[0x6], bytes[0x7],
      bytes[0x8], bytes[0x9], bytes[0xA], bytes[0xB], bytes[0xC], bytes[0xD], bytes[0xE], bytes[0xF]);
    // clang-format on
  } else {
    client_ = address.to_string();
  }
}

}  // namespace net
