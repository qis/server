#include <net/http.h>
#include <net/http/parser_v1.h>
#include <net/http/parser_v2.h>
#include <fmt/format.h>

namespace net::http {

net::async_generator<request> recv(net::connection& socket, std::size_t size) {
  std::vector<char> buffer;
  buffer.resize(size);
  if (!co_await socket->handshake()) {
    co_return;
  }

  //if (const auto alpn = socket->alpn(); alpn && std::string_view(alpn) == "h2") {
  //}

  parser_v1 parser;
  while (true) {
    const auto& data = co_await socket->recv(buffer.data(), buffer.size());
    if (data.empty()) {
      parser.parse(nullptr, 0);
      if (parser.ready()) {
        parser.get().closed = true;
        co_yield parser.get();
      }
      break;
    }
    auto buffer_data = data.data();
    auto buffer_size = data.size();
    while (buffer_size) {
      const auto bytes = parser.parse(buffer_data, buffer_size);
      if (parser.ready()) {
        co_yield parser.get();
      }
      buffer_data += bytes;
      buffer_size -= bytes;
    }
  }
  co_return;
}

net::async_generator<std::string_view> request::recv() noexcept {
  while (true) {
    co_await event_;
    event_.reset();
    if (data_.empty()) {
      break;
    }
    co_yield data_;
  }
  co_return;
}

void request::resume(std::string_view data) {
  data_ = data;
  event_.set();
}

void request::reset() {
  method = method::none;
  path.clear();
  version = {};
  headers.clear();
  content_length = 0;
  keep_alive = false;
  closed = false;
  event_.reset();
}

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const method& method) {
  switch (method) {
  case method::get: formatter.writer().write("GET"); break;
  case method::head: formatter.writer().write("HEAD"); break;
  case method::post: formatter.writer().write("POST"); break;
  case method::put: formatter.writer().write("PUT"); break;
  case method::del: formatter.writer().write("DELETE"); break;
  default: formatter.writer().write("NONE"); break;
  }
}

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const version& version) {
  if (version == http::version{ 2, 0 }) {
    formatter.writer().write("HTTP/2");
  } else {
    formatter.writer().write("HTTP/{}.{}", version.major, version.minor);
  }
}

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const header& header) {
  switch (header) {
  case header::accept_encoding: formatter.writer().write("Accept-Encoding"); break;
  case header::cache_control: formatter.writer().write("Cache-Control"); break;
  case header::content_type: formatter.writer().write("Content-Type"); break;
  case header::cookie: formatter.writer().write("Cookie"); break;
  case header::if_modified_since: formatter.writer().write("If-Modified-Since"); break;
  case header::range: formatter.writer().write("Range"); break;
  }
}

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const request& request) {
  formatter.writer().write("{} {} {}", request.method, request.path, request.version);
  for (const auto& e : request.headers) {
    formatter.writer().write("\n{}: {}", e.first, e.second);
  }
  if (request.content_length) {
    formatter.writer().write("\nContent-Length: {}", request.content_length);
  }
  formatter.writer().write("\nConnection: {}", request.keep_alive ? "keep-alive" : "close");
}

}  // namespace net::http
