#include <net/socket.h>
#include <tls.h>
#include <vector>

namespace net {

socket::~socket() {
  close();
}

net::async_generator<std::string_view> socket::recv(std::size_t size) {
  std::vector<char> buffer;
  buffer.resize(size);
  while (true) {
    auto data = co_await recv(buffer.data(), buffer.size());
    if (data.empty()) {
      break;
    }
    co_yield data;
  }
  co_return;
}

std::string_view socket::alpn() noexcept {
  if (tls_) {
    if (const auto alpn = tls_conn_alpn_selected(tls_.get())) {
      return alpn;
    }
  }
  return {};
}

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const socket& socket) {
  formatter.writer().write("{:016X}", socket.value());
}

}  // namespace net
