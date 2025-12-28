#pragma once

#include <cstdint>

namespace telemetry {

struct MetricsSnapshot final {
  // Timestamp of snapshot creation (monotonic-ish in ms since epoch; sufficient for dashboard).
  std::uint64_t ts_ms{0};

  // CPU usage (% busy) over last sampling interval.
  double cpu_usage_pct{0.0};

  // Memory (kB).
  std::uint64_t mem_total_kb{0};
  std::uint64_t mem_available_kb{0};

  // Temperature (C).
  double temperature_c{0.0};

  // Uptime (seconds).
  std::uint64_t uptime_s{0};
};

}  // namespace telemetry


