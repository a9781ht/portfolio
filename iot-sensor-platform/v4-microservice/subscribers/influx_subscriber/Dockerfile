# Stage 1: Build
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    g++ cmake make \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    librabbitmq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -B /out && cmake --build /out -- -j$(nproc)

# Stage 2: Runtime
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libcurl4 \
    librabbitmq4 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /out/influx_subscriber /app/influx_subscriber

CMD ["/app/influx_subscriber"]
