#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

#include "telemetry/metrics/collector.h"
#include "telemetry/metrics_snapshot.h"
#include "telemetry/status.h"

namespace telemetry::net {

// Cross-platform socket handle representation.
#ifdef _WIN32
using SocketHandle = std::uintptr_t;
#else
using SocketHandle = int;
#endif

struct TcpServerConfig final {
  const char* host = "0.0.0.0";
  std::uint16_t port = 9000;
  std::uint32_t throttle_ms = 250;
  std::uint32_t run_for_ms = 0;  // 0 = run forever
};

class TcpServer final {
 public:
  TcpServer(metrics::Collector& collector, TcpServerConfig cfg);

  Status run_forever();

 private:
  Status handle_command(std::string_view cmd, SocketHandle client_fd);
  Status write_json_metrics(SocketHandle client_fd, const telemetry::MetricsSnapshot& snap, Status collect_status);
  Status write_json_ok(SocketHandle client_fd, const char* msg);
  Status write_json_error(SocketHandle client_fd, const char* msg);

  metrics::Collector& collector_;
  TcpServerConfig cfg_;
  std::atomic<std::uint32_t> throttle_ms_;

  // Cached snapshot for throttling.
  telemetry::MetricsSnapshot last_snapshot_{};
  Status last_collect_status_{Status::Ok()};
  std::uint64_t last_collect_ms_{0};
};

}  // namespace telemetry::net


