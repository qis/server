#pragma once
#include <net/http.h>

namespace net::http {

class parser_v2 {
public:
  std::size_t parse(const char* data, std::size_t size) {
    return size;
  }

  net::http::request& get() noexcept {
    return request_;
  }

  bool ready() const noexcept {
    return ready_;
  }

private:
  bool ready_ = false;
  net::http::request request_;
};

}  // namespace net::http
