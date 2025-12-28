from __future__ import annotations

import argparse
import time
from typing import Any

from rich.console import Console
from rich.live import Live
from rich.table import Table

from .client import TelemetryClient, TelemetryClientConfig


def _metrics_table(m: dict[str, Any]) -> Table:
    t = Table(title="Telemetry")
    t.add_column("Key", style="bold")
    t.add_column("Value")

    def add(key: str) -> None:
        t.add_row(key, str(m.get(key)))

    add("ok")
    add("status_code")
    add("platform")
    add("temperature_best_effort")
    add("ts_ms")
    add("cpu_usage_pct")
    add("mem_total_kb")
    add("mem_available_kb")
    add("temperature_c")
    add("uptime_s")
    add("throttle_ms")
    if not m.get("ok", True):
        add("error")
    return t


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="telemetry_client")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", default=9000, type=int)
    p.add_argument("--timeout", default=1.0, type=float)

    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("once", help="Fetch metrics once")

    watch = sub.add_parser("watch", help="Continuously fetch and display metrics")
    watch.add_argument("--interval", default=1.0, type=float)

    sub.add_parser("restart", help="Request a service restart (stub)")

    throttle = sub.add_parser("throttle", help="Set agent throttle (ms)")
    throttle.add_argument("--ms", required=True, type=int)

    args = p.parse_args(argv)
    console = Console()

    client = TelemetryClient(TelemetryClientConfig(host=args.host, port=args.port, timeout_s=args.timeout))

    if args.cmd == "once":
        m = client.get_metrics()
        console.print(_metrics_table(m))
        return 0 if m.get("ok", True) else 1

    if args.cmd == "watch":
        interval = float(args.interval)
        with Live(_metrics_table({"ok": True}), refresh_per_second=4, console=console) as live:
            try:
                while True:
                    m = client.get_metrics()
                    live.update(_metrics_table(m))
                    time.sleep(max(0.05, interval))
            except KeyboardInterrupt:
                return 0

    if args.cmd == "restart":
        r = client.restart()
        console.print(r)
        return 0 if r.get("ok", True) else 1

    if args.cmd == "throttle":
        r = client.throttle(int(args.ms))
        console.print(r)
        return 0 if r.get("ok", True) else 1

    console.print("[red]Unknown command[/red]")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())


