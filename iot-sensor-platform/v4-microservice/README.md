# IoT Sensor Platform — v4 Microservice

Full microservice architecture built on top of V3. Every component runs as an independent Docker container. Three subscribers are written in different languages (C++ / Python / Go) to demonstrate language interoperability through a shared message broker. A gRPC call between `alert_subscriber` and `notification_service` demonstrates synchronous service-to-service communication alongside the asynchronous AMQP messaging layer.

Evolved from [v3-rabbitmq](../v3-rabbitmq/README.md): all processes containerized, subscribers split into separate images, gRPC added as a second communication protocol.

## Architecture

```
[temperature_sensor C++] ─┐
[humidity_sensor    C++] ─┼──MQTT──► [RabbitMQ MQTT Plugin :1883]
[gps_sensor         C++] ─┘                    │
                                          AMQP internally
                                     [amq.topic Exchange]
                                    /         |          \
                             sensors.#  sensors.temp    sensors.#
                                │       sensors.hum        │
                                ▼             │             ▼
                      [influx_queue]    [alert_queue]  [logger_queue]
                             │                │               │
                      [influx_subscriber] [alert_subscriber] [logger_subscriber]
                          C++               Python              Go
                             │                │
                          InfluxDB     gRPC :50051
                             │                │
                          Grafana    [notification_service]
                                          Python
                                             │
                                        Slack Webhook
```

## v3 vs v4 Key Differences

| | v3-rabbitmq | v4-microservice |
|---|---|---|
| Deployment | Manual terminal per process | `docker compose up` — all services |
| Sensor code | localhost (host machine) | Containerized, connects to `rabbitmq` |
| alert_subscriber | C++ (rabbitmq-c) | Python (pika) |
| logger_subscriber | C++ (rabbitmq-c) | Go (amqp091-go) |
| Alert delivery | stdout only | gRPC → notification_service → Slack |
| Communication protocols | MQTT + AMQP | MQTT + AMQP + gRPC |
| Scaling | N/A | `docker compose scale influx_subscriber=3` |

## Service Communication Protocols

| Between | Protocol | Mode |
|---|---|---|
| Sensor → RabbitMQ | MQTT | Async (fire-and-forget) |
| RabbitMQ → Subscribers | AMQP | Async (durable queue + manual ACK) |
| alert_subscriber → notification_service | gRPC | Sync (waits for response) |
| notification_service → Slack | HTTPS REST | Sync |

## gRPC Design

`alert.proto` defines a single RPC call shared by both services:

```protobuf
service NotificationService {
    rpc SendAlert (AlertRequest) returns (AlertResponse);
}
message AlertRequest {
    string sensor_id = 1;
    string type      = 2;
    double value     = 3;
    string message   = 4;
}
message AlertResponse {
    bool success = 1;
}
```

Python stubs (`alert_pb2.py`, `alert_pb2_grpc.py`) are generated inside the Dockerfile during `docker build` — no local `grpc_tools` installation required.

## Language Choice per Service

| Service | Language | Reason |
|---|---|---|
| sensors | C++ | Performance-sensitive, low overhead per message |
| influx_subscriber | C++ | Ported directly from v3, high-throughput writes |
| alert_subscriber | Python | Simple threshold logic, easy gRPC client wiring |
| logger_subscriber | Go | Efficient file I/O, minimal memory footprint, fast compile |
| notification_service | Python | gRPC server + HTTP in minimal code |

## Prerequisites

Docker Desktop (or Docker Engine + Compose plugin) must be running. No local compilers or language runtimes needed — everything builds inside Docker.

## Quick Start

### 1. (Optional) Set Slack Webhook URL

If you have a Slack Incoming Webhook URL, create a `.env` file:

```bash
echo "SLACK_WEBHOOK_URL=https://hooks.slack.com/services/YOUR/WEBHOOK/URL" > .env
```

Without this, alerts are printed to the `notification_service` container log instead of being sent to Slack.

### 2. Start all services

```bash
cd v4-microservice
docker compose up --build
```

First build takes 3–5 minutes (compiling C++ layers). Subsequent builds use Docker layer cache.

### 3. Verify services

- RabbitMQ Management UI: http://localhost:15672  (guest / guest)
- InfluxDB UI:            http://localhost:8086   (admin / adminpassword)
- Grafana:                http://localhost:3000   (admin / admin)

### 4. Observe

- **Grafana**: Dashboards > IoT > IoT Sensor Dashboard (auto-refresh 5s)
- **Alert logs**: `docker compose logs -f alert_subscriber notification_service`
- **Sensor log file**: `docker compose exec logger_subscriber cat /logs/sensor_log.txt`
- **RabbitMQ Queues**: http://localhost:15672/#/queues

### 5. Scale a subscriber

```bash
# Start 3 influx_subscriber instances — RabbitMQ round-robins messages across them
docker compose up --scale influx_subscriber=3 --no-recreate
```

## Alert Thresholds

| Sensor | Threshold | Action |
|---|---|---|
| temperature | > 35.0 °C | gRPC SendAlert → Slack (or log) |
| humidity | > 80.0 % | gRPC SendAlert → Slack (or log) |

## Configuration

All settings are environment variables in `docker-compose.yml`. Key overrides:

| Variable | Default | Service |
|---|---|---|
| `SLACK_WEBHOOK_URL` | (empty, log only) | notification_service |
| `RABBITMQ_HOST` | `rabbitmq` | alert_subscriber, logger_subscriber |
| `NOTIFICATION_HOST` | `notification_service` | alert_subscriber |
| `LOG_FILE` | `/logs/sensor_log.txt` | logger_subscriber |

## Stop Services

```bash
docker compose down
```

To also remove stored data:

```bash
docker compose down -v
```
