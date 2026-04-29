# Stage 1: Build
# 編譯所有 sensor 執行檔
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    g++ cmake make \
    libmosquitto-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -B /out && cmake --build /out -- -j$(nproc)

# Stage 2: Runtime
# docker-compose 用 build arg SENSOR_TYPE 指定執行哪個 sensor
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libmosquitto1 \
    && rm -rf /var/lib/apt/lists/*

ARG SENSOR_TYPE=temperature_sensor
COPY --from=builder /out/${SENSOR_TYPE} /app/sensor

CMD ["/app/sensor"]
