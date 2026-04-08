#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cstring>

using json = nlohmann::json;

static const char* RABBITMQ_HOST  = "localhost";
static const int   RABBITMQ_PORT  = 5672;
static const char* RABBITMQ_USER  = "guest";
static const char* RABBITMQ_PASS  = "guest";
static const char* RABBITMQ_VHOST = "/";

static const char* EXCHANGE     = "amq.topic";
static const char* QUEUE_NAME   = "alert_queue";

// 只關心溫度和濕度，不需要 GPS
static const char* BINDING_TEMP = "sensors.temperature";
static const char* BINDING_HUM  = "sensors.humidity";

// 警報閾值
static const double TEMP_THRESHOLD = 35.0;   // 超過 35°C 警告
static const double HUM_THRESHOLD  = 80.0;   // 超過 80% 警告

static void check_amqp_reply(amqp_rpc_reply_t reply, const char* context) {
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        std::cerr << "[ALERT_SUB] AMQP 錯誤（" << context << "）\n";
        exit(1);
    }
}

int main() {
    // 1. 建立 TCP 連線
    amqp_connection_state_t conn = amqp_new_connection();
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        std::cerr << "[ALERT_SUB] amqp_tcp_socket_new 失敗\n";
        return 1;
    }

    int status = amqp_socket_open(socket, RABBITMQ_HOST, RABBITMQ_PORT);
    if (status != AMQP_STATUS_OK) {
        std::cerr << "[ALERT_SUB] 無法連線到 RabbitMQ："
                  << RABBITMQ_HOST << ":" << RABBITMQ_PORT << "\n";
        return 1;
    }

    // 2. 登入
    check_amqp_reply(
        amqp_login(conn, RABBITMQ_VHOST, 0, 131072, 0,
                   AMQP_SASL_METHOD_PLAIN, RABBITMQ_USER, RABBITMQ_PASS),
        "login"
    );

    // 3. 開啟 channel
    amqp_channel_open(conn, 1);
    check_amqp_reply(amqp_get_rpc_reply(conn), "open channel");

    // 4. 宣告 Queue
    amqp_queue_declare(
        conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        0, 1, 0, 0,
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "queue declare");

    // 5. 綁定兩個 routing key（溫度 + 濕度）
    // alert_subscriber 不需要 GPS，只看有數值的感測器
    amqp_queue_bind(conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        amqp_cstring_bytes(EXCHANGE),
        amqp_cstring_bytes(BINDING_TEMP),
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "bind temperature");

    amqp_queue_bind(conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        amqp_cstring_bytes(EXCHANGE),
        amqp_cstring_bytes(BINDING_HUM),
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "bind humidity");

    // 6. 開始消費
    amqp_basic_consume(
        conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        amqp_empty_bytes,
        0, 0, 0,
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "basic consume");

    std::cout << "[ALERT_SUB] 等待訊息（binding: "
              << BINDING_TEMP << " + " << BINDING_HUM << "）...\n";

    // 7. Consume loop
    while (true) {
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(conn);

        amqp_rpc_reply_t ret = amqp_consume_message(conn, &envelope, nullptr, 0);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            std::cerr << "[ALERT_SUB] consume 錯誤，退出\n";
            break;
        }

        std::string payload(
            static_cast<char*>(envelope.message.body.bytes),
            envelope.message.body.len
        );

        try {
            json j = json::parse(payload);
            std::string type      = j.at("type").get<std::string>();
            std::string sensor_id = j.at("sensor_id").get<std::string>();
            double value          = j.at("value").get<double>();

            if (type == "temperature" && value > TEMP_THRESHOLD) {
                std::cout << "[ALERT] ⚠  高溫警報！sensor=" << sensor_id
                          << "  溫度=" << value << "°C"
                          << "（閾值 " << TEMP_THRESHOLD << "°C）\n";
            } else if (type == "humidity" && value > HUM_THRESHOLD) {
                std::cout << "[ALERT] ⚠  高濕警報！sensor=" << sensor_id
                          << "  濕度=" << value << "%"
                          << "（閾值 " << HUM_THRESHOLD << "%）\n";
            }
        } catch (const json::exception& e) {
            std::cerr << "[ALERT_SUB] JSON parse 失敗：" << e.what() << "\n";
        }

        amqp_basic_ack(conn, 1, envelope.delivery_tag, 0);
        amqp_destroy_envelope(&envelope);
    }

    // 8. 清理
    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
    return 0;
}
