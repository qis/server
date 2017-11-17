#pragma once
#include <net/async.h>
#include <net/socket.h>
#include <fmt/format.h>
#include <unordered_map>
#include <string>
#include <string_view>

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

class body {
public:
  body() = default;

  body(body&& other) = delete;
  body& operator=(body&& other) = delete;

  body(const body& other) = delete;
  body& operator=(const body& other) = delete;

  ~body() = default;

  net::async_generator<std::string_view> recv();

private:
  void resume(std::string_view data);

  net::single_consumer_event event_;
  std::string_view data_;

  friend class parser_v1;
  friend class parser_v2;
};

class request {
public:
  request(body& body);

  method method = method::none;
  std::string path;
  version version;
  bool closed = false;
  bool keep_alive = false;
  std::size_t content_length = 0;
  std::unordered_multimap<header, std::string> headers;

  net::async_generator<std::string_view> recv();
  void reset();

private:
  std::reference_wrapper<body> body_;
};

net::async_generator<request> recv(net::socket& socket, std::size_t size = 4096);

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const method& method);
void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const version& version);
void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const header& header);
void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const request& request);

}  // namespace net::http
