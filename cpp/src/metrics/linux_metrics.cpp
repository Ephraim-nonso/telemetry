#include <cerrno>
#include <cstdio>
#include <cstring>

#include "telemetry/metrics/metric_source.h"
#include "telemetry/metrics/collector.h"

namespace telemetry::metrics {

#ifdef __linux__

namespace {

class LinuxCpuUsageSource final : public MetricSource {
 public:
  const char* name() const override { return "linux_cpu"; }

  Status collect(MetricsSnapshot& out) override {
    std::FILE* f = std::fopen("/proc/stat", "r");
    if (!f) return Status::Unavailable("open /proc/stat failed");

    char line[256];
    const char* got = std::fgets(line, sizeof(line), f);
    std::fclose(f);
    if (!got) return Status::IoError("read /proc/stat failed");

    // Format: cpu  user nice system idle iowait irq softirq steal guest guest_nice
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    const int n = std::sscanf(line, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
                              &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    if (n < 4) return Status::IoError("parse /proc/stat failed");

    const unsigned long long idle_all = idle + iowait;
    const unsigned long long non_idle = user + nice + system + irq + softirq + steal;
    const unsigned long long total = idle_all + non_idle;

    if (!has_prev_) {
      prev_total_ = total;
      prev_idle_ = idle_all;
      has_prev_ = true;
      out.cpu_usage_pct = 0.0;
      return Status::Ok();
    }

    const unsigned long long totald = total - prev_total_;
    const unsigned long long idled = idle_all - prev_idle_;
    prev_total_ = total;
    prev_idle_ = idle_all;

    if (totald == 0) {
      out.cpu_usage_pct = 0.0;
      return Status::Ok();
    }

    const double usage = (static_cast<double>(totald - idled) / static_cast<double>(totald)) * 100.0;
    out.cpu_usage_pct = usage < 0.0 ? 0.0 : (usage > 100.0 ? 100.0 : usage);
    return Status::Ok();
  }

 private:
  bool has_prev_{false};
  unsigned long long prev_total_{0};
  unsigned long long prev_idle_{0};
};

class LinuxMemInfoSource final : public MetricSource {
 public:
  const char* name() const override { return "linux_meminfo"; }

  Status collect(MetricsSnapshot& out) override {
    std::FILE* f = std::fopen("/proc/meminfo", "r");
    if (!f) return Status::Unavailable("open /proc/meminfo failed");

    char line[256];
    std::uint64_t total_kb = 0;
    std::uint64_t avail_kb = 0;

    while (std::fgets(line, sizeof(line), f)) {
      unsigned long long v = 0;
      if (std::sscanf(line, "MemTotal: %llu kB", &v) == 1) total_kb = static_cast<std::uint64_t>(v);
      if (std::sscanf(line, "MemAvailable: %llu kB", &v) == 1) avail_kb = static_cast<std::uint64_t>(v);
      if (total_kb && avail_kb) break;
    }

    std::fclose(f);

    if (!total_kb) return Status::IoError("parse MemTotal failed");
    if (!avail_kb) return Status::IoError("parse MemAvailable failed");

    out.mem_total_kb = total_kb;
    out.mem_available_kb = avail_kb;
    return Status::Ok();
  }
};

class LinuxUptimeSource final : public MetricSource {
 public:
  const char* name() const override { return "linux_uptime"; }

  Status collect(MetricsSnapshot& out) override {
    std::FILE* f = std::fopen("/proc/uptime", "r");
    if (!f) return Status::Unavailable("open /proc/uptime failed");

    double uptime = 0.0;
    const int n = std::fscanf(f, "%lf", &uptime);
    std::fclose(f);
    if (n != 1) return Status::IoError("parse /proc/uptime failed");

    if (uptime < 0.0) uptime = 0.0;
    out.uptime_s = static_cast<std::uint64_t>(uptime);
    return Status::Ok();
  }
};

class LinuxTemperatureSource final : public MetricSource {
 public:
  const char* name() const override { return "linux_temperature"; }

  Status collect(MetricsSnapshot& out) override {
    // Common path on many embedded Linux systems.
    std::FILE* f = std::fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return Status::Unavailable("open thermal temp failed");

    long temp_milli_c = 0;
    const int n = std::fscanf(f, "%ld", &temp_milli_c);
    std::fclose(f);
    if (n != 1) return Status::IoError("parse thermal temp failed");

    out.temperature_c = static_cast<double>(temp_milli_c) / 1000.0;
    return Status::Ok();
  }
};

}  // namespace

void add_linux_sources(Collector& collector) {
  collector.add_source(std::make_unique<LinuxCpuUsageSource>());
  collector.add_source(std::make_unique<LinuxMemInfoSource>());
  collector.add_source(std::make_unique<LinuxUptimeSource>());
  collector.add_source(std::make_unique<LinuxTemperatureSource>());
}

#endif

}  // namespace telemetry::metrics


