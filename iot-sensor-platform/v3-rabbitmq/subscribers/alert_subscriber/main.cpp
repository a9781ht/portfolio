#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cstring>

using json = nlohmann::json;

static const char* RABBITMQ_HOST  = "localhost";
static const int   RABBITMQ_PORT  = 5672;               // RabbitMQ 的埠號
static const char* RABBITMQ_USER  = "guest";            // RabbitMQ 的使用者名稱
static const char* RABBITMQ_PASS  = "guest";            // RabbitMQ 的密碼
static const char* RABBITMQ_VHOST = "/";                // RabbitMQ 的 vhost

static const char* EXCHANGE       = "amq.topic";        // RabbitMQ 的交換器
static const char* QUEUE_NAME     = "alert_queue";      // RabbitMQ 的佇列

// 只關心溫度和濕度，不需要 GPS
static const char* BINDING_TEMP   = "sensors.temperature";
static const char* BINDING_HUM    = "sensors.humidity";

// 警報閾值
static const double TEMP_THRESHOLD = 35.0;   // 超過 35°C 警告
static const double HUM_THRESHOLD  = 80.0;   // 超過 80% 警告

// 檢查 AMQP 回應是否有錯，有錯就印出並結束程式
static void check_amqp_reply(amqp_rpc_reply_t reply, const char* context) {
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        std::cerr << "[ALERT_SUB] AMQP 錯誤（" << context << "）\n";
        exit(1);
    }
}

int main() {
    // 1. 建立 TCP 連線到 RabbitMQ
    amqp_connection_state_t conn = amqp_new_connection();        // 建立 RabbitMQ 連線物件本體 AMQP connection state
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);           // 在那個 connection 上建立並綁定一個 TCP socket transport
    if (!socket) {
        std::cerr << "[ALERT_SUB] amqp_tcp_socket_new 失敗\n";
        return 1;
    }

    int status = amqp_socket_open(socket, RABBITMQ_HOST, RABBITMQ_PORT);   // 用你先前建立好的 amqp_socket_t，對 broker 做實際的 TCP 連線，連到指定的 host 與 port
    if (status != AMQP_STATUS_OK) {
        std::cerr << "[ALERT_SUB] 無法連線到 RabbitMQ："
                  << RABBITMQ_HOST << ":" << RABBITMQ_PORT << "\n";
        return 1;
    }

    // 2. 登入AMQP
    check_amqp_reply(
        amqp_login(conn, RABBITMQ_VHOST, 0, 131072, 0,                     // channel_max 為 0，表示不限制 channel 數量，交給伺服器預設。frame_max 為 131072，表示最大 frame 大小為 131072 bytes。heartbeat 為 0，表示不使用心跳機制，交給伺服器決定
                   AMQP_SASL_METHOD_PLAIN, RABBITMQ_USER, RABBITMQ_PASS),  // 使用 SASL 方法進行登入，RABBITMQ_USER 為使用者名稱，RABBITMQ_PASS 為密碼
        "login"
    );

    // 3. 開啟 channel
    amqp_channel_open(conn, 1);                                            // 開啟 channel，1 表示 channel ID
    check_amqp_reply(amqp_get_rpc_reply(conn), "open channel");

    // 4. 宣告 Queue
    amqp_queue_declare(
        conn, 1,                                                           // channel ID
        amqp_cstring_bytes(QUEUE_NAME),                                    // queue name
        0,                                                                 // passive=0 (檢查佇列是否存在，1為只檢查不建立，0為不存在就建立)
        1,                                                                 // durable=1 (持久化，即使 RabbitMQ 重啟，佇列也不會消失)
        0,                                                                 // exclusive=0 (是否排他，0為不排他，可以有其他消費者連接這個佇列，1為排他，只能有一個消費者連接這個佇列)
        0,                                                                 // auto_delete=0 (當最後一個 consumer 取消訂閱後，佇列是否刪除，1為自動刪除，0為不自動刪除，即使沒有消費者連接這個佇列)                                                      // passive=0 (檢查佇列是否存在，1為只檢查不建立，0為不存在就建立), durable=1 (持久化，即使 RabbitMQ 重啟，佇列也不會消失), exclusive=0 (是否排他，0為不排他，可以有其他消費者連接這個佇列，1為排他，只能有一個消費者連接這個佇列), auto_delete=0 (當最後一個 consumer 取消訂閱後，佇列是否刪除，1為自動刪除，0為不自動刪除，即使沒有消費者連接這個佇列)
        amqp_empty_table                                                   // 額外參數表，沒有就傳 amqp_empty_table，表示沒有其他設定
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "queue declare");

    // 5. 綁定Queue
    amqp_queue_bind(
        conn, 1,                                                           // channel ID
        amqp_cstring_bytes(QUEUE_NAME),                                    // queue name
        amqp_cstring_bytes(EXCHANGE),                                      // exchange name，為 amq.topic，表示使用 Topic Exchange
        amqp_cstring_bytes(BINDING_TEMP),                                  // routing key，為 sensors.temperature，表示溫度 sensor 資料
        amqp_empty_table                                                   // 額外參數表，沒有就傳 amqp_empty_table，表示沒有其他設定
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "bind temperature");

    amqp_queue_bind(
        conn, 1,                                                           // channel ID
        amqp_cstring_bytes(QUEUE_NAME),                                    // queue name
        amqp_cstring_bytes(EXCHANGE),                                      // exchange name，為 amq.topic，表示使用 Topic Exchange
        amqp_cstring_bytes(BINDING_HUM),                                   // routing key，為 sensors.humidity，表示濕度 sensor 資料
        amqp_empty_table                                                   // 額外參數表，沒有就傳 amqp_empty_table，表示沒有其他設定
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "bind humidity");

    // 6. 開始消費
    amqp_basic_consume(
        conn, 1,                                                           // channel ID
        amqp_cstring_bytes(QUEUE_NAME),                                    // queue name
        amqp_empty_bytes,                                                  // 消費者標籤，用 amqp_empty_bytes 通常表示交給 broker 自動產生
        0,                                                                 // no_local，設為 0 表示同連線發佲的不轉給自己
        0,                                                                 // no_ack，設為 0 表示手動 ACK，須自己 ACK 才會從佇列移，確保訊息不遺失。設為 1 表示自動 ACK，收到訊息就會自動從佇列移除
        0,                                                                 // exclusive，設為 0 表示不排他，可以有其他消費者連接這個佇列，設為 1 表示排他，只能有一個消費者連接這個佇列
        amqp_empty_table                                                   // 額外參數表，沒有就傳 amqp_empty_table，表示沒有其他設定
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "basic consume");

    std::cout << "[ALERT_SUB] 等待訊息（binding: "
              << BINDING_TEMP << " + " << BINDING_HUM << "）...\n";

    // 7. Consume loop
    while (true) {
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(conn);                                          // 釋放 buffers，避免記憶體洩漏

        amqp_rpc_reply_t ret = amqp_consume_message(conn, &envelope, nullptr, 0);  // 消費訊息，conn 為 RabbitMQ 連線物件。&envelope 為訊息容器。&timeout 為最多等多久才回傳，nullptr 表示一直等到有訊息。flags 保留或擴充用，傳 0 表示不使用
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            std::cerr << "[ALERT_SUB] consume 錯誤，退出\n";
            break;
        }

        std::string payload(                                                       // 取出 payload 字串
            static_cast<char*>(envelope.message.body.bytes),                       // envelope.message.body.bytes 為訊息內容，envelope.message.body.len 為訊息長度
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

        amqp_basic_ack(conn, 1, envelope.delivery_tag, 0);                         // 手動 ACK，告知 RabbitMQ 訊息已處理完畢
        amqp_destroy_envelope(&envelope);                                          // 釋放訊息容器，避免記憶體洩漏
    }

    // 8. 清理
    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
    return 0;
}
