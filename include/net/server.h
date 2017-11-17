#pragma once
#include <net/async.h>
#include <net/service.h>
#include <net/socket.h>
#include <net/tls.h>
#include <functional>
#include <string>

namespace net {

class server : public handle {
public:
  explicit server(net::service& service);
  explicit server(net::service& service, handle_type value);

  server(server&& other) noexcept = default;
  server& operator=(server&& other) noexcept = default;

  server(const server& other) = delete;
  server& operator=(const server& other) = delete;

  ~server();

  // Creates and binds a tcp/udp socket.
  void create(std::string host, std::string port, net::type type);

  // Creates and binds a tls/dtls socket.
  // The certificate file contents must be: private key, server cert, intermediate ca cert, root ca cert.
  void create(std::string host, std::string port, net::type type, std::string cert, std::string alpn = {});

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
