#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cstring>
#include "../common/influx_writer.h"

using json = nlohmann::json;

static const char* RABBITMQ_HOST   = "localhost";
static const int   RABBITMQ_PORT   = 5672;
static const char* RABBITMQ_USER   = "guest";
static const char* RABBITMQ_PASS   = "guest";
static const char* RABBITMQ_VHOST  = "/";

static const char* EXCHANGE        = "amq.topic";
static const char* QUEUE_NAME      = "influx_queue";
static const char* BINDING_KEY     = "sensors.#";   // 訂閱所有 sensor 資料

static const char* INFLUX_HOST   = "localhost";
static const int   INFLUX_PORT   = 8086;
static const char* INFLUX_ORG    = "iot_org";
static const char* INFLUX_BUCKET = "sensors";
static const char* INFLUX_TOKEN  = "mytoken123";

// 把 JSON 轉成 InfluxDB Line Protocol
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

// 檢查 AMQP 回應是否有錯，有錯就印出並結束程式
static void check_amqp_reply(amqp_rpc_reply_t reply, const char* context) {
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        std::cerr << "[INFLUX_SUB] AMQP 錯誤（" << context << "）\n";
        exit(1);
    }
}

int main() {
    InfluxWriter writer(INFLUX_HOST, INFLUX_PORT,
                        INFLUX_ORG, INFLUX_BUCKET, INFLUX_TOKEN);

    // 1. 建立 TCP 連線到 RabbitMQ
    amqp_connection_state_t conn = amqp_new_connection();
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        std::cerr << "[INFLUX_SUB] amqp_tcp_socket_new 失敗\n";
        return 1;
    }

    int status = amqp_socket_open(socket, RABBITMQ_HOST, RABBITMQ_PORT);
    if (status != AMQP_STATUS_OK) {
        std::cerr << "[INFLUX_SUB] 無法連線到 RabbitMQ："
                  << RABBITMQ_HOST << ":" << RABBITMQ_PORT << "\n";
        return 1;
    }

    // 2. AMQP 登入
    check_amqp_reply(
        amqp_login(conn, RABBITMQ_VHOST, 0, 131072, 0,
                   AMQP_SASL_METHOD_PLAIN, RABBITMQ_USER, RABBITMQ_PASS),
        "login"
    );

    // 3. 開啟 channel 1
    amqp_channel_open(conn, 1);
    check_amqp_reply(amqp_get_rpc_reply(conn), "open channel");

    // 4. 宣告 Queue（passive=0, durable=1, exclusive=0, auto_delete=0）
    amqp_queue_declare_ok_t* q = amqp_queue_declare(
        conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        0, 1, 0, 0,
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "queue declare");
    (void)q;

    // 5. 把 Queue 綁定到 amq.topic exchange，routing key = sensors.#
    amqp_queue_bind(
        conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        amqp_cstring_bytes(EXCHANGE),
        amqp_cstring_bytes(BINDING_KEY),
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "queue bind");

    // 6. 開始消費（no_ack=0，手動 ACK 確保訊息不遺失）
    amqp_basic_consume(
        conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        amqp_empty_bytes,   // consumer_tag（空 = 自動產生）
        0,                  // no_local
        0,                  // no_ack（手動 ACK）
        0,                  // exclusive
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "basic consume");

    std::cout << "[INFLUX_SUB] 等待訊息（binding: " << BINDING_KEY << "）...\n";

    // 7. Consume loop（類比 mosquitto_loop_forever）
    while (true) {
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(conn);

        amqp_rpc_reply_t ret = amqp_consume_message(conn, &envelope, nullptr, 0);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            std::cerr << "[INFLUX_SUB] consume 錯誤，退出\n";
            break;
        }

        // 取出 payload 字串
        std::string payload(
            static_cast<char*>(envelope.message.body.bytes),
            envelope.message.body.len
        );

        try {
            json j = json::parse(payload);
            std::string line = to_line_protocol(j);
            if (!writer.write(line)) {
                std::cerr << "[INFLUX_SUB] InfluxDB 寫入失敗\n";
            }
        } catch (const json::exception& e) {
            std::cerr << "[INFLUX_SUB] JSON parse 失敗：" << e.what() << "\n";
        }

        // 手動 ACK：告知 RabbitMQ 訊息已處理完畢
        amqp_basic_ack(conn, 1, envelope.delivery_tag, 0);
        amqp_destroy_envelope(&envelope);
    }

    // 8. 清理
    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
    return 0;
}
