#include "minitest.h"
#include <memory>

#include "telemetry/metrics/collector.h"

namespace {

class OkSource final : public telemetry::metrics::MetricSource {
 public:
  const char* name() const override { return "ok"; }
  telemetry::Status collect(telemetry::MetricsSnapshot& out) override {
    out.mem_total_kb = 123;
    return telemetry::Status::Ok();
  }
};

class UnavailableSource final : public telemetry::metrics::MetricSource {
 public:
  const char* name() const override { return "unavail"; }
  telemetry::Status collect(telemetry::MetricsSnapshot&) override { return telemetry::Status::Unavailable("nope"); }
};

class IoErrorSource final : public telemetry::metrics::MetricSource {
 public:
  const char* name() const override { return "ioerr"; }
  telemetry::Status collect(telemetry::MetricsSnapshot&) override { return telemetry::Status::IoError("bad"); }
};

}  // namespace

TELEMETRY_TEST_CASE("Collector ignores Unavailable but returns hard errors") {
  telemetry::metrics::Collector c;
  telemetry::MetricsSnapshot snap{};

  c.add_source(std::make_unique<OkSource>());
  c.add_source(std::make_unique<UnavailableSource>());
  REQUIRE(c.collect(snap).ok());
  REQUIRE(snap.mem_total_kb == 123);

  telemetry::metrics::Collector c2;
  telemetry::MetricsSnapshot snap2{};
  c2.add_source(std::make_unique<OkSource>());
  c2.add_source(std::make_unique<IoErrorSource>());
  const telemetry::Status st = c2.collect(snap2);
  REQUIRE_FALSE(st.ok());
  REQUIRE(st.code == telemetry::StatusCode::kIoError);
}


