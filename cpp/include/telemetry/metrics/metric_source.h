#pragma once

#include "telemetry/metrics_snapshot.h"
#include "telemetry/status.h"

namespace telemetry::metrics {

class MetricSource {
 public:
  virtual ~MetricSource() = default;
  virtual const char* name() const = 0;
  virtual Status collect(MetricsSnapshot& out) = 0;
};

}  // namespace telemetry::metrics


