#include "telemetry/metrics/collector.h"

namespace telemetry::metrics {

void Collector::add_source(std::unique_ptr<MetricSource> src) { sources_.push_back(std::move(src)); }

Status Collector::collect(MetricsSnapshot& out) {
  // We intentionally keep "best-effort" semantics: if one source fails, we still
  // return partial data.
  //
  // IMPORTANT: StatusCode::kUnavailable is treated as non-fatal (common for
  // optional metrics like temperature on some platforms).
  Status first_error = Status::Ok();
  for (auto& s : sources_) {
    const Status st = s->collect(out);
    if (!st.ok() && st.code != StatusCode::kUnavailable && first_error.ok()) first_error = st;
  }
  return first_error;
}

}  // namespace telemetry::metrics


