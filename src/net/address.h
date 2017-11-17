#pragma once
#include <net/error.h>
#include <net/socket.h>
#include <memory>
#include <string>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

namespace net {

class address_error_category : public std::error_category {
public:
  const char* name() const noexcept override {
    return "address";
  }

  std::string message(int condition) const override {
#ifdef WIN32
    return ::gai_strerrorA(condition);
#else
    return ::gai_strerror(condition);
#endif
  }
};

inline const std::error_category& address_category() noexcept {
  static const address_error_category category;
  return category;
}

inline int to_int(family family) noexcept {
  switch (family) {
  case family::none: return 0;
  case family::ipv4: return AF_INET;
  case family::ipv6: return AF_INET6;
  }
  return static_cast<int>(family);
}

inline int to_int(type type) noexcept {
  switch (type) {
  case type::none: return 0;
  case type::tcp: return SOCK_STREAM;
  case type::udp: return SOCK_DGRAM;
  }
  return static_cast<int>(type);
}

class address {
public:
  address(const std::string& host, const std::string& port, net::type type, int flags) : info_(nullptr, freeaddrinfo) {
    struct addrinfo* info = nullptr;
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = to_int(type);
    hints.ai_flags = flags;
    if (const auto rv = ::getaddrinfo(host.data(), port.data(), &hints, &info)) {
      throw exception("get address info", rv, address_category());
    }
    info_.reset(info);
  }

  auto family() const noexcept {
    switch (info_->ai_family) {
    case AF_INET: return family::ipv4;
    case AF_INET6: return family::ipv6;
    }
    return static_cast<net::family>(info_->ai_family);
  }

  auto type() const noexcept {
    switch (info_->ai_socktype) {
    case SOCK_STREAM: return type::tcp;
    case SOCK_DGRAM: return type::udp;
    }
    return static_cast<net::type>(info_->ai_socktype);
  }

  auto addr() const noexcept {
    return info_->ai_addr;
  }

  auto addrlen() const noexcept {
    return info_->ai_addrlen;
  }

private:
  std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> info_;
};

}  // namespace net
