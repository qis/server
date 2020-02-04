#pragma once
#include <app/config.hpp>

namespace net {

class server {
public:
  server(app::config config) : config_(std::move(config)), path_((config_.path / "html").string()) {}

  auto operator()() noexcept -> asio::awaitable<void>;

  constexpr const app::config& config() const noexcept
  {
    return config_;
  }

  const std::string_view path() const noexcept
  {
    return path_;
  }

private:
  app::config config_;
  std::string path_;
};

}  // namespace net
