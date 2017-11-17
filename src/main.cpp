#include <fmt/format.h>
#include <fmt/ostream.h>
#include <net/http.h>
#include <net/server.h>
#include <net/utility.h>
#include <array>
#include <mutex>

// nc 127.0.0.1 8080
// openssl s_client -alpn h2 -connect 127.0.0.1:8080
// wrk -t2 -c10 -d3 --latency --timeout 1s http://127.0.0.1:8080
// wrk -t2 -c10 -d3 --latency --timeout 1s https://127.0.0.1:8080

// clang-format off

class session : public std::enable_shared_from_this<session> {
public:
  session(net::socket socket) noexcept : socket_(std::move(socket)) {
  }

  net::async recv() noexcept {
    const auto self = shared_from_this();
    try {
      for co_await(auto& request : net::http::recv(socket_)) {
        handle(std::move(request));
      }
    }
    catch (const std::exception& e) {
      fmt::print("{}: {}\n", socket_, e.what());
    }
    co_return;
  }

private:
  net::async handle(net::http::request request) noexcept {
    const auto self = shared_from_this();
    const auto lock = co_await mutex_.scoped_lock_async();
    try {
      for co_await(const auto& data : request.recv()) {
        (void)data;
      }
      if (request.closed) {
        co_return;
      }
      auto response = fmt::format(
        "{} 200 OK\r\n"
        "Server: deus/1.0.0\r\n"
        "Connection: {}\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 0\r\n\r\n",
        request.version, request.keep_alive ? "keep-alive" : "close"
      );
      (void)co_await socket_.send(response);
    }
    catch (const std::exception& e) {
      fmt::print("{}: {}\n", socket_, e.what());
    }
    co_return;
  }

  net::async_mutex mutex_;
  net::socket socket_;
};

net::async accept(net::server& server) noexcept {
  try {
    for co_await(auto& socket : server.accept()) {
      if (const auto ec = socket.set(net::option::nodelay, true)) {
        fmt::print("{}: [{}:{}] set nodelay error: {}\n", socket, ec.category().name(), ec.value(), ec.message());
      }
      std::make_shared<session>(std::move(socket))->recv();
    }
  }
  catch (const std::exception& e) {
    fmt::print(e.what());
  }
  co_return;
}

// clang-format on

int main(int argc, char* argv[]) {
  try {
    const std::string host = argc > 1 ? argv[1] : "0.0.0.0";
    const std::string port = argc > 2 ? argv[2] : "8080";
    const std::string cert = argc > 3 ? argv[3] : "res/cer/server.cer";
    const std::string alpn = argc > 4 ? argv[4] : "h2,http/1.1";

    // Create event loop.
    net::service service;

    // Trap SIGINT signals.
    net::signal(SIGINT, [&](int signum) { service.close(); });

    // Trap SIGPIPE signals.
    net::signal(SIGPIPE);

    // Create TCP server.
    net::server server(service);
    server.create(host, port, net::type::tcp);

    // Enable TLS support.
    if (!cert.empty()) {
      server.configure(cert, alpn);
    }

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
