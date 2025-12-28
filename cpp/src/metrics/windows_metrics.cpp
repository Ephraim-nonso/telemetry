#include "telemetry/metrics/collector.h"
#include "telemetry/metrics/metric_source.h"

#ifdef _WIN32

#define NOMINMAX
#include <windows.h>

#include <pdh.h>
#include <pdhmsg.h>

#include <cstdint>

namespace telemetry::metrics {

namespace {

class WinCpuUsageSource final : public MetricSource {
 public:
  WinCpuUsageSource() {
    if (PdhOpenQuery(nullptr, 0, &query_) != ERROR_SUCCESS) return;
    if (PdhAddEnglishCounter(query_, L"\\Processor(_Total)\\% Processor Time", 0, &counter_) != ERROR_SUCCESS) return;
    (void)PdhCollectQueryData(query_);  // prime
    ok_ = true;
  }

  ~WinCpuUsageSource() override {
    if (query_) PdhCloseQuery(query_);
  }

  const char* name() const override { return "windows_cpu"; }

  Status collect(MetricsSnapshot& out) override {
    if (!ok_) return Status::Unavailable("PDH unavailable");
    if (PdhCollectQueryData(query_) != ERROR_SUCCESS) return Status::Unavailable("PDH collect failed");

    PDH_FMT_COUNTERVALUE v{};
    if (PdhGetFormattedCounterValue(counter_, PDH_FMT_DOUBLE, nullptr, &v) != ERROR_SUCCESS) {
      return Status::Unavailable("PDH format failed");
    }
    double pct = v.doubleValue;
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    out.cpu_usage_pct = pct;
    return Status::Ok();
  }

 private:
  bool ok_{false};
  PDH_HQUERY query_{nullptr};
  PDH_HCOUNTER counter_{nullptr};
};

class WinMemSource final : public MetricSource {
 public:
  const char* name() const override { return "windows_mem"; }
  Status collect(MetricsSnapshot& out) override {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return Status::Unavailable("GlobalMemoryStatusEx failed");
    out.mem_total_kb = static_cast<std::uint64_t>(ms.ullTotalPhys / 1024ULL);
    out.mem_available_kb = static_cast<std::uint64_t>(ms.ullAvailPhys / 1024ULL);
    return Status::Ok();
  }
};

class WinUptimeSource final : public MetricSource {
 public:
  const char* name() const override { return "windows_uptime"; }
  Status collect(MetricsSnapshot& out) override {
    out.uptime_s = static_cast<std::uint64_t>(GetTickCount64() / 1000ULL);
    return Status::Ok();
  }
};

class WinTemperatureSource final : public MetricSource {
 public:
  const char* name() const override { return "windows_temperature"; }
  Status collect(MetricsSnapshot&) override {
    // Temperature generally requires WMI + vendor drivers or a third-party sensor service.
    return Status::Unavailable("temperature unsupported on Windows by default");
  }
};

}  // namespace

void add_windows_sources(Collector& collector) {
  collector.add_source(std::make_unique<WinCpuUsageSource>());
  collector.add_source(std::make_unique<WinMemSource>());
  collector.add_source(std::make_unique<WinUptimeSource>());
  collector.add_source(std::make_unique<WinTemperatureSource>());
}

}  // namespace telemetry::metrics

#endif


