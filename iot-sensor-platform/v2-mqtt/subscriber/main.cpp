#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cstring>
#include "influx_writer.h"

using json = nlohmann::json;

static const char*  BROKER_HOST    = "localhost";
static const int    BROKER_PORT    = 1883;
static const char*  SUBSCRIBE_TOPIC = "sensors/#";  // # 接收所有子 Topic

static const char*  INFLUX_HOST   = "localhost";
static const int    INFLUX_PORT   = 8086;
static const char*  INFLUX_ORG    = "iot_org";
static const char*  INFLUX_BUCKET = "sensors";
static const char*  INFLUX_TOKEN  = "mytoken123";

// 把 JSON 封包轉成 InfluxDB Line Protocol（與 v1 邏輯相同）
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
    (void)mosq;  // 未使用，避免 compiler 警告

    if (!msg->payload || msg->payloadlen == 0) return;

    InfluxWriter* writer = static_cast<InfluxWriter*>(userdata);

    std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);
    std::cout << "[收到] topic=" << msg->topic << " → " << payload << "\n";

    try {
        json j = json::parse(payload);
        std::string line = to_line_protocol(j);

        if (writer->write(line)) {
            std::cout << "[DB]   " << line << "\n";
        }
    } catch (const json::exception& e) {
        std::cerr << "[錯誤] JSON parse 失敗：" << e.what() << "\n";
    }
}

// 成功連上 Broker 時呼叫，在這裡發出訂閱請求
static void on_connect(struct mosquitto* mosq, void* userdata, int rc) {
    (void)userdata;
    if (rc != 0) {
        std::cerr << "[Subscriber] 連線被拒，錯誤碼：" << rc << "\n";
        return;
    }
    std::cout << "[Subscriber] 已連線，訂閱 " << SUBSCRIBE_TOPIC << "\n";
    // QoS 0：訂閱端也用 0，與發布端一致
    mosquitto_subscribe(mosq, nullptr, SUBSCRIBE_TOPIC, 0);
}

int main() {
    InfluxWriter writer(INFLUX_HOST, INFLUX_PORT,
                        INFLUX_ORG, INFLUX_BUCKET, INFLUX_TOKEN);

    mosquitto_lib_init();

    // 把 writer 指標當 userdata 傳進去，讓 callback 能存取
    struct mosquitto* mosq = mosquitto_new("iot_subscriber", true, &writer);
    if (!mosq) {
        std::cerr << "[Subscriber] mosquitto_new 失敗\n";
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    int rc = mosquitto_connect(mosq, BROKER_HOST, BROKER_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[Subscriber] 連線失敗：" << mosquitto_strerror(rc) << "\n";
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }

    std::cout << "[Subscriber] 啟動，等待感測器資料...\n";

    // 進入事件迴圈，持續處理網路 I/O 直到程式被中斷
    // 等同於 v1 的 accept loop，但完全由 libmosquitto 管理
    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
