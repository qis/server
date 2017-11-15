#pragma once
#include <experimental/coroutine>
#include <cstddef>

#if NET_USE_IOCP
#include <windows.h>

namespace net {

class event final : public OVERLAPPED {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  event() noexcept : OVERLAPPED({}) {
  }

  event(event&& other) = delete;
  event& operator=(event&& other) = delete;

  event(const event& other) = delete;
  event& operator=(const event& other) = delete;

  ~event() = default;

  constexpr bool await_ready() noexcept {
    return ready_;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
  }

  constexpr auto await_resume() noexcept {
    return result_;
  }

  void operator()(DWORD size) noexcept {
    result_ = size;
    ready_ = true;
    if (handle_) {
      handle_.resume();
    }
  }

private:
  bool ready_ = false;
  DWORD result_ = 0;
  handle_type handle_ = nullptr;
};

}  // namespace net

#elif NET_USE_EPOLL
#include <sys/epoll.h>

#define NET_TLS_RECV EPOLLIN
#define NET_TLS_SEND EPOLLOUT

namespace net {

class event final : public epoll_event {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  event(int epoll, int socket, uint32_t filter) noexcept : epoll_event({}), epoll_(epoll), socket_(socket) {
    const auto ev = static_cast<struct ::epoll_event*>(this);
    ev->events = filter;
    ev->data.ptr = this;
  }

  event(event&& event) = delete;
  event& operator=(event&& other) = delete;

  event(const event& other) = delete;
  event& operator=(const event& other) = delete;

  ~event() = default;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
    ::epoll_ctl(epoll_, EPOLL_CTL_ADD, socket_, this);
  }

  constexpr void await_resume() noexcept {
  }

  void operator()() noexcept {
    ::epoll_ctl(epoll_, EPOLL_CTL_DEL, socket_, this);
    handle_.resume();
  }

private:
  handle_type handle_ = nullptr;
  int epoll_ = -1;
  int socket_ = -1;
};

}  // namespace net

#elif NET_USE_KQUEUE
#include <sys/event.h>

#define NET_TLS_RECV EVFILT_READ
#define NET_TLS_SEND EVFILT_WRITE

namespace net {

class event final : public kevent {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  event(int kqueue, int socket, short filter) noexcept : kevent({}), kqueue_(kqueue) {
    const auto ev = static_cast<kevent*>(this);
    const auto ident = static_cast<uintptr_t>(socket);
    EV_SET(ev, ident, filter, EV_ADD | EV_ONESHOT, 0, 0, this);
  }

  event(event&& event) = delete;
  event& operator=(event&& other) = delete;

  event(const event& other) = delete;
  event& operator=(const event& other) = delete;

  ~event() = default;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
    ::kevent(kqueue_, this, 1, nullptr, 0, nullptr);
  }

  constexpr void await_resume() noexcept {
  }

  void operator()() noexcept {
    handle_.resume();
  }

private:
  handle_type handle_ = nullptr;
  int kqueue_ = -1;
};

}  // namespace net

#endif
