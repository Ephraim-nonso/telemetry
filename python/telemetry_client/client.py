from __future__ import annotations

import json
import socket
from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class TelemetryClientConfig:
    host: str
    port: int
    timeout_s: float = 1.0
    max_line_bytes: int = 8192


class TelemetryClient:
    def __init__(self, cfg: TelemetryClientConfig):
        self._cfg = cfg

    def _request(self, line: str) -> dict[str, Any]:
        if not line.endswith("\n"):
            line = line + "\n"

        with socket.create_connection((self._cfg.host, self._cfg.port), timeout=self._cfg.timeout_s) as s:
            s.settimeout(self._cfg.timeout_s)
            s.sendall(line.encode("utf-8"))
            raw = self._read_line(s)

        try:
            return json.loads(raw)
        except json.JSONDecodeError as e:
            raise RuntimeError(f"Invalid JSON from agent: {e}: {raw!r}") from e

    def _read_line(self, s: socket.socket) -> str:
        buf = bytearray()
        while True:
            chunk = s.recv(1024)
            if not chunk:
                break
            buf += chunk
            if b"\n" in chunk:
                break
            if len(buf) > self._cfg.max_line_bytes:
                raise RuntimeError("Response too large")

        line = bytes(buf).split(b"\n", 1)[0]
        return line.decode("utf-8", errors="replace")

    def get_metrics(self) -> dict[str, Any]:
        return self._request("GET")

    def ping(self) -> dict[str, Any]:
        return self._request("PING")

    def restart(self) -> dict[str, Any]:
        return self._request("RESTART")

    def throttle(self, ms: int) -> dict[str, Any]:
        if ms < 0:
            raise ValueError("ms must be >= 0")
        return self._request(f"THROTTLE {ms}")


