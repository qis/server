#pragma once
#include <app/config.hpp>

namespace net {

class server {
public:
  server(app::config config) : config_(std::move(config)), data_(config_.data.string()), html_(config_.html.string()) {}

  auto operator()() noexcept -> asio::awaitable<void>;

  constexpr const app::config& config() const noexcept
  {
    return config_;
  }

  const std::string_view data() const noexcept
  {
    return data_;
  }

  const std::string_view html() const noexcept
  {
    return html_;
  }

private:
  app::config config_;
  std::string data_;
  std::string html_;
};

}  // namespace net
