#include <net/tls.h>
#include <net/error.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <tls.h>
#include <numeric>
#include <tuple>

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
#ifdef WIN32
  using handle = std::unique_ptr<std::remove_pointer_t<HANDLE>, void(*)(HANDLE handle)>;
#endif

  impl(const std::string& filename) {
#ifdef WIN32
    std::wstring path;
    path.resize(MultiByteToWideChar(CP_UTF8, 0, filename.data(), -1, nullptr, 0) + 1);
    path.resize(MultiByteToWideChar(CP_UTF8, 0, filename.data(), -1, path.data(), static_cast<int>(path.size())));

    constexpr auto access = GENERIC_READ;
    constexpr auto share = FILE_SHARE_READ;
    constexpr auto disposition = OPEN_EXISTING;
    constexpr auto buffering = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING;
    handle file(CreateFile(path.data(), access, share, nullptr, disposition, buffering, nullptr), file_deleter);
    if (file.get() == INVALID_HANDLE_VALUE) {
      throw exception("tls read certificate file", GetLastError());
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file.get(), &size)) {
      throw exception("tls get certificate file size", GetLastError());
    }

    constexpr auto protect = PAGE_READONLY | SEC_NOCACHE | SEC_COMMIT;
    const auto hi = static_cast<DWORD>(size.HighPart);
    const auto lo = static_cast<DWORD>(size.LowPart);
    handle mapping(CreateFileMapping(file.get(), nullptr, protect, hi, lo, path.data()), mapping_deleter);
    if (!mapping) {
      throw exception("tls create certificate file mapping", GetLastError());
    }

    const auto view = MapViewOfFileEx(mapping.get(), FILE_MAP_READ, 0, 0, 0, nullptr);
    if (!view) {
      throw exception("tls map certificate file", GetLastError());
    }
    data = { reinterpret_cast<const char*>(view), static_cast<std::size_t>(size.QuadPart) };
#endif

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

#ifdef WIN32
    mapping_ = std::move(mapping);
    file_ = std::move(file);
#endif
  }

  std::string_view ca;
  std::string_view cer;
  std::string_view key;
  std::string_view data;

private:
#ifdef WIN32
  static void file_deleter(HANDLE handle) noexcept {
    CloseHandle(handle);
  }

  static void mapping_deleter(HANDLE handle) noexcept {
    UnmapViewOfFile(handle);
    CloseHandle(handle);
  }

  handle file_ = { INVALID_HANDLE_VALUE, file_deleter };
  handle mapping_ = { nullptr, mapping_deleter };
#endif
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
