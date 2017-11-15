#pragma once
#include <net/socket.h>
#include <memory>

namespace net {

using connection = std::shared_ptr<net::socket>;

class server : public handle {
public:
  explicit server(net::service& service) noexcept : service_(service) {
  }

  explicit server(net::service& service, handle_type value) noexcept : handle(value), service_(service) {
  }

  server(server&& other) noexcept = default;
  server& operator=(server&& other) noexcept = default;

  server(const server& other) = delete;
  server& operator=(const server& other) = delete;

  ~server() {
    close();
  }

  // Creates and binds socket.
  void create(const char* host, const char* port, net::type type);

  // Accepts client connections.
  virtual net::async_generator<net::connection> accept(std::size_t backlog = 0);

  // Closes socket.
  std::error_code close() noexcept override;

  // Returns associated service.
  net::service& service() const noexcept {
    return service_.get();
  }

protected:
  std::reference_wrapper<net::service> service_;
};

}  // namespace net
