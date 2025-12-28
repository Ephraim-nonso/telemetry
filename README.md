# Telemetry (C++ agent + Python client)

This repo is a small embedded-style telemetry system:

- **C++ agent** (`telemetryd`): runs on-device, exposes metrics over a minimal TCP protocol.
- **Python client**: polls the agent, prints a dashboard, and can trigger actions.

## Layout

- `cpp/`: CMake-based C++ daemon
- `python/`: Python CLI client

## C++ (agent)

### Build

```bash
cd telemetry/cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run

```bash
./build/telemetryd --host 0.0.0.0 --port 9000
```

## Python (client)

### Install deps (optional)

```bash
cd telemetry/python
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### Run

```bash
python3 -m telemetry_client.cli --host 127.0.0.1 --port 9000 once
python3 -m telemetry_client.cli --host 127.0.0.1 --port 9000 watch --interval 1.0
```

### Actions

```bash
python3 -m telemetry_client.cli --host 127.0.0.1 --port 9000 restart
python3 -m telemetry_client.cli --host 127.0.0.1 --port 9000 throttle --ms 500
```

## Protocol

Line-based, TCP:

- `GET\n` → returns a single-line JSON document with metrics
- `RESTART\n` → returns `{"ok":true,...}`
- `THROTTLE <ms>\n` → sets agent-side sampling throttle

## Notes

- On **Linux**, metrics are read from `/proc` (CPU/memory/uptime) and `/sys` (temperature, best-effort).
- On **macOS** and **Windows** (Non-linux), CPU/memory/uptime use native APIs.
- On other/unknown OSes, metrics fall back to **simulated** values.

## Docker

### Build + run agent

```bash
cd telemetry
docker build -t telemetry:dev .
docker run --rm -p 9000:9000 telemetry:dev
```

### Run agent + client (dashboard)

```bash
cd telemetry
docker compose up --build
```
