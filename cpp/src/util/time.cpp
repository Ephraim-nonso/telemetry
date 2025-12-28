#include "telemetry/util/time.h"

#include <chrono>

namespace telemetry::util {

std::uint64_t unix_time_ms() {
  const auto now = std::chrono::system_clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  return static_cast<std::uint64_t>(ms.count());
}

}  // namespace telemetry::util


