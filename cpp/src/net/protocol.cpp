#include "telemetry/net/protocol.h"

namespace telemetry::net {

namespace {

static bool starts_with(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

}  // namespace

ParsedCommand parse_command(std::string_view line) {
  if (line == "PING") return ParsedCommand{CommandType::kPing, 0, true, nullptr};
  if (line == "GET") return ParsedCommand{CommandType::kGet, 0, true, nullptr};
  if (line == "RESTART") return ParsedCommand{CommandType::kRestart, 0, true, nullptr};

  if (starts_with(line, "THROTTLE ")) {
    const std::string_view arg = line.substr(std::string_view("THROTTLE ").size());
    if (arg.empty()) return ParsedCommand{CommandType::kThrottle, 0, false, "missing ms"};

    unsigned long ms = 0;
    for (char ch : arg) {
      if (ch < '0' || ch > '9') return ParsedCommand{CommandType::kThrottle, 0, false, "invalid ms"};
      ms = ms * 10UL + static_cast<unsigned long>(ch - '0');
      if (ms > 60000UL) return ParsedCommand{CommandType::kThrottle, 0, false, "ms too large"};
    }
    return ParsedCommand{CommandType::kThrottle, static_cast<std::uint32_t>(ms), true, nullptr};
  }

  return ParsedCommand{CommandType::kUnknown, 0, true, nullptr};
}

}  // namespace telemetry::net


