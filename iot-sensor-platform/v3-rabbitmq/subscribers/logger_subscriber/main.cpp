#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
#include <chrono>

static const char* RABBITMQ_HOST  = "localhost";
static const int   RABBITMQ_PORT  = 5672;
static const char* RABBITMQ_USER  = "guest";
static const char* RABBITMQ_PASS  = "guest";
static const char* RABBITMQ_VHOST = "/";

static const char* EXCHANGE    = "amq.topic";
static const char* QUEUE_NAME  = "logger_queue";
static const char* BINDING_KEY = "sensors.#";   // 訂閱所有 sensor 資料

static const char* LOG_FILE = "sensor_log.txt";

static void check_amqp_reply(amqp_rpc_reply_t reply, const char* context) {
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        std::cerr << "[LOGGER_SUB] AMQP 錯誤（" << context << "）\n";
        exit(1);
    }
}

// 取得目前時間的 ISO 8601 格式字串（含毫秒）
static std::string now_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    char result[40];
    snprintf(result, sizeof(result), "%s.%03lldZ", buf,
             static_cast<long long>(ms.count()));
    return result;
}

int main() {
    // 1. 建立 TCP 連線
    amqp_connection_state_t conn = amqp_new_connection();
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        std::cerr << "[LOGGER_SUB] amqp_tcp_socket_new 失敗\n";
        return 1;
    }

    int status = amqp_socket_open(socket, RABBITMQ_HOST, RABBITMQ_PORT);
    if (status != AMQP_STATUS_OK) {
        std::cerr << "[LOGGER_SUB] 無法連線到 RabbitMQ："
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

    // 5. 綁定到 amq.topic，接收所有 sensor 訊息
    amqp_queue_bind(
        conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        amqp_cstring_bytes(EXCHANGE),
        amqp_cstring_bytes(BINDING_KEY),
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "queue bind");

    // 6. 開始消費
    amqp_basic_consume(
        conn, 1,
        amqp_cstring_bytes(QUEUE_NAME),
        amqp_empty_bytes,
        0, 0, 0,
        amqp_empty_table
    );
    check_amqp_reply(amqp_get_rpc_reply(conn), "basic consume");

    // 開啟 log 檔（追加模式）
    std::ofstream log_file(LOG_FILE, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "[LOGGER_SUB] 無法開啟 " << LOG_FILE << "\n";
        return 1;
    }

    std::cout << "[LOGGER_SUB] 等待訊息（binding: " << BINDING_KEY
              << "），寫入 " << LOG_FILE << " ...\n";

    // 7. Consume loop
    while (true) {
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(conn);

        amqp_rpc_reply_t ret = amqp_consume_message(conn, &envelope, nullptr, 0);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            std::cerr << "[LOGGER_SUB] consume 錯誤，退出\n";
            break;
        }

        std::string payload(
            static_cast<char*>(envelope.message.body.bytes),
            envelope.message.body.len
        );

        // 寫入格式：[時間戳] 原始 JSON
        log_file << "[" << now_iso8601() << "] " << payload << "\n";
        log_file.flush();

        amqp_basic_ack(conn, 1, envelope.delivery_tag, 0);
        amqp_destroy_envelope(&envelope);
    }

    // 8. 清理
    log_file.close();
    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
    return 0;
}
