#pragma once
#include <net/http.h>
#include <net/http/parser.h>
#include <algorithm>
#include <cstdio>

namespace net::http {

class parser_v1 final : public http_parser {
public:
  parser_v1() noexcept : http_parser({}) {
    http_parser_init(this, HTTP_REQUEST);
    settings_.on_message_begin = [](http_parser* parser) -> int {
      return static_cast<parser_v1*>(parser)->on_message_begin();
    };
    settings_.on_url = [](http_parser* parser, const char* data, size_t size) -> int {
      return static_cast<parser_v1*>(parser)->on_url(data, size);
    };
    settings_.on_header_field = [](http_parser* parser, const char* data, size_t size) -> int {
      return static_cast<parser_v1*>(parser)->on_header_field(data, size);
    };
    settings_.on_header_value = [](http_parser* parser, const char* data, size_t size) -> int {
      return static_cast<parser_v1*>(parser)->on_header_value(data, size);
    };
    settings_.on_headers_complete = [](http_parser* parser) -> int {
      return static_cast<parser_v1*>(parser)->on_headers_complete();
    };
    settings_.on_body = [](http_parser* parser, const char* data, size_t size) -> int {
      return static_cast<parser_v1*>(parser)->on_body(data, size);
    };
    settings_.on_message_complete = [](http_parser* parser) -> int {
      return static_cast<parser_v1*>(parser)->on_message_complete();
    };
    field_.reserve(256);
    value_.reserve(256);
  }

  std::size_t parse(const char* data, std::size_t size) {
    return http_parser_execute(this, &settings_, data, size);
  }

  net::http::request& get() noexcept {
    return request_;
  }

  bool ready() const noexcept {
    return ready_;
  }

private:
  int on_message_begin() {
    ready_ = false;
    query_ = false;
    field_.clear();
    value_.clear();
    request_.reset();
    return 0;
  }

  int on_url(const char* data, size_t size) {
    if (!query_) {
      for (size_t i = 0; i < size; i++) {
        if (data[i] == '?') {
          request_.path.append(data, i);
          query_ = true;
          return 0;
        }
      }
      request_.path.append(data, size);
    }
    return 0;
  }

  int on_header_field(const char* data, size_t size) {
    if (!value_.empty()) {
      parse_header();
    }
    field_.append(data, size);
    return 0;
  }

  int on_header_value(const char* data, size_t size) {
    if (!size) {
      parse_header();
    } else {
      value_.append(data, size);
    }
    return 0;
  }

  int on_headers_complete() {
    switch (method) {
    case HTTP_GET: request_.method = method::get; break;
    case HTTP_HEAD: request_.method = method::head; break;
    case HTTP_POST: request_.method = method::post; break;
    case HTTP_PUT: request_.method = method::put; break;
    case HTTP_DELETE: request_.method = method::del; break;
    default: break;
    }
    if (!value_.empty()) {
      parse_header();
    }
    request_.version = { http_major, http_minor };
    if (http_should_keep_alive(this)) {
      request_.keep_alive = true;
    }
    if (flags & F_CONTENTLENGTH) {
      request_.content_length = static_cast<std::size_t>(content_length);
    }
    return 1;
  }

  int on_body(const char* data, size_t size) {
    if (size) {
      request_.resume({ data, size });
    }
    return 0;
  }

  int on_message_complete() {
    request_.resume({});
    return 0;
  }

  void parse_header() {
    if (!value_.empty()) {
      bool valid = true;
      std::transform(field_.begin(), field_.end(), field_.begin(), [&valid](char c) {
        if (valid) {
          if (c > '@' && c < '[') {
            return static_cast<char>(c + 32);
          } else if (c < ' ' || c > '~') {
            valid = false;
          }
        }
        return c;
      });
      if (valid) {
        if (field_ == "accept-encoding") {
          parse_header(header::accept_encoding);
        } else if (field_ == "cache-control") {
          parse_header(header::cache_control);
        } else if (field_ == "content-type") {
          parse_header(header::content_type);
        } else if (field_ == "cookie") {
          parse_header(header::cookie);
        } else if (field_ == "if-modified-since") {
          parse_header(header::if_modified_since);
        } else if (field_ == "range") {
          parse_header(header::range);
        }
      }
      value_.clear();
    }
    field_.clear();
  }

  void parse_header(header header) {
    request_.headers.emplace(header, std::move(value_));
  }

  bool ready_ = false;
  bool query_ = false;
  std::string field_;
  std::string value_;
  net::http::request request_;
  http_parser_settings settings_ = {};
};

}  // namespace net::http
