#pragma once
#include <net/async.h>
#include <net/error.h>
#include <net/handle.h>

namespace net {

class service final : public handle {
public:
  service();

  service(service&& other) = delete;
  service& operator=(service&& other) = delete;

  service(const service& other) = delete;
  service& operator=(const service& other) = delete;

  ~service();

  // Runs service on the specified processor.
  void run(int processor = -1);

  // Closes service.
  std::error_code close() noexcept override;

private:
  bool stop_ = true;
};

}  // namespace net
