#pragma once

#include <memory>

#include "telemetry/metrics/collector.h"

namespace telemetry::metrics {

// Adds a default set of metric sources to the collector.
// - Linux: reads from /proc and /sys
// - Non-Linux: simulated values
void add_default_sources(Collector& collector);

}  // namespace telemetry::metrics


