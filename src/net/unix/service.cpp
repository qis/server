#include <net/service.h>
#include <net/event.h>
#ifdef __linux__
#include <sched.h>
#else
#include <sys/param.h>
#include <sys/cpuset.h>
#endif
#include <unistd.h>
#include <array>
#include <cerrno>

namespace net {

service::service() {
#ifdef NET_USE_EPOLL
  const auto handle = epoll_create1(0);
#else
  const auto handle = ::kqueue();
#endif
  if (handle < 0) {
    throw exception("create service", errno);
  } else {
    reset(handle);
  }
}

service::~service() {
  close();
}

void service::run(int processor) {
  if (!valid()) {
    throw exception("run service", std::errc::bad_file_descriptor);
  }
  if (processor >= 0) {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(processor, &set);
    if (sched_setaffinity(getpid(), sizeof(set), &set) < 0) {
      throw exception("set thread affinity", errno);
    }
#else
    cpuset_t set;
    CPU_ZERO(&set);
    CPU_SET(processor, &set);
    if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(set), &set) < 0) {
      throw exception("set thread affinity", errno);
    }
#endif
  }
#ifdef NET_USE_EPOLL
  std::array<epoll_event, 1024> events;
#else
  std::array<struct ::kevent, 1024> events;
#endif
  const auto service_data = events.data();
  const auto service_size = events.size();
  while (true) {
#ifdef NET_USE_EPOLL
    const auto count = ::epoll_wait(handle_, service_data, service_size, -1);
#else
    const auto count = ::kevent(handle_, nullptr, 0, service_data, service_size, nullptr);
#endif
    if (count < 0) {
      if (errno != EINTR) {
        throw exception("get queued events", errno);
      }
      break;
    }
    for (std::size_t i = 0, max = static_cast<std::size_t>(count); i < max; i++) {
#ifdef NET_USE_EPOLL
      auto data = events[i].data.ptr;
#else
      auto data = events[i].udata;
#endif
      if (data) {
        auto& handler = *static_cast<event*>(data);
        handler();
      }
    }
  }
}

std::error_code service::close() noexcept {
  if (valid()) {
    if (::close(handle_) < 0) {
      return { errno, std::system_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace net
