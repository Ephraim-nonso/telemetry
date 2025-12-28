#include "telemetry/metrics/default_sources.h"

namespace telemetry::metrics {

// Forward decls implemented per-platform (compiled conditionally via CMake).
void add_linux_sources(Collector& collector);
void add_macos_sources(Collector& collector);
void add_windows_sources(Collector& collector);
void add_simulated_sources(Collector& collector);

void add_default_sources(Collector& collector) {
#if defined(__linux__)
  add_linux_sources(collector);
#elif defined(__APPLE__)
  add_macos_sources(collector);
#elif defined(_WIN32)
  add_windows_sources(collector);
#else
  add_simulated_sources(collector);
#endif
}

}  // namespace telemetry::metrics


