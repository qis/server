#include <fmt/format.h>
#include <fmt/ostream.h>
#include <net/server.h>
#include <net/utility.h>
#include <array>
#include <iostream>

// nc 127.0.0.1 8080
// openssl s_client -alpn h2 -connect 127.0.0.1:8080

// clang-format off

class session : public std::enable_shared_from_this<session> {
public:
  session(net::socket socket) noexcept : socket_(std::move(socket)) {
  }

  net::async recv() noexcept {
    const auto self = shared_from_this();
    std::array<char, 4096> buffer;
    while (true) {
      const auto& data = co_await socket_.recv(buffer.data(), buffer.size());
      if (data.empty()) {
        co_return;
      }
      if (!co_await socket_.send(data)) {
        break;
      }
    }
  }

private:
  net::socket socket_;
};

net::async accept(net::server& server) noexcept {
  try {
    for co_await(auto& socket : server.accept()) {
      if (const auto ec = socket.set(net::option::nodelay, true)) {
        std::cerr << "set nodelay error: " << ec.message() << '\n';
      }
      std::make_shared<session>(std::move(socket))->recv();
    }
  }
  catch (const std::exception& e) {
    std::cerr << "accept error: " << e.what() << '\n';
  }
}

// clang-format on

int main(int argc, char* argv[]) {
  try {
    const std::string host = argc > 1 ? argv[1] : "0.0.0.0";
    const std::string port = argc > 2 ? argv[2] : "8080";
    const std::string cert = argc > 3 ? argv[3] : "res/cer/server.cer";
    const std::string alpn = argc > 4 ? argv[4] : "h2";

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
    service.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
