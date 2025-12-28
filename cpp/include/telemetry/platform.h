#pragma once

namespace telemetry {

inline const char* platform_name() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

inline bool temperature_best_effort_supported() {
#if defined(__linux__)
  return true;
#else
  return false;
#endif
}

}  // namespace telemetry


