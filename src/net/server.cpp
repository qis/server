#include <net/server.h>
#include <tls.h>

namespace net {

server::server(net::service& service) : service_(service) {
}

server::server(net::service& service, handle_type value) : handle(value), service_(service) {
}

server::~server() {
  close();
}

void server::configure(const std::string& cert, const std::string& alpn) {
  if (tls_init() < 0) {
    throw exception("tls initialize", std::errc::not_enough_memory);
  }

  net::certificate certificate;
  certificate.load(cert);

  auto config = make_tls_config(::tls_config_new());
  if (!config) {
    throw exception("tls create config", std::errc::not_enough_memory);
  }

  const auto ca_data = reinterpret_cast<const uint8_t*>(certificate.ca().data());
  const auto ca_size = certificate.ca().size();
  if (::tls_config_set_ca_mem(config.get(), ca_data, ca_size) < 0) {
    throw exception(std::string("tls load ca file"), ::tls_config_error(config.get()));
  }

  const auto cer_data = reinterpret_cast<const uint8_t*>(certificate.cer().data());
  const auto cer_size = certificate.cer().size();
  const auto key_data = reinterpret_cast<const uint8_t*>(certificate.key().data());
  const auto key_size = certificate.key().size();
  if (::tls_config_set_keypair_mem(config.get(), cer_data, cer_size, key_data, key_size) < 0) {
    throw exception(std::string("tls load certificate and key files"), ::tls_config_error(config.get()));
  }

  if (::tls_config_set_protocols(config.get(), TLS_PROTOCOLS_DEFAULT) < 0) {
    throw exception(std::string("tls set protocol"), ::tls_config_error(config.get()));
  }
  if (::tls_config_set_ciphers(config.get(), rsa_cipher_list) < 0) {
    throw exception(std::string("tls set ciphers"), ::tls_config_error(config.get()));
  }
  if (!alpn.empty() && ::tls_config_set_alpn(config.get(), alpn.data()) < 0) {
    throw exception(std::string("tls set alpn"), ::tls_config_error(config.get()));
  }
  ::tls_config_prefer_ciphers_server(config.get());

  auto tls = make_tls(::tls_server());
  if (!tls) {
    throw exception("tls create server context", std::errc::not_enough_memory);
  }
  if (::tls_configure(tls.get(), config.get()) < 0) {
    throw exception(std::string("tls configure"), ::tls_error(tls.get()));
  }
  tls_ = std::move(tls);
}

}  // namespace net
