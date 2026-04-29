# IoT Sensor Platform — v2 MQTT

MQTT-based IoT data collection platform. Simulated sensors publish JSON payloads to a Mosquitto broker. A C++ subscriber consumes the messages and writes to InfluxDB. Grafana displays a live dashboard.

Evolved from [v1-tcp](../v1-tcp/README.md): raw TCP socket replaced with standard MQTT protocol, eliminating manual buffer management and enabling decoupled pub/sub architecture.

## Architecture

```
[temperature_sensor] ─┐
[humidity_sensor]    ─┼──► [Mosquitto Broker :1883] ──► [C++ Subscriber] ──► [InfluxDB :8086]
[gps_sensor]         ─┘         (MQTT Pub/Sub)                                      │
                                                                                     ▼
                                                                             [Grafana :3000]
```

## v1 vs v2 Key Differences

| | v1-tcp | v2-mqtt |
|---|---|---|
| Protocol | Raw TCP + custom JSON+`\n` | MQTT over TCP |
| Connection mgmt | Manual `accept` + `std::thread` per client | Mosquitto Broker handles all clients |
| Message boundary | Manual buffer + `\n` delimiter | MQTT header length field |
| Reconnection | None | libmosquitto built-in |
| Extensibility | Add features by modifying server code | Add independent subscribers per feature |

## Prerequisites

```bash
sudo apt install -y g++ cmake make libcurl4-openssl-dev nlohmann-json3-dev libmosquitto-dev
```

Docker Desktop (or Docker Engine) must be running.

## Quick Start

### 1. Start Mosquitto, InfluxDB and Grafana

```bash
cd v2-mqtt
docker compose up -d
```

Wait about 10 seconds for InfluxDB to initialize, then verify:
- Mosquitto Broker: localhost:1883
- InfluxDB UI:      http://localhost:8086  (admin / adminpassword)
- Grafana:          http://localhost:3000  (admin / admin)

### 2. Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Executables will be at:
- `build/subscriber/subscriber`
- `build/sensors/temperature_sensor`
- `build/sensors/humidity_sensor`
- `build/sensors/gps_sensor`

### 3. Run

Open four terminals:

```bash
# Terminal 1 - Start the subscriber first
./build/subscriber/subscriber

# Terminal 2
./build/sensors/temperature_sensor

# Terminal 3
./build/sensors/humidity_sensor

# Terminal 4
./build/sensors/gps_sensor
```

### 4. View Dashboard

Open Grafana at http://localhost:3000, navigate to **Dashboards > IoT > IoT Sensor Dashboard**.
The dashboard auto-refreshes every 5 seconds.

## MQTT Topic Design

Each sensor publishes to its own topic. The subscriber uses `#` wildcard to receive all:

| Sensor | Topic | QoS |
|---|---|---|
| Temperature | `sensors/temperature` | 0 |
| Humidity | `sensors/humidity` | 0 |
| GPS | `sensors/gps` | 0 |
| Subscriber | `sensors/#` | 0 |

## Packet Format

Each sensor publishes a JSON payload (no `\n` needed — MQTT handles message boundaries):

```json
{"sensor_id":"temp_01","type":"temperature","value":36.5,"timestamp":1711420800}
{"sensor_id":"hum_01","type":"humidity","value":65.2,"timestamp":1711420800}
{"sensor_id":"gps_01","type":"gps","lat":25.0478,"lng":121.5319,"timestamp":1711420800}
```

The subscriber converts each payload to InfluxDB Line Protocol before writing:

```
temperature,sensor_id=temp_01 value=36.500000 1711420800000000000
humidity,sensor_id=hum_01 value=65.200000 1711420800000000000
gps,sensor_id=gps_01 lat=25.047800,lng=121.531900 1711420800000000000
```

## Configuration

Sensor broker settings are in each sensor's `BaseSensor` constructor call (`sensors/*.cpp`).
Subscriber and InfluxDB settings are constants at the top of `subscriber/main.cpp`:

| Constant | Default | Description |
|---|---|---|
| BROKER_HOST | localhost | Mosquitto broker host |
| BROKER_PORT | 1883 | Mosquitto broker port |
| SUBSCRIBE_TOPIC | sensors/# | MQTT topic wildcard |
| INFLUX_HOST | localhost | InfluxDB host |
| INFLUX_PORT | 8086 | InfluxDB port |
| INFLUX_ORG | iot_org | InfluxDB organization |
| INFLUX_BUCKET | sensors | InfluxDB bucket |
| INFLUX_TOKEN | mytoken123 | InfluxDB auth token |

## Stop Services

```bash
docker compose down
```

To also remove stored data:

```bash
docker compose down -v
```
