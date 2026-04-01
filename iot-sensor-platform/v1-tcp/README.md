# IoT Sensor Platform — v1 TCP

Multi-client TCP socket IoT data collection platform. Simulated sensors send JSON data to a C++ TCP server, which writes to InfluxDB. Grafana displays a live dashboard.

## Architecture

```
[temperature_sensor] ─┐
[humidity_sensor]    ─┼──► [C++ TCP Server :8888] ──► [InfluxDB :8086]
[gps_sensor]         ─┘                                      │
                                                              ▼
                                                      [Grafana :3000]
```

## Prerequisites

```bash
sudo apt install -y g++ cmake make libcurl4-openssl-dev nlohmann-json3-dev
```

Docker Desktop (or Docker Engine) must be running.

## Quick Start

### 1. Start InfluxDB and Grafana

```bash
cd iot-sensor-platform
docker compose up -d
```

Wait about 10 seconds for InfluxDB to initialize, then verify:
- InfluxDB UI: http://localhost:8086  (admin / adminpassword)
- Grafana:     http://localhost:3000  (admin / admin)

### 2. Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Executables will be at:
- `build/server/server`
- `build/sensors/temperature_sensor`
- `build/sensors/humidity_sensor`
- `build/sensors/gps_sensor`

### 3. Run

Open four terminals:

```bash
# Terminal 1 - Start the server first
./build/server/server

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

## Packet Format

Each sensor sends a JSON line terminated with `\n`:

```json
{"sensor_id":"temp_01","type":"temperature","value":36.5,"timestamp":1711420800}
{"sensor_id":"hum_01","type":"humidity","value":65.2,"timestamp":1711420800}
{"sensor_id":"gps_01","type":"gps","lat":25.0478,"lng":121.5319,"timestamp":1711420800}
```

The server converts each packet to InfluxDB Line Protocol before writing:

```
temperature,sensor_id=temp_01 value=36.500000 1711420800000000000
humidity,sensor_id=hum_01 value=65.200000 1711420800000000000
gps,sensor_id=gps_01 lat=25.047800,lng=121.531900 1711420800000000000
```

## Configuration

All connection settings are constants at the top of `server/main.cpp`:

| Constant | Default | Description |
|---|---|---|
| PORT | 8888 | TCP server port |
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
