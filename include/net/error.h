#pragma once
#include <exception>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>

namespace net {

class exception : public std::domain_error {
public:
  exception(const std::string& message) noexcept : std::domain_error(message) {
  }

  exception(const std::string& message, const char* description) noexcept :
    std::domain_error(format(message, description)) {
  }

  template <typename T>
  exception(const std::string& message, T code, const std::error_category& category = std::system_category()) noexcept :
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
  static std::string format(const std::string& message, T code, const std::error_category& category) {
    if constexpr (std::is_same_v<T, std::error_code>) {
      return '[' + std::to_string(code.value()) + "] " + message + ": " + code.category().message(code.value());
    } else if constexpr (std::is_unsigned_v<T>) {
      return '[' + std::to_string(static_cast<std::uintptr_t>(code)) + "] " + message + ": " +
        category.message(static_cast<int>(code));
    } else {
      return '[' + std::to_string(static_cast<std::intptr_t>(code)) + "] " + message + ": " +
        category.message(static_cast<int>(code));
    }
  }
};

}  // namespace net
