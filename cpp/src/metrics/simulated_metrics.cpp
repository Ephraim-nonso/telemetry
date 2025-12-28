#include <cmath>

#include "telemetry/metrics/metric_source.h"
#include "telemetry/metrics/collector.h"
#include "telemetry/util/time.h"

namespace telemetry::metrics {

namespace {

class SimulatedSource final : public MetricSource {
 public:
  SimulatedSource() : start_ms_(telemetry::util::unix_time_ms()) {}
  const char* name() const override { return "simulated"; }

  Status collect(MetricsSnapshot& out) override {
    const std::uint64_t t = telemetry::util::unix_time_ms();
    const double seconds = static_cast<double>(t % 600000ULL) / 1000.0;

    // Smooth-ish signals.
    out.cpu_usage_pct = 20.0 + 30.0 * std::sin(seconds * 0.7);
    out.mem_total_kb = 512ULL * 1024ULL;
    out.mem_available_kb = static_cast<std::uint64_t>((256.0 + 64.0 * std::sin(seconds * 0.2)) * 1024.0);
    out.temperature_c = 45.0 + 8.0 * std::sin(seconds * 0.1);
    out.uptime_s = (t - start_ms_) / 1000ULL;
    return Status::Ok();
  }

 private:
  std::uint64_t start_ms_{0};
};

}  // namespace

void add_simulated_sources(Collector& collector) { collector.add_source(std::make_unique<SimulatedSource>()); }

}  // namespace telemetry::metrics

