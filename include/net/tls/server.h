#pragma once
#include <net/server.h>
#include <net/tls/types.h>

namespace net::tls {

class server : public net::server {
public:
  explicit server(net::service& service) noexcept;

  server(server&& other) noexcept = default;
  server& operator=(server&& other) noexcept = default;

  server(const server& other) = delete;
  server& operator=(const server& other) = delete;

  ~server() {
    close();
  }

  // Creates and binds socket.
  void create(const char* host, const char* port, net::type type);

  // Configures ca, certificate, key and application layer prottocol negotiation.
  void config(const char* ca, const char* cer, const char* key, const char* alpn = nullptr);

  // Accepts client connections.
  net::async_generator<net::connection> accept(std::size_t backlog = 0) override;

  // Closes socket.
  std::error_code close() noexcept override;

private:
  context_ptr context_;
};

}  // namespace net::tls
