#pragma once
#include <net/error.h>
#include <net/socket.h>
#include <net/tls/types.h>
#include <memory>
#include <string>
#include <tls.h>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

namespace net {

class address_error_category : public std::error_category {
public:
  const char* name() const noexcept override {
    return "address";
  }

  std::string message(int condition) const override {
#ifdef WIN32
    return ::gai_strerrorA(condition);
#else
    return ::gai_strerror(condition);
#endif
  }
};

inline const std::error_category& address_category() noexcept {
  static const address_error_category category;
  return category;
}

inline int to_int(family family) noexcept {
  switch (family) {
  case family::none: return 0;
  case family::ipv4: return AF_INET;
  case family::ipv6: return AF_INET6;
  }
  return static_cast<int>(family);
}

inline int to_int(type type) noexcept {
  switch (type) {
  case type::none: return 0;
  case type::tcp: return SOCK_STREAM;
  case type::udp: return SOCK_DGRAM;
  }
  return static_cast<int>(type);
}

class address {
public:
  address(const char* host, const char* port, net::type type, int flags) : info_(nullptr, freeaddrinfo) {
    struct addrinfo* info = nullptr;
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = to_int(type);
    hints.ai_flags = flags;
    if (const auto rv = ::getaddrinfo(host, port, &hints, &info)) {
      throw exception("get address info", rv, address_category());
    }
    info_.reset(info);
  }

  auto family() const noexcept {
    switch (info_->ai_family) {
    case AF_INET: return family::ipv4;
    case AF_INET6: return family::ipv6;
    }
    return static_cast<net::family>(info_->ai_family);
  }

  auto type() const noexcept {
    switch (info_->ai_socktype) {
    case SOCK_STREAM: return type::tcp;
    case SOCK_DGRAM: return type::udp;
    }
    return static_cast<net::type>(info_->ai_socktype);
  }

  auto addr() const noexcept {
    return info_->ai_addr;
  }

  auto addrlen() const noexcept {
    return info_->ai_addrlen;
  }

private:
  std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> info_;
};

namespace tls {

constexpr auto ecdsa_cipher_list =
  "ECDHE-ECDSA-AES256-GCM-SHA384:"  // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AESGCM(256)    Mac=AEAD
  "ECDHE-ECDSA-CHACHA20-POLY1305:"  // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=ChaCha20(256)  Mac=AEAD
  "ECDHE-ECDSA-AES128-GCM-SHA256:"  // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AESGCM(128)    Mac=AEAD
  "ECDHE-ECDSA-AES256-SHA384:"      // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AES(256)       Mac=SHA384
  "ECDHE-ECDSA-AES128-SHA256:";     // TLSv1.2  Kx=ECDH  Au=ECDSA  Enc=AES(128)       Mac=SHA256

constexpr auto rsa_cipher_list =
  "ECDHE-RSA-AES256-GCM-SHA384:"  // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AESGCM(256)    Mac=AEAD
  "ECDHE-RSA-CHACHA20-POLY1305:"  // TLSv1.2  Kx=ECDH  Au=RSA    Enc=ChaCha20(256)  Mac=AEAD
  "ECDHE-RSA-AES128-GCM-SHA256:"  // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AESGCM(128)    Mac=AEAD
  "ECDHE-RSA-AES256-SHA384:"      // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AES(256)       Mac=SHA384
  "ECDHE-RSA-AES128-SHA256:";     // TLSv1.2  Kx=ECDH  Au=RSA    Enc=AES(128)       Mac=SHA256

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

}  // namespace tls
}  // namespace net
