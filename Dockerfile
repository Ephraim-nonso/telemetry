FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY cpp/ /src/cpp/

RUN cmake -S /src/cpp -B /src/cpp/build -DCMAKE_BUILD_TYPE=Release -DTELEMETRY_BUILD_TESTS=OFF \
 && cmake --build /src/cpp/build -j


FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-pip \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/cpp/build/telemetryd /app/telemetryd
COPY python/ /app/python/

RUN pip3 install --no-cache-dir -r /app/python/requirements.txt

ENV PYTHONPATH=/app/python

EXPOSE 9000

# Default: run the agent. docker-compose can override this command for the client container.
CMD ["/app/telemetryd", "--host", "0.0.0.0", "--port", "9000", "--throttle-ms", "250"]


