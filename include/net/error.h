#pragma once
#include <fmt/format.h>
#include <exception>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>

namespace net {

class exception : public std::domain_error {
public:
  exception(const std::string& message) : std::domain_error(message) {
  }

  exception(const std::string& message, const char* description) : std::domain_error(format(message, description)) {
  }

  exception(const std::string& message, const std::string& description) :
    std::domain_error(message + ": " + description) {
  }

  template <typename T>
  exception(const std::string& message, T code) : std::domain_error(format(message, code)) {
  }

  template <typename T>
  exception(const std::string& message, T code, const std::error_category& category) :
    std::domain_error(format(message, code, category)) {
  }

private:
  static std::string format(const std::string& message, const char* description) {
    if (description) {
      return message + ": " + description;
    }
    return message;
  }

  template <typename T>
  static std::string format(const std::string& message, T code) {
    if constexpr (std::is_same_v<T, std::error_code>) {
      return format(message, code.value(), code.category());
    } else {
      return format(message, code, std::system_category());
    }
  }

  template <typename T>
  static std::string format(const std::string& message, T code, const std::error_category& category) {
    const auto name = category.name();
    const auto text = category.message(static_cast<int>(code));
    if constexpr (std::is_unsigned_v<T>) {
      return fmt::format("[{}:{}] {}: {}", name, static_cast<std::uintptr_t>(code), message, text);
    } else {
      return fmt::format("[{}:{}] {}: {}", name, static_cast<std::intptr_t>(code), message, text);
    }
  }
};

}  // namespace net
