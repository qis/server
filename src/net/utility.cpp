#include <net/utility.h>
#include <net/error.h>
#include <mutex>
#include <unordered_map>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#include <fmt/format.h>

namespace net {
namespace {

std::unordered_map<int, std::function<void(int)>> g_signal_handlers;
std::mutex g_signal_handlers_mutex;

void signal_callback(int signum) {
  std::function<void(int)> handler;
  {
    std::lock_guard<std::mutex> lock(g_signal_handlers_mutex);
    auto it = g_signal_handlers.find(signum);
    if (it != g_signal_handlers.end()) {
      handler = it->second;
    }
  }
  if (handler) {
    handler(signum);
  }
}

}  // namespace

void signal(int signum, std::function<void(int)> handler) {
  std::lock_guard<std::mutex> lock(g_signal_handlers_mutex);
  if (handler) {
    std::signal(signum, signal_callback);
  } else {
    std::signal(signum, SIG_IGN);
  }
  g_signal_handlers[signum] = std::move(handler);
}

void drop(const std::string& user) {
#ifndef WIN32
  if (getuid() == 0) {
    const auto pw = getpwnam(user.data());
    setgid(pw->pw_gid);
    setuid(pw->pw_uid);
  }
#endif
}

file_view::file_view() noexcept {
}

file_view::file_view(file_view&& other) noexcept : handle(other.release()) {
  fmt::print("move ctor\n");
  data_ = std::exchange(other.data_, nullptr);
  size_ = std::exchange(other.size_, 0);
#ifdef WIN32
  mapping_ = std::exchange(other.mapping_, nullptr);
#endif
}

file_view& file_view::operator=(file_view&& other) noexcept {
  fmt::print("move op\n");
  data_ = std::exchange(other.data_, nullptr);
  size_ = std::exchange(other.size_, 0);
#ifdef WIN32
  mapping_ = std::exchange(other.mapping_, nullptr);
#endif
  handle_ = other.release();
  return *this;
}

file_view::~file_view() {
  close();
}

file_view::operator bool() const noexcept {
  return data_ != nullptr;
}

std::error_code file_view::open(const std::string& filename) noexcept {
  close();
#ifdef WIN32
  std::wstring path;
  path.resize(MultiByteToWideChar(CP_UTF8, 0, filename.data(), -1, nullptr, 0) + 1);
  path.resize(MultiByteToWideChar(CP_UTF8, 0, filename.data(), -1, path.data(), static_cast<int>(path.size())));
  constexpr auto access = GENERIC_READ;
  constexpr auto share = FILE_SHARE_READ;
  constexpr auto disposition = OPEN_EXISTING;
  constexpr auto buffering = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING;
  reset(CreateFile(path.data(), access, share, nullptr, disposition, buffering, nullptr));
  if (as<HANDLE>() == INVALID_HANDLE_VALUE) {
    return { static_cast<int>(GetLastError()), std::system_category() };
  }
  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(as<HANDLE>(), &size)) {
    return { static_cast<int>(GetLastError()), std::system_category() };
  }
  size_ = static_cast<std::size_t>(size.QuadPart);
  constexpr auto protect = PAGE_READONLY | SEC_NOCACHE | SEC_COMMIT;
  const auto hi = static_cast<DWORD>(size.HighPart);
  const auto lo = static_cast<DWORD>(size.LowPart);
  mapping_ = CreateFileMapping(as<HANDLE>(), nullptr, protect, hi, lo, path.data());
  if (!mapping_) {
    return { static_cast<int>(GetLastError()), std::system_category() };
  }
  data_ = MapViewOfFileEx(mapping_, FILE_MAP_READ, 0, 0, 0, nullptr);
  if (!data_) {
    return { static_cast<int>(GetLastError()), std::system_category() };
  }
#else
  reset(::open(filename.data(), O_RDONLY));
  if (!valid()) {
    return { errno, std::system_category() };
  }
  struct stat st = {};
  if (::fstat(value(), &st) < 0) {
    return { errno, std::system_category() };
  }
  size_ = static_cast<std::size_t>(st.st_size);
  data_ = ::mmap(nullptr, size_, PROT_READ, MAP_SHARED, value(), 0);
  if (!data_) {
    return { errno, std::system_category() };
  }
#endif
  return {};
}

std::string_view file_view::data() const noexcept {
  if (data_ && size_) {
    return { reinterpret_cast<const char*>(data_), size_ };
  }
  return {};
}

std::size_t file_view::size() const noexcept {
  return size_;
}

std::error_code file_view::close() noexcept {
#ifdef WIN32
  if (mapping_) {
    UnmapViewOfFile(mapping_);
    CloseHandle(mapping_);
    mapping_ = nullptr;
  }
  if (as<HANDLE>() != INVALID_HANDLE_VALUE) {
    if (!CloseHandle(as<HANDLE>())) {
      return { errno, std::system_category() };
    }
    handle_ = invalid_handle_value;
  }
#else
  if (data_) {
    ::munmap(data_, size_);
    data_ = nullptr;
  }
  size_ = 0;
  if (handle_ != invalid_handle_value) {
    if (::close(handle_) < 0) {
      return { errno, std::system_category() };
    }
  }
#endif
  return {};
}

}  // namespace net
