#pragma once

#ifdef __INTELLISENSE__
#define ASIO_THIS_CORO_HPP
#define ASIO_USE_AWAITABLE_HPP
#define ASIO_AWAITABLE_HPP
#include <experimental/coroutine>

// ================================================================================================
// boost/asio/this_coro.hpp
// ================================================================================================
#include <boost/asio/executor.hpp>

namespace boost::asio::this_coro {

struct executor_t {
  executor_t();
  bool await_ready() const;
  bool await_suspend(std::experimental::coroutine_handle<> handle) const;
  asio::executor await_resume() const;
};

constexpr executor_t executor;

}  // namespace boost::asio::this_coro

// ================================================================================================
// boost/asio/use_awaitable.hpp
// ================================================================================================

namespace boost::asio {

template <typename Executor = executor>
struct use_awaitable_t {
  constexpr use_awaitable_t() {}
};

constexpr use_awaitable_t<> use_awaitable;

}  // namespace boost::asio

// ================================================================================================
// asio/awaitable.hpp
// ================================================================================================

namespace boost::asio {
namespace detail {

using std::experimental::coroutine_handle;
using std::experimental::suspend_always;

template <typename>
class awaitable_thread;

template <typename, typename>
class awaitable_frame;

}  // namespace detail

template <typename T, typename Executor = executor>
class awaitable {
public:
  using value_type = T;
  using executor_type = Executor;

  awaitable();
  auto await_ready() const -> bool;
  void await_suspend(std::experimental::coroutine_handle<> handle);
  auto await_resume() -> T;
};

namespace detail {

template <typename Executor>
class awaitable_frame_base {
public:
  auto initial_suspend() -> suspend_always;
  auto final_suspend() -> suspend_always;
  void unhandled_exception();
  template <typename T>
  auto await_transform(awaitable<T, Executor> a) const -> awaitable<T, Executor>;
  auto await_transform(this_coro::executor_t) -> awaitable<executor>;
};

template <typename T, typename Executor>
class awaitable_frame : public awaitable_frame_base<Executor> {
public:
  auto get_return_object() -> awaitable<T, Executor>;
  template <typename... Args>
  void return_values(Args&&... args);
  auto get() -> T;
};

template <typename Executor>
class awaitable_frame<void, Executor> : public awaitable_frame_base<Executor> {
public:
  auto get_return_object() -> awaitable<void, Executor>;
  void return_void();
};

}  // namespace detail
}  // namespace boost::asio

namespace std::experimental {

template <typename T, typename Executor, typename... Args>
struct coroutine_traits<boost::asio::awaitable<T, Executor>, Args...> {
  using promise_type = asio::detail::awaitable_frame<T, Executor>;
};

}  // namespace std::experimental

// ================================================================================================
// boost/asio/awaitable.hpp
// ================================================================================================
#include <boost/asio/async_result.hpp>

namespace boost::asio {
namespace detail {

template <typename Executor, typename T>
struct awaitable_handler_base {
  using result_type = void;
  using awaitable_type = awaitable<T>;

  template <typename... Args>
  void operator()(Args&&... args);
};

template <typename, typename...>
struct awaitable_handler;

template <typename Executor>
struct awaitable_handler<Executor, void> : awaitable_handler_base<Executor, void> {};

template <typename Executor>
struct awaitable_handler<Executor, boost::system::error_code> : awaitable_handler_base<Executor, void> {};

template <typename Executor>
struct awaitable_handler<Executor, std::exception_ptr> : awaitable_handler_base<Executor, void> {};

template <typename Executor, typename T>
struct awaitable_handler<Executor, T> : awaitable_handler_base<Executor, T> {};

template <typename Executor, typename T>
struct awaitable_handler<Executor, boost::system::error_code, T> : awaitable_handler_base<Executor, T> {};

template <typename Executor, typename T>
struct awaitable_handler<Executor, std::exception_ptr, T> : awaitable_handler_base<Executor, T> {};

template <typename Executor, typename... Ts>
struct awaitable_handler : awaitable_handler_base<Executor, std::tuple<Ts...>> {};

template <typename Executor, typename... Ts>
struct awaitable_handler<Executor, boost::system::error_code, Ts...> :
  awaitable_handler_base<Executor, std::tuple<Ts...>> {};

template <typename Executor, typename... Ts>
struct awaitable_handler<Executor, std::exception_ptr, Ts...> : awaitable_handler_base<Executor, std::tuple<Ts...>> {};

}  // namespace detail

template <typename Executor, typename R, typename... Args>
struct async_result<use_awaitable_t<Executor>, R(Args...)> {
  using handler_type = typename detail::awaitable_handler<Executor, typename decay<Args>::type...>;
  using return_type = typename handler_type::awaitable_type;

  template <typename Initiation, typename... InitArgs>
  static return_type initiate(Initiation initiation, use_awaitable_t<Executor>, InitArgs... args);
};

}  // namespace boost::asio

#endif  // __INTELLISENSE__

// clang-format off

// ============================================================================
// asio
// ============================================================================

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/this_coro.hpp>

namespace asio = boost::asio;

// ============================================================================
// beast
// ============================================================================

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

namespace beast = boost::beast;
namespace http = beast::http;

// ============================================================================
// json
// ============================================================================

#include <boost/json.hpp>

namespace json = boost::json;

// ============================================================================
// fmt
// ============================================================================

#include <fmt/color.h>
#include <fmt/format.h>

// ============================================================================
// spdlog
// ============================================================================

#ifndef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
//#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_CRITICAL  // for benchmarks
#endif

#include <spdlog/spdlog.h>

#define LOGT SPDLOG_TRACE
#define LOGD SPDLOG_DEBUG
#define LOGI SPDLOG_INFO
#define LOGW SPDLOG_WARN
#define LOGE SPDLOG_ERROR
#define LOGC SPDLOG_CRITICAL

// clang-format on

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <cstdint>
