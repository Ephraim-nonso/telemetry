#pragma once

#include <cstdint>
#include <string_view>

namespace telemetry::net {

enum class CommandType : std::uint8_t {
  kUnknown = 0,
  kPing,
  kGet,
  kRestart,
  kThrottle,
};

struct ParsedCommand final {
  CommandType type{CommandType::kUnknown};
  std::uint32_t throttle_ms{0};
  bool ok{true};
  const char* error{nullptr};
};

// Parses a single line (no trailing \n, optional \r already stripped).
// Supported:
// - PING
// - GET
// - RESTART
// - THROTTLE <ms>
ParsedCommand parse_command(std::string_view line);

}  // namespace telemetry::net


