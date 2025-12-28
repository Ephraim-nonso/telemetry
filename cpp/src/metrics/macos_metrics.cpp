#include "telemetry/metrics/collector.h"
#include "telemetry/metrics/metric_source.h"

#ifdef __APPLE__

#include <sys/sysctl.h>

#include <chrono>
#include <cstdint>

#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>

namespace telemetry::metrics {

namespace {

class MacCpuUsageSource final : public MetricSource {
 public:
  const char* name() const override { return "macos_cpu"; }

  Status collect(MetricsSnapshot& out) override {
    natural_t cpu_count = 0;
    processor_info_array_t cpu_info = nullptr;
    mach_msg_type_number_t num_cpu_info = 0;

    const kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &cpu_info,
                                                 &num_cpu_info);
    if (kr != KERN_SUCCESS || !cpu_info) return Status::Unavailable("host_processor_info failed");

    // Aggregate across CPUs.
    unsigned long long user = 0, sys = 0, idle = 0, nice = 0;
    for (natural_t i = 0; i < cpu_count; ++i) {
      const std::size_t base = static_cast<std::size_t>(i) * CPU_STATE_MAX;
      user += static_cast<unsigned long long>(cpu_info[base + CPU_STATE_USER]);
      sys += static_cast<unsigned long long>(cpu_info[base + CPU_STATE_SYSTEM]);
      idle += static_cast<unsigned long long>(cpu_info[base + CPU_STATE_IDLE]);
      nice += static_cast<unsigned long long>(cpu_info[base + CPU_STATE_NICE]);
    }

    // Free vm-allocated memory for cpu_info.
    (void)vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpu_info),
                        static_cast<vm_size_t>(num_cpu_info * sizeof(integer_t)));

    const unsigned long long total = user + sys + idle + nice;
    if (!has_prev_) {
      prev_total_ = total;
      prev_idle_ = idle;
      has_prev_ = true;
      out.cpu_usage_pct = 0.0;
      return Status::Ok();
    }

    const unsigned long long totald = total - prev_total_;
    const unsigned long long idled = idle - prev_idle_;
    prev_total_ = total;
    prev_idle_ = idle;

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

class MacMemSource final : public MetricSource {
 public:
  const char* name() const override { return "macos_mem"; }

  Status collect(MetricsSnapshot& out) override {
    // Total physical memory.
    std::uint64_t memsize = 0;
    std::size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0) != 0) {
      return Status::Unavailable("sysctl hw.memsize failed");
    }

    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmstat{};
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmstat), &count) !=
        KERN_SUCCESS) {
      return Status::Unavailable("host_statistics64 failed");
    }

    vm_size_t page_size = 0;
    if (host_page_size(mach_host_self(), &page_size) != KERN_SUCCESS || page_size == 0) {
      return Status::Unavailable("host_page_size failed");
    }

    // "Available" is not a perfect concept on macOS; we approximate with free + inactive.
    const std::uint64_t free_bytes = static_cast<std::uint64_t>(vmstat.free_count) * static_cast<std::uint64_t>(page_size);
    const std::uint64_t inactive_bytes =
        static_cast<std::uint64_t>(vmstat.inactive_count) * static_cast<std::uint64_t>(page_size);
    const std::uint64_t avail_bytes = free_bytes + inactive_bytes;

    out.mem_total_kb = memsize / 1024ULL;
    out.mem_available_kb = avail_bytes / 1024ULL;
    return Status::Ok();
  }
};

class MacUptimeSource final : public MetricSource {
 public:
  const char* name() const override { return "macos_uptime"; }

  Status collect(MetricsSnapshot& out) override {
    // kern.boottime returns timeval.
    timeval bt{};
    std::size_t len = sizeof(bt);
    if (sysctlbyname("kern.boottime", &bt, &len, nullptr, 0) != 0) return Status::Unavailable("sysctl kern.boottime failed");

    const auto now = std::chrono::system_clock::now();
    const auto now_s = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto boot_s = static_cast<long long>(bt.tv_sec);
    const auto up = (now_s > boot_s) ? static_cast<std::uint64_t>(now_s - boot_s) : 0ULL;
    out.uptime_s = up;
    return Status::Ok();
  }
};

class MacTemperatureSource final : public MetricSource {
 public:
  const char* name() const override { return "macos_temperature"; }
  Status collect(MetricsSnapshot&) override {
    // CPU temperature is not available via stable public APIs on macOS without vendor-specific/SMC access.
    return Status::Unavailable("temperature unsupported on macOS by default");
  }
};

}  // namespace

void add_macos_sources(Collector& collector) {
  collector.add_source(std::make_unique<MacCpuUsageSource>());
  collector.add_source(std::make_unique<MacMemSource>());
  collector.add_source(std::make_unique<MacUptimeSource>());
  collector.add_source(std::make_unique<MacTemperatureSource>());
}

}  // namespace telemetry::metrics

#endif


