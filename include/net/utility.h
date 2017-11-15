#pragma once
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

}  // namespace net
