#pragma once

#include <cstdint>

namespace telemetry {

enum class StatusCode : std::uint8_t {
  kOk = 0,
  kUnavailable = 1,
  kInvalidArgument = 2,
  kIoError = 3,
  kInternal = 4,
};

struct Status final {
  StatusCode code{StatusCode::kOk};
  const char* message{nullptr};

  constexpr bool ok() const { return code == StatusCode::kOk; }

  static constexpr Status Ok() { return Status{StatusCode::kOk, nullptr}; }
  static constexpr Status Unavailable(const char* msg) { return Status{StatusCode::kUnavailable, msg}; }
  static constexpr Status InvalidArgument(const char* msg) { return Status{StatusCode::kInvalidArgument, msg}; }
  static constexpr Status IoError(const char* msg) { return Status{StatusCode::kIoError, msg}; }
  static constexpr Status Internal(const char* msg) { return Status{StatusCode::kInternal, msg}; }
};

}  // namespace telemetry


