#pragma once
#include <memory>

extern "C" {

struct tls;

}  // extern "C"

namespace net::tls {

using context = ::tls;
using context_ptr = std::unique_ptr<context, void (*)(context*)>;

}  // namespace net::tls
