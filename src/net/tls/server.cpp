#include <net/tls/server.h>
#include <net/event.h>
#include <tls.h>
#include <array>

namespace net::tls {
namespace {

constexpr auto cipher_list =
  "ECDHE-ECDSA-AES256-GCM-SHA384:"  // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AESGCM(256)    Mac=AEAD
  "ECDHE-ECDSA-CHACHA20-POLY1305:"  // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=ChaCha20(256)  Mac=AEAD
  "ECDHE-ECDSA-AES128-GCM-SHA256:"  // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AESGCM(128)    Mac=AEAD
  "ECDHE-ECDSA-AES256-SHA384:"      // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AES(256)       Mac=SHA384
  "ECDHE-ECDSA-AES128-SHA256:"      // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AES(128)       Mac=SHA256
  "ECDHE-RSA-AES256-GCM-SHA384:"    // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AESGCM(256)    Mac=AEAD
  "ECDHE-RSA-CHACHA20-POLY1305:"    // TLSv1.2  Kx=ECDH  Au=RSA    Enc=ChaCha20(256)  Mac=AEAD
  "ECDHE-RSA-AES128-GCM-SHA256:"    // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AESGCM(128)    Mac=AEAD
  "ECDHE-RSA-AES256-SHA384:"        // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AES(256)       Mac=SHA384
  "ECDHE-RSA-AES128-SHA256:";       // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AES(128)       Mac=SHA256

using config = ::tls_config;
using config_ptr = std::unique_ptr<config, void (*)(config*)>;

inline config_ptr make_config(::tls_config* ptr = nullptr) noexcept {
  static const auto free = [](config* ptr) {
    if (ptr) {
      ::tls_config_free(ptr);
    }
  };
  return { static_cast<config*>(ptr), free };
}

inline context_ptr make_context(::tls* ptr = nullptr) noexcept {
  static const auto free = [](context* ptr) {
    if (ptr) {
      ::tls_close(ptr);
      ::tls_free(ptr);
    }
  };
  return { static_cast<context*>(ptr), free };
}

}  // namespace

#ifdef NET_USE_IOCP

class socket final : public net::socket {
public:
  socket(net::socket socket, net::tls::context* context) : net::socket(std::move(socket)) {
    // clang-format off
    static const auto on_recv = [](::tls* ctx, void* data, size_t size, void* arg) noexcept -> ssize_t {
      auto& self = *reinterpret_cast<net::tls::socket*>(arg);
      if (!self.recv_.empty()) {
        size = std::min(self.recv_.size(), size);
        std::memcpy(data, self.recv_.data(), size);
        self.recv_ = self.recv_.substr(size);
        return static_cast<ssize_t>(size);
      }
      return TLS_WANT_POLLIN;
    };
    static const auto on_send = [](::tls* ctx, const void* data, size_t size, void* arg) noexcept -> ssize_t {
      auto& self = *reinterpret_cast<net::tls::socket*>(arg);
      if (self.send_.empty()) {
        self.send_ = { reinterpret_cast<const char*>(data), size };
        return TLS_WANT_POLLOUT;
      }
      const auto ret = static_cast<ssize_t>(self.send_.size());
      self.send_ = {};
      return ret;
    };
    // clang-format on

    ::tls* client = nullptr;
    if (::tls_accept_cbs(context, &client, on_recv, on_send, this) < 0) {
      throw exception("tls accept", ::tls_error(context));
    }
    context_.reset(client);
  }

  socket(socket&& other) noexcept = default;
  socket& operator=(socket&& other) noexcept = default;

  socket(const socket& other) = delete;
  socket& operator=(const socket& other) = delete;

  ~socket() override;

  net::task<bool> handshake() override {
    while (true) {
      const auto rv = ::tls_handshake(context_.get());
      if (rv == 0) {
        co_return true;
      }
      if (rv == TLS_WANT_POLLIN) {
        recv_ = co_await net::socket::recv(buffer_.data(), buffer_.size());
        if (recv_.empty()) {
          co_return false;
        }
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        if (!co_await net::socket::send(send_)) {
          co_return false;
        }
        continue;
      }
      throw exception("tls handshake", ::tls_error(context_.get()));
    }
    co_return true;
  }

  net::task<std::string_view> recv(char* data, std::size_t size) override {
    while (true) {
      const auto rv = ::tls_read(context_.get(), buffer_.data(), buffer_.size());
      if (rv >= 0) {
        co_return std::string_view{ buffer_.data(), static_cast<std::size_t>(rv) };
      }
      if (rv == TLS_WANT_POLLIN) {
        recv_ = co_await net::socket::recv(data, size);
        if (recv_.empty()) {
          co_return std::string_view{};
        }
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        if (!co_await net::socket::send(send_)) {
          co_return std::string_view{};
        }
        continue;
      }
      throw exception("tls recv", ::tls_error(context_.get()));
    }
    co_return std::string_view{};
  }

  net::task<bool> send(std::string_view data) override {
    auto buffer_data = data.data();
    auto buffer_size = data.size();
    while (buffer_size) {
      const auto rv = ::tls_write(context_.get(), buffer_data, buffer_size);
      if (rv > 0) {
        buffer_data += rv;
        buffer_size -= static_cast<size_t>(rv);
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        if (!co_await net::socket::send(send_)) {
          co_return false;
        }
        continue;
      }
      if (rv == TLS_WANT_POLLIN) {
        recv_ = co_await net::socket::recv(buffer_.data(), buffer_.size());
        if (recv_.empty()) {
          co_return false;
        }
        continue;
      }
      if (rv == 0) {
        co_return false;
      }
      throw exception("tls send", ::tls_error(context_.get()));
    }
    co_return true;
  }

  std::error_code close() noexcept override;
  const char* alpn() const noexcept override;

private:
  std::string_view recv_;
  std::string_view send_;
  std::array<char, 2048> buffer_;
  context_ptr context_ = make_context();
};

#else

class socket final : public net::socket {
public:
  socket(net::socket socket, net::tls::context* context) : net::socket(std::move(socket)) {
    ::tls* client = nullptr;
    if (::tls_accept_socket(context, &client, handle_) < 0) {
      throw exception("tls accept", ::tls_error(context));
    }
    context_.reset(client);
  }

  socket(socket&& other) noexcept = default;
  socket& operator=(socket&& other) noexcept = default;

  socket(const socket& other) = delete;
  socket& operator=(const socket& other) = delete;

  ~socket() override;

  net::task<bool> handshake() override {
    while (true) {
      const auto rv = ::tls_handshake(context_.get());
      if (rv == 0) {
        co_return true;
      }
      if (rv == TLS_WANT_POLLOUT) {
        co_await event(service().value(), handle_, NET_TLS_SEND);
        continue;
      }
      if (rv == TLS_WANT_POLLIN) {
        co_await event(service().value(), handle_, NET_TLS_RECV);
        continue;
      }
      throw exception("tls handshake", ::tls_error(context_.get()));
    }
    co_return true;
  }

  net::task<std::string_view> recv(char* data, std::size_t size) override {
    while (true) {
      auto rv = ::tls_read(context_.get(), data, size);
      if (rv >= 0) {
        co_return std::string_view{ data, static_cast<std::size_t>(rv) };
      }
      if (rv == TLS_WANT_POLLIN) {
        co_await event(service().value(), handle_, NET_TLS_RECV);
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        co_await event(service().value(), handle_, NET_TLS_SEND);
        continue;
      }
      throw exception("tls recv", ::tls_error(context_.get()));
    }
    co_return std::string_view{};
  }

  net::task<bool> send(std::string_view data) override {
    auto buffer_data = data.data();
    auto buffer_size = data.size();
    while (buffer_size) {
      const auto rv = ::tls_write(context_.get(), buffer_data, buffer_size);
      if (rv > 0) {
        buffer_data += rv;
        buffer_size -= static_cast<size_t>(rv);
        continue;
      }
      if (rv == TLS_WANT_POLLOUT) {
        co_await event(service().value(), handle_, NET_TLS_SEND);
        continue;
      }
      if (rv == TLS_WANT_POLLIN) {
        co_await event(service().value(), handle_, NET_TLS_RECV);
        continue;
      }
      if (rv == 0) {
        co_return false;
      }
      throw exception("tls send", ::tls_error(context_.get()));
    }
    co_return true;
  }

  std::error_code close() noexcept override;
  const char* alpn() const noexcept override;

private:
  context_ptr context_ = make_context();
};

#endif

socket::~socket() {
  close();
}

std::error_code socket::close() noexcept {
  context_.reset();
  return net::socket::close();
}

const char* socket::alpn() const noexcept {
  return tls_conn_alpn_selected(context_.get());
}

server::server(net::service& service) noexcept : net::server(service), context_(make_context()) {
}

void server::create(const char* host, const char* port, net::type type) {
  if (tls_init() < 0) {
    throw exception("tls initialize", std::errc::not_enough_memory);
  }
  net::server socket(service());
  socket.create(host, port, type);

  auto context = make_context(::tls_server());
  if (!context) {
    throw exception("tls create context", std::errc::not_enough_memory);
  }
  static_cast<net::server&>(*this) = std::move(socket);
  context_ = std::move(context);
}

void server::config(const char* ca, const char* cer, const char* key, const char* alpn) {
  auto config = make_config(::tls_config_new());
  if (!config) {
    throw exception("tls create config", std::errc::not_enough_memory);
  }
  if (::tls_config_set_ca_file(config.get(), ca) < 0) {
    throw exception(std::string("tls load ca file"), ::tls_config_error(config.get()));
  }
  if (::tls_config_set_keypair_file(config.get(), cer, key) < 0) {
    throw exception(std::string("tls load certificate and key files"), ::tls_config_error(config.get()));
  }
  if (::tls_config_set_protocols(config.get(), TLS_PROTOCOLS_DEFAULT) < 0) {
    throw exception(std::string("tls set protocol"), ::tls_config_error(config.get()));
  }
  if (::tls_config_set_ciphers(config.get(), cipher_list) < 0) {
    throw exception(std::string("tls set ciphers"), ::tls_config_error(config.get()));
  }
  if (alpn && ::tls_config_set_alpn(config.get(), alpn) < 0) {
    throw exception(std::string("tls set alpn"), ::tls_config_error(config.get()));
  }
  ::tls_config_prefer_ciphers_server(config.get());
  if (::tls_configure(context_.get(), config.get()) < 0) {
    throw exception(std::string("tls configure context"), ::tls_error(context_.get()));
  }
}

// clang-format off

net::async_generator<net::connection> server::accept(std::size_t backlog) {
  for co_await(auto& connection : net::server::accept(backlog)) {
    auto tls_connection = std::make_shared<net::tls::socket>(std::move(*connection), context_.get());
    co_yield tls_connection;
  }
  co_return;
}

// clang-format on

std::error_code server::close() noexcept {
  context_.reset();
  net::server::close();
  return {};
}

}  // namespace net::tls
