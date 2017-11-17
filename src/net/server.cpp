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

void server::create(std::string host, std::string port, net::type type, std::string cert, std::string alpn) {
  const auto certificate = net::certificate(cert);
  if (tls_init() < 0) {
    throw exception("tls initialize", std::errc::not_enough_memory);
  }
  auto tls = make_tls(::tls_server());
  if (!tls) {
    throw exception("tls create server context", std::errc::not_enough_memory);
  }
  auto config = make_tls_config(::tls_config_new());
  if (!config) {
    throw exception("tls create config", std::errc::not_enough_memory);
  }
  if (::tls_config_set_ca_file(config.get(), certificate.ca()) < 0) {
    throw exception(std::string("tls load ca file"), ::tls_config_error(config.get()));
  }
  if (::tls_config_set_keypair_file(config.get(), certificate.cer(), certificate.key()) < 0) {
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
  if (::tls_configure(tls.get(), config.get()) < 0) {
    throw exception(std::string("tls configure"), ::tls_config_error(config.get()));
  }
  create(host, port, type);
  tls_ = std::move(tls);
}

}  // namespace net
