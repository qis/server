#pragma once
#include <memory>
#include <string>

extern "C" {

struct tls;
struct tls_config;

}  // extern "C"

namespace net {

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

void tls_deleter(::tls* ptr) noexcept;
void tls_config_deleter(::tls_config* ptr) noexcept;

using tls = std::unique_ptr<::tls, decltype(&tls_deleter)>;
using tls_config = std::unique_ptr<::tls_config, decltype(&tls_config_deleter)>;

inline tls make_tls(::tls* ptr = nullptr) noexcept {
  return { ptr, tls_deleter };
}

inline tls_config make_tls_config(::tls_config* ptr = nullptr) noexcept {
  return { ptr, tls_config_deleter };
}

class certificate {
public:
  certificate() = default;
  certificate(const std::string& filename);

  certificate(certificate&& other);
  certificate& operator=(certificate&& other);

  certificate(const certificate& other) = delete;
  certificate& operator=(const certificate& other) = delete;

  ~certificate();

  const char* ca() const noexcept {
    return ca_.data();
  }

  const char* cer() const noexcept {
    return cer_.data();
  }

  const char* key() const noexcept {
    return key_.data();
  }

private:
  std::string ca_;
  std::string cer_;
  std::string key_;
};

}  // namespace net
