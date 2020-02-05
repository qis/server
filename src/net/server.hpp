#pragma once
#include <app/config.hpp>

namespace net {

class server {
public:
  server(app::config config, const std::filesystem::path& html, const std::filesystem::path& data) :
    config_(std::move(config)), html_(html.string()), data_(data.string())
  {}

  auto operator()() noexcept -> asio::awaitable<void>;

  constexpr const app::config& config() const noexcept
  {
    return config_;
  }

  const std::string_view html() const noexcept
  {
    return html_;
  }

  const std::string_view data() const noexcept
  {
    return data_;
  }

private:
  app::config config_;
  std::string html_;
  std::string data_;
};

}  // namespace net
