#pragma once
#include <net/async.h>
#include <net/error.h>
#include <net/handle.h>

namespace net {

class service final : public handle {
public:
  service();

  service(service&& other) noexcept = default;
  service& operator=(service&& other) noexcept = default;

  service(const service& other) = delete;
  service& operator=(const service& other) = delete;

  ~service() {
    close();
  }

  // Runs service on the specified processor.
  void run(int processor = -1);

  // Closes service.
  std::error_code close() noexcept override;
};

}  // namespace net
