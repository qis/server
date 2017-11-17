#include <net/socket.h>
#include <vector>

namespace net {

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

void format_arg(fmt::BasicFormatter<char>& formatter, const char*& format, const socket& socket) {
  formatter.writer().write("{:016X}", socket.value());
}

}  // namespace net
