#pragma once
#include <net/async.h>
#include <net/service.h>
#include <net/socket.h>
#include <net/tls.h>
#include <functional>
#include <string>

namespace net {

class server final : public handle {
public:
  explicit server(net::service& service);

  server(server&& other) noexcept = default;
  server& operator=(server&& other) noexcept = default;

  server(const server& other) = delete;
  server& operator=(const server& other) = delete;

  ~server();

  // Creates and binds a tcp/udp socket.
  void create(const std::string& host, const std::string& port, net::type type);

  // Configures tls/dtls.
  // The certificate file must be in PEM format and have the following order:
  // 1. Private key.
  // 2. Server certificate.
  // 3. Intermediate certificates (optional).
  // 4. Root ca certificates.
  void configure(const std::string& cert, const std::string& alpn = {});

  // Accepts client connections.
  net::async_generator<net::socket> accept(std::size_t backlog = 0);

  // Closes socket.
  std::error_code close() noexcept override;

  // Returns associated service.
  net::service& service() const noexcept {
    return service_.get();
  }

protected:
  std::reference_wrapper<net::service> service_;
  net::tls tls_ = make_tls();
};

}  // namespace net
