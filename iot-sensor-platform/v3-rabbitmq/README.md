# IoT Sensor Platform — v3 RabbitMQ

RabbitMQ-based IoT data collection platform with multiple independent subscribers. Sensors publish JSON via MQTT to RabbitMQ's MQTT plugin. RabbitMQ internally routes each message through a Topic Exchange to three separate AMQP queues, each consumed by an independent C++ subscriber.

Evolved from [v2-mqtt](../v2-mqtt/README.md): Mosquitto replaced with RabbitMQ, single subscriber split into three independent AMQP consumers, demonstrating RabbitMQ's routing, queue isolation, and message ACK capabilities.

## Architecture

```
[temperature_sensor] ─┐  MQTT(libmosquitto)
[humidity_sensor]    ─┼──► [RabbitMQ :1883]
[gps_sensor]         ─┘   MQTT Plugin
                              │
                              ▼ AMQP internally
                         [amq.topic Exchange]
                        /         |          \
               sensors.#   sensors.temperature  sensors.#
                  │         sensors.humidity        │
                  ▼               │                 ▼
           [influx_queue]         ▼          [logger_queue]
                  │         [alert_queue]          │
                  ▼               │                ▼
     [influx_subscriber]          ▼    [logger_subscriber]
           → InfluxDB    [alert_subscriber]   → sensor_log.txt
                           → stdout alerts
```

## v2 vs v3 Key Differences

| | v2-mqtt | v3-rabbitmq |
|---|---|---|
| Broker | Mosquitto (MQTT only) | RabbitMQ (MQTT + AMQP) |
| Subscriber count | 1 (influx only) | 3 independent subscribers |
| Subscriber protocol | libmosquitto (MQTT) | rabbitmq-c (AMQP) |
| Message routing | Topic wildcard `#` at subscriber | Exchange routes by routing key |
| Queue isolation | None (single consumer) | Each subscriber has own Queue |
| Message ACK | QoS 0 (fire-and-forget) | Manual AMQP ACK after processing |
| Crash resilience | Lost if subscriber is down | Messages buffered in Queue |
| Observability | None | Management UI at :15672 |

## MQTT Topic → AMQP Routing Key Conversion

RabbitMQ's MQTT Plugin automatically converts `/` to `.` in topic names:

| Sensor MQTT Topic | AMQP Routing Key |
|---|---|
| `sensors/temperature` | `sensors.temperature` |
| `sensors/humidity` | `sensors.humidity` |
| `sensors/gps` | `sensors.gps` |

Sensor code is **unchanged** from v2 — only the broker host (`localhost` → `rabbitmq`) differs.

## Three Subscribers

| Subscriber | Queue | Binding Key(s) | Action |
|---|---|---|---|
| `influx_subscriber` | `influx_queue` | `sensors.#` | Parse JSON → InfluxDB Line Protocol → write to InfluxDB |
| `alert_subscriber` | `alert_queue` | `sensors.temperature`<br>`sensors.humidity` | Check thresholds → print warning to stdout |
| `logger_subscriber` | `logger_queue` | `sensors.#` | Append raw JSON + ISO 8601 timestamp to `sensor_log.txt` |

Alert thresholds: temperature > 35°C, humidity > 80%.

## Exchange / Queue Design

All queues bind to the built-in `amq.topic` exchange (Topic Exchange). RabbitMQ matches routing keys using `.` as separator and `#` as multi-word wildcard.

```
amq.topic  (Topic Exchange)
    ├── sensors.#        → influx_queue  (receives all sensors)
    ├── sensors.#        → logger_queue  (receives all sensors)
    ├── sensors.temperature → alert_queue
    └── sensors.humidity    → alert_queue
```

One `sensors.temperature` message is delivered to **all three queues simultaneously** — each subscriber processes it independently.

## Prerequisites

```bash
sudo apt install -y \
    g++ cmake make \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libmosquitto-dev \
    librabbitmq-dev
```

Docker Desktop (or Docker Engine) must be running.

## Quick Start

### 1. Start RabbitMQ, InfluxDB and Grafana

```bash
cd v3-rabbitmq
docker compose up -d
```

Wait about 15 seconds for all services to initialize, then verify:
- RabbitMQ Management UI: http://localhost:15672  (guest / guest)
- InfluxDB UI:            http://localhost:8086   (admin / adminpassword)
- Grafana:                http://localhost:3000   (admin / admin)

### 2. Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Executables will be at:
- `build/subscribers/influx_subscriber`
- `build/subscribers/alert_subscriber`
- `build/subscribers/logger_subscriber`
- `build/sensors/temperature_sensor`
- `build/sensors/humidity_sensor`
- `build/sensors/gps_sensor`

### 3. Run

Open six terminals:

```bash
# Terminal 1 - InfluxDB subscriber
./build/subscribers/influx_subscriber

# Terminal 2 - Alert subscriber
./build/subscribers/alert_subscriber

# Terminal 3 - Logger subscriber
./build/subscribers/logger_subscriber

# Terminal 4
./build/sensors/temperature_sensor

# Terminal 5
./build/sensors/humidity_sensor

# Terminal 6
./build/sensors/gps_sensor
```

### 4. Observe

- **Grafana**: http://localhost:3000 → Dashboards > IoT > IoT Sensor Dashboard (auto-refresh 5s)
- **Alert subscriber**: watch for `[ALERT] ⚠` lines in Terminal 2
- **Logger subscriber**: `tail -f sensor_log.txt` to see timestamped raw JSON
- **RabbitMQ Management**: http://localhost:15672 → Queues tab to see message rates and queue depths

## Packet Format

Same JSON format as v2 (sensor code is unchanged):

```json
{"sensor_id":"temp_01","type":"temperature","value":36.5,"timestamp":1711420800}
{"sensor_id":"hum_01","type":"humidity","value":65.2,"timestamp":1711420800}
{"sensor_id":"gps_01","type":"gps","lat":25.0478,"lng":121.5319,"timestamp":1711420800}
```

## AMQP Connection Pattern

Each subscriber follows the same skeleton:

```
amqp_new_connection()         ← allocate connection handle
amqp_tcp_socket_new()         ← create TCP socket
amqp_socket_open()            ← TCP connect to RabbitMQ :5672
amqp_login()                  ← AMQP authentication
amqp_channel_open()           ← open channel (logical multiplexer)
amqp_queue_declare()          ← create durable queue
amqp_queue_bind()             ← bind queue to amq.topic with routing key
amqp_basic_consume()          ← register consumer
while(true)
    amqp_consume_message()    ← blocking wait for next message
    process(envelope)         ← application logic
    amqp_basic_ack()          ← acknowledge to RabbitMQ
    amqp_destroy_envelope()   ← free memory
```

## Configuration

Sensor broker settings are in each sensor's constructor call (`sensors/*.cpp`).
Subscriber and service settings are constants at the top of each `subscribers/*/main.cpp`:

| Constant | Default | Description |
|---|---|---|
| RABBITMQ_HOST | localhost | RabbitMQ host |
| RABBITMQ_PORT | 5672 | AMQP port |
| RABBITMQ_USER | guest | RabbitMQ username |
| RABBITMQ_PASS | guest | RabbitMQ password |
| INFLUX_HOST | localhost | InfluxDB host (influx_subscriber only) |
| INFLUX_TOKEN | mytoken123 | InfluxDB auth token |
| TEMP_THRESHOLD | 35.0 | Alert threshold °C (alert_subscriber) |
| HUM_THRESHOLD | 80.0 | Alert threshold % (alert_subscriber) |
| LOG_FILE | sensor_log.txt | Output log path (logger_subscriber) |

## Stop Services

```bash
docker compose down
```

To also remove stored data:

```bash
docker compose down -v
```
