#include "server.hpp"
#include <net/session.hpp>
#include <version.h>

namespace net {

auto server::operator()() noexcept -> asio::awaitable<void>
{
  try {
    auto executor = co_await asio::this_coro::executor;
    auto resolver = asio::ip::tcp::resolver{ executor };
    auto endpoint = resolver.resolve(config_.server.address, config_.server.service)->endpoint();
    auto acceptor = asio::ip::tcp::acceptor{ asio::make_strand(executor), endpoint, true };
    LOGI("[:SERVER:] Version: {}", PROJECT_VERSION);
    if (config_.server.proxied) {
      LOGD("[:SERVER:] {}:{}", endpoint.address().to_string(), endpoint.port());
    } else {
      LOGD("[:SERVER:] http://{}:{}", endpoint.address().to_string(), endpoint.port());
    }
    while (true) {
      boost::system::error_code ec;
      auto socket = co_await acceptor.async_accept(asio::redirect_error(asio::use_awaitable, ec));
      if (ec) {
        if (ec == asio::error::connection_reset) {
          LOGT("[:SERVER:] {} ({})", ec.message(), ec.value());
        } else {
          LOGE("[:SERVER:] {} ({})", ec.message(), ec.value());
        }
        continue;
      }
      asio::co_spawn(executor, net::session(*this, std::move(socket)), asio::detached);
    }
  }
  catch (const boost::system::system_error& e) {
    LOGC("[:SERVER:] {}: {} ({})", e.code().category().name(), e.what(), e.code().value());
  }
  catch (const std::system_error& e) {
    LOGC("[:SERVER:] {}: {} ({})", e.code().category().name(), e.what(), e.code().value());
  }
  catch (const std::exception& e) {
    LOGC("[:SERVER:] {}", e.what());
  }
  co_return;
}

}  // namespace net
