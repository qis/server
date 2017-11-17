#pragma once
#include <net/handle.h>
#include <functional>
#include <string>
#include <system_error>
#include <csignal>

#ifndef SIGPIPE
#define SIGPIPE 1
#endif

namespace net {

// Permanently installs a signal handler.
void signal(int signum, std::function<void(int)> handler = {});

// Deops user privileges to the given user on unix systems.
void drop(const std::string& user);

class file_view : public net::handle {
public:
  file_view() noexcept;
  file_view(file_view&& other) noexcept;
  file_view& operator=(file_view&& other) noexcept;

  ~file_view();

  operator bool() const noexcept;

  std::error_code open(const std::string& filename) noexcept;
  std::string_view data() const noexcept;
  std::size_t size() const noexcept;

  std::error_code close() noexcept override;

private:
  void* data_ = nullptr;
  std::size_t size_ = 0;

#ifdef WIN32
  void* mapping_ = nullptr;
#endif
};

}  // namespace net
