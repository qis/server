#include <fmt/format.h>
#include <fmt/ostream.h>
#include <net/http.h>
#include <net/tls/server.h>
#include <net/utility.h>
#include <array>
#include <mutex>

// openssl s_client -alpn h2 -connect 127.0.0.1:8080

const std::string g_response =
  "HTTP/1.1 200 OK\r\n"
  "Server: deus/1.0.0\r\n"
  "Content-Type: text/plain\r\n"
  "Content-Length: 2\r\n"
  "Connection: keep-alive\r\n"
  "\r\n"
  "ok";

// clang-format off

class session : public std::enable_shared_from_this<session> {
public:
  session(net::connection connection) noexcept : connection_(std::move(connection)) {
  }

  net::async handle(net::http::request& request) {
    for co_await(const auto& data : request.recv()) {
      (void)data;
    }
    send(g_response);
    co_return;
  }

  net::async recv() noexcept {
    const auto self = shared_from_this();
    try {
      for co_await(auto& request : net::http::recv(connection_)) {
        handle(request);
      }
    }
    catch (const std::exception& e) {
      fmt::print("{}: {}\n", connection_, e.what());
    }
    co_return;
  }

  net::async send(std::string data) noexcept {
    const auto self = shared_from_this();
    try {
      net::async_mutex_lock lock = co_await mutex_.scoped_lock_async();
      (void) co_await connection_->send(data);
    }
    catch (const std::exception& e) {
      fmt::print("{}: {}\n", connection_, e.what());
    }
    co_return;
  }

private:
  net::async_mutex mutex_;
  net::connection connection_;
};

net::async accept(net::server& server) noexcept {
  try {
    for co_await(auto& connection : server.accept()) {
      if (const auto ec = connection->set(net::option::nodelay, true)) {
        fmt::print("{}: [{}:{}] set nodelay error: {}\n", connection, ec.category().name(), ec.value(), ec.message());
      }
      std::make_shared<session>(std::move(connection))->recv();
    }
  }
  catch (const std::exception& e) {
    fmt::print(e.what());
  }
  co_return;
}

// clang-format on

int main(int argc, char* argv[]) {
  const auto host = argc > 1 ? argv[1] : "0.0.0.0";
  const auto port = argc > 2 ? argv[2] : "8080";
  try {
    // Create event loop.
    net::service service;

    // Trap SIGINT signals.
    net::signal(SIGINT, [&](int signum) { service.close(); });

    // Trap SIGPIPE signals.
    net::signal(SIGPIPE);

    // Create TLS Server.
    net::server server(service);
    server.create(host, port, net::type::tcp);
    //server.config("res/cer/ca.cer", "res/cer/server.cer", "res/cer/server.key", "h2,http/1.1");

    // Drop privileges.
    net::drop("nobody");

    // Accept incoming connections.
    accept(server);

    // Run event loop.
    fmt::print("{}:{}\n", host, port);
    service.run();
  } catch (const std::exception& e) {
    fmt::print("{}\n", e.what());
    return 1;
  }
}
