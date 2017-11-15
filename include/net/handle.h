#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <cstdint>

namespace net {

class handle {
public:
#ifdef WIN32
  using handle_type = std::intptr_t;
  constexpr static handle_type invalid_handle_value = 0;
#else
  using handle_type = int;
  constexpr static handle_type invalid_handle_value = -1;
#endif

  constexpr handle() noexcept = default;

#ifdef WIN32
  template <typename T>
  constexpr explicit handle(T handle) noexcept {
    if constexpr (std::is_pointer_v<T>) {
      handle_ = reinterpret_cast<handle_type>(handle);
    } else {
      handle_ = static_cast<handle_type>(handle);
    }
  }
#else
  constexpr explicit handle(handle_type handle) noexcept : handle_(handle) {
  }
#endif

  handle(handle&& other) noexcept : handle_(other.release()) {
  }

  handle& operator=(handle&& other) noexcept {
    if (this != std::addressof(other)) {
      reset(other.release());
    }
    return *this;
  }

  handle(const handle& other) = delete;
  handle& operator=(const handle& other) = delete;

  virtual ~handle() = default;

  constexpr bool operator==(const handle& other) const noexcept {
    return handle_ == other.handle_;
  }

  constexpr bool operator!=(const handle& other) const noexcept {
    return handle_ != other.handle_;
  }

  constexpr bool valid() const noexcept {
    return handle_ != invalid_handle_value;
  }

  constexpr explicit operator bool() const noexcept {
    return valid();
  }

#ifdef WIN32
  template <typename T>
  constexpr T as() const noexcept {
    if constexpr (std::is_pointer_v<T>) {
      return reinterpret_cast<T>(handle_);
    } else {
      return static_cast<T>(handle_);
    }
  }
#endif

  constexpr handle_type value() const noexcept {
    return handle_;
  }

  void reset(handle_type handle = invalid_handle_value) {
    close();
    handle_ = handle;
  }

  handle_type release() noexcept {
    return std::exchange(handle_, invalid_handle_value);
  }

  virtual std::error_code close() noexcept = 0;

protected:
  handle_type handle_ = invalid_handle_value;
};

}  // namespace net
