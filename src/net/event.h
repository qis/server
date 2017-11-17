#pragma once
#include <experimental/coroutine>
#include <cstddef>

#if NET_USE_IOCP
#include <windows.h>
#elif NET_USE_EPOLL
#include <sys/epoll.h>
#define NET_TLS_RECV EPOLLIN
#define NET_TLS_SEND EPOLLOUT
#elif NET_USE_KQUEUE
#include <sys/event.h>
#define NET_TLS_RECV EVFILT_READ
#define NET_TLS_SEND EVFILT_WRITE
#endif

namespace net {

#if NET_USE_IOCP
using event_base = OVERLAPPED;
#elif NET_USE_EPOLL
using event_base = epoll_event;
#elif NET_USE_KQUEUE
using event_base = struct kevent;
#endif

class event final : public event_base {
public:
  using handle_type = std::experimental::coroutine_handle<>;

#if NET_USE_IOCP
  event() noexcept : event_base({}) {
  }
#elif NET_USE_EPOLL
  event(int epoll, int socket, uint32_t filter) noexcept : event_base({}), epoll_(epoll), socket_(socket) {
    events = filter;
    data.fd = socket;
    data.ptr = this;
  }
#elif NET_USE_KQUEUE
  event(int kqueue, int socket, short filter) noexcept : event_base({}), kqueue_(kqueue) {
    const auto ev = static_cast<kevent*>(this);
    const auto ident = static_cast<uintptr_t>(socket);
    EV_SET(ev, ident, filter, EV_ADD | EV_ONESHOT, 0, 0, this);
  }
#endif

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
#if NET_USE_EPOLL
    ::epoll_ctl(epoll_, EPOLL_CTL_ADD, socket_, this);
#elif NET_USE_KQUEUE
    ::kevent(kqueue_, this, 1, nullptr, 0, nullptr);
#endif
  }

  constexpr auto await_resume() noexcept {
    return size_;
  }

  void operator()(std::size_t size) noexcept {
#if NET_USE_EPOLL
    ::epoll_ctl(epoll_, EPOLL_CTL_DEL, socket_, this);
#endif
    size_ = size;
    ready_ = true;
    if (handle_) {
      handle_.resume();
    }
  }

private:
  bool ready_ = false;
  std::size_t size_ = 0;
  handle_type handle_ = nullptr;

#if NET_USE_EPOLL
  int epoll_ = -1;
  int socket_ = -1;
#elif NET_USE_KQUEUE
  int kqueue_ = -1;
#endif
};

}  // namespace net
