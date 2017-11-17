#include <net/tls.h>
#include <net/error.h>
#include <net/handle.h>
#include <net/utility.h>
#include <tls.h>

namespace net {

void tls_config_deleter(::tls_config* ptr) noexcept {
  if (ptr) {
    ::tls_config_free(ptr);
  }
}

void tls_deleter(::tls* ptr) noexcept {
  if (ptr) {
    ::tls_close(ptr);
    ::tls_free(ptr);
  }
}

namespace {

const std::string_view g_cer_beg = "-----BEGIN CERTIFICATE-----";
const std::string_view g_cer_end = "-----END CERTIFICATE-----";
const std::string_view g_key_beg = "-----BEGIN RSA PRIVATE KEY-----";
const std::string_view g_key_end = "-----END RSA PRIVATE KEY-----";

}  // namespace

class certificate::impl {
public:
  impl(const std::string& filename) {
    if (const auto ec = file.open(filename)) {
      throw exception("tls open \"" + filename + "\"", ec);
    }
    data = file.data();

    // Get private key.
    const auto key_beg = data.find(g_key_beg);
    if (key_beg == data.npos) {
      throw exception("tls certificate file is missing the server key begin line");
    }
    const auto key_end = data.find(g_key_end, key_beg);
    if (key_end == data.npos) {
      throw exception("tls certificate file is missing the server key end line");
    }
    key = data.substr(key_beg, key_end - key_beg + g_key_end.size());

    // Get server certificate.
    const auto cer_beg = data.find(g_cer_beg, key_end);
    if (cer_beg == data.npos) {
      throw exception("tls certificate file is missing the server certificate begin line");
    }
    const auto cer_end = data.find(g_cer_end, cer_beg);
    if (cer_end == data.npos) {
      throw exception("tls certificate file is missing the server certificate end line");
    }
    cer = data.substr(cer_beg, cer_end - cer_beg + g_cer_end.size());

    // Get ca certificate.
    const auto ca_beg = data.find(g_cer_beg, cer_end);
    if (ca_beg == data.npos) {
      throw exception("tls certificate file is missing the ca certificate begin line");
    }
    ca = data.substr(ca_beg);
  }

  std::string_view ca;
  std::string_view cer;
  std::string_view key;
  std::string_view data;
  net::file_view file;
};

certificate::certificate() noexcept {
}

certificate::~certificate() {
  reset();
}

void certificate::load(const std::string& filename) {
  impl_ = std::make_unique<impl>(filename);
}

void certificate::reset() noexcept {
  impl_.reset();
}

std::string_view certificate::ca() const noexcept {
  return impl_ ? impl_->ca : std::string_view{};
}

std::string_view certificate::cer() const noexcept {
  return impl_ ? impl_->cer : std::string_view{};
}

std::string_view certificate::key() const noexcept {
  return impl_ ? impl_->key : std::string_view{};
}

}  // namespace net
