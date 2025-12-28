#include "telemetry/net/tcp_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <string_view>

#include "telemetry/net/protocol.h"
#include "telemetry/platform.h"
#include "telemetry/util/time.h"

namespace telemetry::net {

namespace {

constexpr int kMaxClients = 64;
constexpr std::size_t kBufSize = 1024;

struct Client final {
  int fd{-1};
  std::array<char, kBufSize> buf{};
  std::size_t len{0};
};

static bool set_nonblocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static void close_client(Client& c) {
  if (c.fd >= 0) close(c.fd);
  c.fd = -1;
  c.len = 0;
}

}  // namespace

TcpServer::TcpServer(metrics::Collector& collector, TcpServerConfig cfg)
    : collector_(collector), cfg_(cfg), throttle_ms_(cfg.throttle_ms) {}

Status TcpServer::run_forever() {
  const std::uint64_t start_ms = telemetry::util::unix_time_ms();
  const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) return Status::IoError("socket() failed");

  int yes = 1;
  (void)::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg_.port);
  if (::inet_pton(AF_INET, cfg_.host, &addr.sin_addr) != 1) {
    ::close(listen_fd);
    return Status::InvalidArgument("invalid host");
  }

  if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(listen_fd);
    return Status::IoError("bind() failed");
  }

  if (::listen(listen_fd, 16) != 0) {
    ::close(listen_fd);
    return Status::IoError("listen() failed");
  }

  if (!set_nonblocking(listen_fd)) {
    ::close(listen_fd);
    return Status::IoError("set_nonblocking(listen_fd) failed");
  }

  std::array<Client, kMaxClients> clients{};

  // Simple poll loop: [0] is listener, [1..] clients.
  while (true) {
    if (cfg_.run_for_ms != 0) {
      const std::uint64_t now = telemetry::util::unix_time_ms();
      if (now - start_ms >= cfg_.run_for_ms) {
        ::close(listen_fd);
        for (auto& c : clients) close_client(c);
        return Status::Ok();
      }
    }

    std::array<pollfd, kMaxClients + 1> pfds{};
    pfds[0].fd = listen_fd;
    pfds[0].events = POLLIN;

    for (int i = 0; i < kMaxClients; ++i) {
      pfds[i + 1].fd = clients[i].fd;
      pfds[i + 1].events = (clients[i].fd >= 0) ? POLLIN : 0;
    }

    const int rc = ::poll(pfds.data(), pfds.size(), 250);
    if (rc < 0) {
      if (errno == EINTR) continue;
      ::close(listen_fd);
      return Status::IoError("poll() failed");
    }

    if (pfds[0].revents & POLLIN) {
      // Accept as many as possible.
      while (true) {
        sockaddr_in caddr{};
        socklen_t clen = sizeof(caddr);
        const int cfd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&caddr), &clen);
        if (cfd < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) break;
          break;
        }
        (void)set_nonblocking(cfd);

        bool placed = false;
        for (auto& c : clients) {
          if (c.fd < 0) {
            c.fd = cfd;
            c.len = 0;
            placed = true;
            break;
          }
        }
        if (!placed) ::close(cfd);
      }
    }

    for (int i = 0; i < kMaxClients; ++i) {
      Client& c = clients[i];
      pollfd& p = pfds[i + 1];

      if (c.fd < 0) continue;
      if (p.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        close_client(c);
        continue;
      }

      if (p.revents & POLLIN) {
        // Read available data.
        while (true) {
          if (c.len >= c.buf.size()) {
            (void)write_json_error(c.fd, "request too large");
            close_client(c);
            break;
          }

          const ssize_t n = ::read(c.fd, c.buf.data() + c.len, c.buf.size() - c.len);
          if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close_client(c);
            break;
          }
          if (n == 0) {
            close_client(c);
            break;
          }

          c.len += static_cast<std::size_t>(n);

          // Process complete lines.
          while (true) {
            const void* nl = std::memchr(c.buf.data(), '\n', c.len);
            if (!nl) break;

            const std::size_t line_len = static_cast<const char*>(nl) - c.buf.data();
            std::string_view line(c.buf.data(), line_len);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

            (void)handle_command(line, c.fd);

            // Shift remaining bytes left.
            const std::size_t remaining = c.len - (line_len + 1);
            if (remaining > 0) std::memmove(c.buf.data(), c.buf.data() + line_len + 1, remaining);
            c.len = remaining;
          }
        }
      }
    }
  }
}

Status TcpServer::handle_command(std::string_view cmd, int client_fd) {
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

  if (pc.type == CommandType::kRestart) {
    // Stub: in real embedded deployments you'd interface with systemd/init or a watchdog.
    return write_json_ok(client_fd, "restart requested");
  }

  if (pc.type == CommandType::kThrottle) {
    if (!pc.ok) return write_json_error(client_fd, pc.error ? pc.error : "invalid throttle");
    throttle_ms_.store(pc.throttle_ms, std::memory_order_relaxed);
    cfg_.throttle_ms = pc.throttle_ms;
    return write_json_ok(client_fd, "throttle set");
  }

  return write_json_error(client_fd, "unknown command");
}

Status TcpServer::write_json_metrics(int client_fd, const telemetry::MetricsSnapshot& snap, Status collect_status) {
  char out[512];
  const std::uint32_t throttle = throttle_ms_.load(std::memory_order_relaxed);
  const int n = std::snprintf(
      out, sizeof(out),
      "{\"ok\":%s,\"status_code\":%u,\"platform\":\"%s\",\"temperature_best_effort\":%s,\"ts_ms\":%llu,\"cpu_usage_pct\":%.2f,"
      "\"mem_total_kb\":%llu,\"mem_available_kb\":%llu,\"temperature_c\":%.2f,"
      "\"uptime_s\":%llu,\"throttle_ms\":%u}\n",
      collect_status.ok() ? "true" : "false", static_cast<unsigned>(collect_status.code),
      telemetry::platform_name(),
      telemetry::temperature_best_effort_supported() ? "true" : "false",
      static_cast<unsigned long long>(snap.ts_ms), snap.cpu_usage_pct,
      static_cast<unsigned long long>(snap.mem_total_kb), static_cast<unsigned long long>(snap.mem_available_kb),
      snap.temperature_c, static_cast<unsigned long long>(snap.uptime_s), static_cast<unsigned>(throttle));

  if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(out)) return Status::Internal("response too large");
  const ssize_t w = ::write(client_fd, out, static_cast<std::size_t>(n));
  if (w < 0) return Status::IoError("write() failed");
  return Status::Ok();
}

Status TcpServer::write_json_ok(int client_fd, const char* msg) {
  char out[256];
  const int n = std::snprintf(out, sizeof(out), "{\"ok\":true,\"message\":\"%s\"}\n", msg ? msg : "");
  if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(out)) return Status::Internal("response too large");
  const ssize_t w = ::write(client_fd, out, static_cast<std::size_t>(n));
  if (w < 0) return Status::IoError("write() failed");
  return Status::Ok();
}

Status TcpServer::write_json_error(int client_fd, const char* msg) {
  char out[256];
  const int n = std::snprintf(out, sizeof(out), "{\"ok\":false,\"error\":\"%s\"}\n", msg ? msg : "error");
  if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(out)) return Status::Internal("response too large");
  const ssize_t w = ::write(client_fd, out, static_cast<std::size_t>(n));
  if (w < 0) return Status::IoError("write() failed");
  return Status::Ok();
}

}  // namespace telemetry::net


