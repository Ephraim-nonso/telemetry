#include "telemetry/net/tcp_server.h"

#ifdef _WIN32

#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <cstring>

#include <array>
#include <string_view>

#include "telemetry/net/protocol.h"
#include "telemetry/platform.h"
#include "telemetry/util/time.h"

#pragma comment(lib, "Ws2_32.lib")

namespace telemetry::net {

namespace {

constexpr int kMaxClients = 64;
constexpr std::size_t kBufSize = 1024;

struct Client final {
  SOCKET s{INVALID_SOCKET};
  std::array<char, kBufSize> buf{};
  std::size_t len{0};
};

static void close_client(Client& c) {
  if (c.s != INVALID_SOCKET) closesocket(c.s);
  c.s = INVALID_SOCKET;
  c.len = 0;
}

static bool set_nonblocking(SOCKET s) {
  u_long mode = 1;
  return ioctlsocket(s, FIONBIO, &mode) == 0;
}

static SocketHandle to_handle(SOCKET s) { return static_cast<SocketHandle>(reinterpret_cast<std::uintptr_t>(s)); }
static SOCKET to_socket(SocketHandle h) { return reinterpret_cast<SOCKET>(static_cast<std::uintptr_t>(h)); }

}  // namespace

TcpServer::TcpServer(metrics::Collector& collector, TcpServerConfig cfg)
    : collector_(collector), cfg_(cfg), throttle_ms_(cfg.throttle_ms) {}

Status TcpServer::run_forever() {
  const std::uint64_t start_ms = telemetry::util::unix_time_ms();

  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return Status::IoError("WSAStartup failed");

  SOCKET listen_s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_s == INVALID_SOCKET) {
    WSACleanup();
    return Status::IoError("socket() failed");
  }

  BOOL yes = TRUE;
  (void)setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg_.port);
  if (inet_pton(AF_INET, cfg_.host, &addr.sin_addr) != 1) {
    closesocket(listen_s);
    WSACleanup();
    return Status::InvalidArgument("invalid host");
  }

  if (bind(listen_s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
    closesocket(listen_s);
    WSACleanup();
    return Status::IoError("bind() failed");
  }

  if (listen(listen_s, 16) != 0) {
    closesocket(listen_s);
    WSACleanup();
    return Status::IoError("listen() failed");
  }

  (void)set_nonblocking(listen_s);

  std::array<Client, kMaxClients> clients{};

  while (true) {
    if (cfg_.run_for_ms != 0) {
      const std::uint64_t now = telemetry::util::unix_time_ms();
      if (now - start_ms >= cfg_.run_for_ms) {
        closesocket(listen_s);
        for (auto& c : clients) close_client(c);
        WSACleanup();
        return Status::Ok();
      }
    }

    std::array<WSAPOLLFD, kMaxClients + 1> pfds{};
    pfds[0].fd = listen_s;
    pfds[0].events = POLLRDNORM;

    for (int i = 0; i < kMaxClients; ++i) {
      pfds[i + 1].fd = clients[i].s;
      pfds[i + 1].events = (clients[i].s != INVALID_SOCKET) ? POLLRDNORM : 0;
    }

    const int rc = WSAPoll(pfds.data(), static_cast<ULONG>(pfds.size()), 250);
    if (rc < 0) continue;

    if (pfds[0].revents & POLLRDNORM) {
      while (true) {
        SOCKET cs = accept(listen_s, nullptr, nullptr);
        if (cs == INVALID_SOCKET) break;
        (void)set_nonblocking(cs);

        bool placed = false;
        for (auto& c : clients) {
          if (c.s == INVALID_SOCKET) {
            c.s = cs;
            c.len = 0;
            placed = true;
            break;
          }
        }
        if (!placed) closesocket(cs);
      }
    }

    for (int i = 0; i < kMaxClients; ++i) {
      Client& c = clients[i];
      WSAPOLLFD& p = pfds[i + 1];
      if (c.s == INVALID_SOCKET) continue;
      if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        close_client(c);
        continue;
      }
      if (p.revents & POLLRDNORM) {
        while (true) {
          if (c.len >= c.buf.size()) {
            (void)write_json_error(to_handle(c.s), "request too large");
            close_client(c);
            break;
          }
          const int n = recv(c.s, c.buf.data() + c.len, static_cast<int>(c.buf.size() - c.len), 0);
          if (n == 0) {
            close_client(c);
            break;
          }
          if (n < 0) {
            const int e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) break;
            close_client(c);
            break;
          }
          c.len += static_cast<std::size_t>(n);

          while (true) {
            const void* nl = std::memchr(c.buf.data(), '\n', c.len);
            if (!nl) break;
            const std::size_t line_len = static_cast<const char*>(nl) - c.buf.data();
            std::string_view line(c.buf.data(), line_len);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

            (void)handle_command(line, to_handle(c.s));

            const std::size_t remaining = c.len - (line_len + 1);
            if (remaining > 0) std::memmove(c.buf.data(), c.buf.data() + line_len + 1, remaining);
            c.len = remaining;
          }
        }
      }
    }
  }
}

Status TcpServer::handle_command(std::string_view cmd, SocketHandle client_fd) {
  const ParsedCommand pc = parse_command(cmd);
  if (pc.type == CommandType::kPing) return write_json_ok(client_fd, "pong");

  if (pc.type == CommandType::kGet) {
    const std::uint64_t now = telemetry::util::unix_time_ms();
    const std::uint32_t throttle = throttle_ms_.load(std::memory_order_relaxed);

    if (last_collect_ms_ == 0 || now - last_collect_ms_ >= throttle) {
      MetricsSnapshot snap{};
      snap.ts_ms = now;
      last_collect_status_ = collector_.collect(snap);
      last_snapshot_ = snap;
      last_collect_ms_ = now;
    }
    return write_json_metrics(client_fd, last_snapshot_, last_collect_status_);
  }

  if (pc.type == CommandType::kRestart) return write_json_ok(client_fd, "restart requested");

  if (pc.type == CommandType::kThrottle) {
    if (!pc.ok) return write_json_error(client_fd, pc.error ? pc.error : "invalid throttle");
    throttle_ms_.store(pc.throttle_ms, std::memory_order_relaxed);
    cfg_.throttle_ms = pc.throttle_ms;
    return write_json_ok(client_fd, "throttle set");
  }

  return write_json_error(client_fd, "unknown command");
}

static Status send_all(SOCKET s, const char* data, int len) {
  int sent = 0;
  while (sent < len) {
    const int n = send(s, data + sent, len - sent, 0);
    if (n <= 0) return Status::IoError("send() failed");
    sent += n;
  }
  return Status::Ok();
}

Status TcpServer::write_json_metrics(SocketHandle client_fd, const telemetry::MetricsSnapshot& snap, Status collect_status) {
  char out[512];
  const std::uint32_t throttle = throttle_ms_.load(std::memory_order_relaxed);
  const int n = std::snprintf(
      out, sizeof(out),
      "{\"ok\":%s,\"status_code\":%u,\"platform\":\"%s\",\"temperature_best_effort\":%s,\"ts_ms\":%llu,\"cpu_usage_pct\":%.2f,"
      "\"mem_total_kb\":%llu,\"mem_available_kb\":%llu,\"temperature_c\":%.2f,"
      "\"uptime_s\":%llu,\"throttle_ms\":%u}\n",
      collect_status.ok() ? "true" : "false", static_cast<unsigned>(collect_status.code),
      telemetry::platform_name(), telemetry::temperature_best_effort_supported() ? "true" : "false",
      static_cast<unsigned long long>(snap.ts_ms), snap.cpu_usage_pct,
      static_cast<unsigned long long>(snap.mem_total_kb), static_cast<unsigned long long>(snap.mem_available_kb),
      snap.temperature_c, static_cast<unsigned long long>(snap.uptime_s), static_cast<unsigned>(throttle));
  if (n <= 0 || n >= static_cast<int>(sizeof(out))) return Status::Internal("response too large");
  return send_all(to_socket(client_fd), out, n);
}

Status TcpServer::write_json_ok(SocketHandle client_fd, const char* msg) {
  char out[256];
  const int n = std::snprintf(out, sizeof(out), "{\"ok\":true,\"message\":\"%s\"}\n", msg ? msg : "");
  if (n <= 0 || n >= static_cast<int>(sizeof(out))) return Status::Internal("response too large");
  return send_all(to_socket(client_fd), out, n);
}

Status TcpServer::write_json_error(SocketHandle client_fd, const char* msg) {
  char out[256];
  const int n = std::snprintf(out, sizeof(out), "{\"ok\":false,\"error\":\"%s\"}\n", msg ? msg : "error");
  if (n <= 0 || n >= static_cast<int>(sizeof(out))) return Status::Internal("response too large");
  return send_all(to_socket(client_fd), out, n);
}

}  // namespace telemetry::net

#endif


