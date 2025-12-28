#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "telemetry/metrics/default_sources.h"
#include "telemetry/net/tcp_server.h"

namespace {

static void print_usage(const char* argv0) {
  std::fprintf(stderr,
               "Usage: %s [--host <ip>] [--port <port>] [--throttle-ms <ms>] [--run-for-ms <ms>]\n"
               "Defaults: --host 0.0.0.0 --port 9000 --throttle-ms 250 --run-for-ms 0\n",
               argv0);
}

static bool parse_u16(const char* s, std::uint16_t& out) {
  if (!s || !*s) return false;
  unsigned long v = 0;
  for (const char* p = s; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10UL + static_cast<unsigned long>(*p - '0');
    if (v > 65535UL) return false;
  }
  out = static_cast<std::uint16_t>(v);
  return true;
}

static bool parse_u32(const char* s, std::uint32_t& out) {
  if (!s || !*s) return false;
  unsigned long v = 0;
  for (const char* p = s; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10UL + static_cast<unsigned long>(*p - '0');
    if (v > 0xFFFFFFFFUL) return false;
  }
  out = static_cast<std::uint32_t>(v);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  telemetry::net::TcpServerConfig cfg{};

  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (std::strcmp(a, "--host") == 0 && i + 1 < argc) {
      cfg.host = argv[++i];
    } else if (std::strcmp(a, "--port") == 0 && i + 1 < argc) {
      std::uint16_t port = 0;
      if (!parse_u16(argv[++i], port)) {
        std::fprintf(stderr, "Invalid --port\n");
        return 2;
      }
      cfg.port = port;
    } else if (std::strcmp(a, "--throttle-ms") == 0 && i + 1 < argc) {
      std::uint32_t ms = 0;
      if (!parse_u32(argv[++i], ms)) {
        std::fprintf(stderr, "Invalid --throttle-ms\n");
        return 2;
      }
      cfg.throttle_ms = ms;
    } else if (std::strcmp(a, "--run-for-ms") == 0 && i + 1 < argc) {
      std::uint32_t ms = 0;
      if (!parse_u32(argv[++i], ms)) {
        std::fprintf(stderr, "Invalid --run-for-ms\n");
        return 2;
      }
      cfg.run_for_ms = ms;
    } else {
      std::fprintf(stderr, "Unknown arg: %s\n", a);
      print_usage(argv[0]);
      return 2;
    }
  }

  telemetry::metrics::Collector collector;
  telemetry::metrics::add_default_sources(collector);

  std::fprintf(stderr, "telemetryd starting: host=%s port=%u throttle_ms=%u\n", cfg.host,
               static_cast<unsigned>(cfg.port), static_cast<unsigned>(cfg.throttle_ms));
  if (cfg.run_for_ms != 0) {
    std::fprintf(stderr, "telemetryd will exit after run_for_ms=%u\n", static_cast<unsigned>(cfg.run_for_ms));
  }
  std::fprintf(stderr, "telemetryd listening... \n");

  telemetry::net::TcpServer server(collector, cfg);
  const telemetry::Status st = server.run_forever();
  if (!st.ok()) {
    std::fprintf(stderr, "telemetryd failed: code=%u msg=%s\n", static_cast<unsigned>(st.code),
                 st.message ? st.message : "(none)");
    return 1;
  }
  std::fprintf(stderr, "telemetryd stopped.\n");
  return 0;
}


