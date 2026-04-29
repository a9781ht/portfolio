#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cstring>
#include "influx_writer.h"

using json = nlohmann::json;

static const char*  BROKER_HOST     = "localhost";
static const int    BROKER_PORT     = 1883;                 // broker 的埠號
static const char*  SUBSCRIBE_TOPIC = "sensors/#";          // 訂閱 topic，# 表示所有子主題

static const char*  INFLUX_HOST     = "localhost";
static const int    INFLUX_PORT     = 8086;
static const char*  INFLUX_ORG      = "iot_org";
static const char*  INFLUX_BUCKET   = "sensors";
static const char*  INFLUX_TOKEN    = "mytoken123";

// 把 JSON 封包轉成 InfluxDB Line Protocol
static std::string to_line_protocol(const json& j) {
    std::string sensor_id = j.at("sensor_id").get<std::string>();
    std::string type      = j.at("type").get<std::string>();
    long long ts_ns = j.at("timestamp").get<long long>() * 1000000000LL;

    std::string line = type + ",sensor_id=" + sensor_id + " ";

    if (type == "gps") {
        double lat = j.at("lat").get<double>();
        double lng = j.at("lng").get<double>();
        line += "lat=" + std::to_string(lat) + ",lng=" + std::to_string(lng);
    } else {
        double value = j.at("value").get<double>();
        line += "value=" + std::to_string(value);
    }

    line += " " + std::to_string(ts_ns);
    return line;
}

// Mosquitto 在收到訊息時呼叫這個 callback
// userdata 是我們在 mosquitto_new 時傳入的指標，這裡存放 InfluxWriter*
static void on_message(struct mosquitto* mosq,
                       void* userdata,
                       const struct mosquitto_message* msg)
{
    (void)mosq;

    if (!msg->payload || msg->payloadlen == 0) return;

    InfluxWriter* writer = static_cast<InfluxWriter*>(userdata);
    std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);

    try {
        json j = json::parse(payload);
        std::string line = to_line_protocol(j);
        if (!writer->write(line)) {
            std::cerr << "[SUB] InfluxDB 寫入失敗\n";
        }
    } catch (const json::exception& e) {
        std::cerr << "[錯誤] JSON parse 失敗：" << e.what() << "\n";
    }
}

// 成功連上 Broker 時呼叫，在這裡發出訂閱請求
static void on_connect(struct mosquitto* mosq, void* userdata, int rc) {
    (void)userdata;
    if (rc != 0) {
        std::cerr << "[SUB] 連線被拒，錯誤碼：" << rc << "\n";
        return;
    }
    mosquitto_subscribe(mosq, nullptr, SUBSCRIBE_TOPIC, 0);                    // 訂閱 topic，mosq 為 mosquitto 的物件，nullptr 表示不使用 retain flag，SUBSCRIBE_TOPIC 為 topic 的名稱，0 表示 QoS 0
}

int main() {
    InfluxWriter writer(INFLUX_HOST, INFLUX_PORT,
                        INFLUX_ORG, INFLUX_BUCKET, INFLUX_TOKEN);

    mosquitto_lib_init();                                                      // 初始化 mosquitto 的記憶體

    struct mosquitto* mosq = mosquitto_new("iot_subscriber", true, &writer);   // 建立 mosquitto 的物件，iot_subscriber 為 subscriber 的 ID，true 表示使用 persistent 連線，&writer 表示 userdata
    if (!mosq) {
        std::cerr << "[SUB] mosquitto_new 失敗\n";
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);                          // 設定連線 callback
    mosquitto_message_callback_set(mosq, on_message);                          // 設定訊息 callback

    int rc = mosquitto_connect(mosq, BROKER_HOST, BROKER_PORT, 60);            // 連線到 broker，mosq 為 mosquitto 的物件，BROKER_HOST 為 broker 的 IP 地址，BROKER_PORT 為 broker 的埠號，60 表示超時時間為 60 秒
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[SUB] 連線失敗：" << mosquitto_strerror(rc) << "\n";
        mosquitto_destroy(mosq);                                               // 釋放 mosquitto 的記憶體
        mosquitto_lib_cleanup();                                               // 釋放 mosquitto 的記憶體
        return 1;
    }

    mosquitto_loop_forever(mosq, -1, 1);                                       // 進入無限迴圈，等待訊息，mosq 為 mosquitto 的物件，-1 表示無限等待，1 表示只處理一個事件

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
