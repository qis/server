#pragma once
#include <net/server.h>
#include <fmt/format.h>
#include <unordered_map>

namespace net::http {

enum class method {
  none,
  get,
  head,
  post,
  put,
  del,
};

struct version {
  unsigned short major = 0;
  unsigned short minor = 0;
};

constexpr bool operator==(const version& lhs, const version& rhs) noexcept {
  return lhs.major == rhs.major && lhs.minor == rhs.minor;
}

constexpr bool operator!=(const version& lhs, const version& rhs) noexcept {
  return !(lhs == rhs);
}

constexpr bool operator<(const version& lhs, const version& rhs) noexcept {
  return lhs.major < rhs.major || (lhs.major == rhs.major && lhs.minor < rhs.minor);
}

constexpr bool operator>(const version& lhs, const version& rhs) noexcept {
  return lhs.major > rhs.major || (lhs.major == rhs.major && lhs.minor > rhs.minor);
}

constexpr bool operator<=(const version& lhs, const version& rhs) noexcept {
  return lhs == rhs || lhs < rhs;
}

constexpr bool operator>=(const version& lhs, const version& rhs) noexcept {
  return lhs == rhs || lhs > rhs;
}

enum class header {
  accept_encoding,
  cache_control,
  content_type,
  cookie,
  if_modified_since,
  range,
};

class resume {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  resume() noexcept = default;

  resume(resume&& other) = delete;
  resume& operator=(resume&& other) = delete;

  resume(const resume& other) = delete;
  resume& operator=(const resume& other) = delete;

  //~resume() {
  //  if (handle_) {
  //    handle_.destroy();
  //  }
  //}

  constexpr bool await_ready() noexcept {
    return ready_;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
  }

  auto await_resume() noexcept {
    ready_ = false;
    return std::exchange(data_, {});
  }

  void operator()(std::string_view data) noexcept {
    data_ = data;
    ready_ = true;
    if (auto handle = std::exchange(handle_, nullptr)) {
      handle.resume();
    }
  }

private:
  bool ready_ = false;
  std::string_view data_ = 0;
  handle_type handle_ = nullptr;
};

class request {
public:
  request() = default;

  request(request&& other) = delete;
  request& operator=(request&& other) = delete;

  request(const request& other) = delete;
  request& operator=(const request& other) = delete;

  ~request() = default;

  method method = method::none;
  std::string path;
  version version;
  std::unordered_multimap<header, std::string> headers;
  std::size_t content_length = 0;
  bool keep_alive = false;
  bool closed = false;
  resume resume;

  net::async_generator<std::string_view> body() noexcept;

  void reset();
};

net::async_generator<request> recv(net::connection& socket, std::size_t size = 4096);

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const method& method);
void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const version& version);
void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const header& header);
void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const request& request);

}  // namespace net::http
