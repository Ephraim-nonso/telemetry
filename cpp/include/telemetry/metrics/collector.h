#pragma once

#include <memory>
#include <vector>

#include "telemetry/metrics/metric_source.h"
#include "telemetry/metrics_snapshot.h"
#include "telemetry/status.h"

namespace telemetry::metrics {

class Collector final {
 public:
  Collector() = default;

  void add_source(std::unique_ptr<MetricSource> src);
  Status collect(MetricsSnapshot& out);

 private:
  std::vector<std::unique_ptr<MetricSource>> sources_;
};

}  // namespace telemetry::metrics


