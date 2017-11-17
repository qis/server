#include <net/tls.h>
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

certificate::certificate(const std::string& filename) {
}

certificate::certificate(certificate&& other) {
}

certificate& certificate::operator=(certificate&& other) {
  return *this;
}

certificate::~certificate() {
}

}  // namespace net
