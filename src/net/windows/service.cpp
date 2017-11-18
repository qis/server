#include <net/service.h>
#include <net/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <cstdio>

namespace net {
namespace {

class library {
public:
  library() {
    WSADATA wsadata = {};
    if (const auto code = WSAStartup(MAKEWORD(2, 2), &wsadata)) {
      throw exception("wsa startup", code);
    }
    const auto major = LOBYTE(wsadata.wVersion);
    const auto minor = HIBYTE(wsadata.wVersion);
    if (major < 2 || (major == 2 && minor < 2)) {
      throw exception("wsa startup", "unsupported version: " + std::to_string(major) + "." + std::to_string(minor));
    }
  }

  ~library() {
    WSACleanup();
    if (IsDebuggerPresent()) {
      std::puts("Press ENTER or CTRL+C to exit . . .");
      std::getchar();
    }
  }
};

}  // namespace

service::service() {
  static library library;
  const auto handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (!handle) {
    throw exception("create service", GetLastError());
  }
  reset(reinterpret_cast<handle_type>(handle));
}

void service::run(int processor) {
  if (!valid()) {
    throw exception("run service", std::errc::bad_file_descriptor);
  }
  if (processor >= 0) {
    const auto mask = (static_cast<DWORD_PTR>(1) << processor);
    if (!SetThreadAffinityMask(GetCurrentThread(), mask)) {
      throw exception("set thread affinity", GetLastError());
    }
  }
  std::array<OVERLAPPED_ENTRY, 1024> events;
  const auto handle = as<HANDLE>();
  const auto service_data = events.data();
  const auto service_size = static_cast<ULONG>(events.size());
  while (true) {
    ULONG count = 0;
    if (!GetQueuedCompletionStatusEx(handle, service_data, service_size, &count, INFINITE, FALSE)) {
      if (const auto code = GetLastError(); code != ERROR_ABANDONED_WAIT_0) {
        throw exception("get queued events", code);
      }
      break;
    }
    for (ULONG i = 0; i < count; i++) {
      auto& ev = events[i];
      if (ev.lpOverlapped) {
        auto& handler = *static_cast<event*>(ev.lpOverlapped);
        handler(ev.dwNumberOfBytesTransferred);
      }
    }
  }
}

std::error_code service::close() noexcept {
  if (valid()) {
    if (!CloseHandle(as<HANDLE>())) {
      return { static_cast<int>(GetLastError()), std::system_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace net
