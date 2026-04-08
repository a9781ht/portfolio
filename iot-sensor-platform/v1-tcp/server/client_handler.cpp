#include "client_handler.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 把 JSON 封包轉成 InfluxDB Line Protocol 格式
// 溫度/濕度：measurement,sensor_id=xxx value=36.5 timestamp_ns
// GPS：      gps,sensor_id=xxx lat=25.0,lng=121.5 timestamp_ns
static std::string to_line_protocol(const json& j) {
    std::string sensor_id = j.at("sensor_id").get<std::string>();
    std::string type      = j.at("type").get<std::string>();
    // Unix timestamp（秒）轉成奈秒，InfluxDB precision=ns
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

void handle_client(int client_fd, InfluxWriter* writer) {
    std::cout << "[連線] fd=" << client_fd << "\n";

    std::string buffer;

    while (true) {
        char tmp[4096]{};
        int n = recv(client_fd, tmp, sizeof(tmp), 0);

        if (n < 0) {
            perror("recv");
            break;
        }
        if (n == 0) {
            std::cout << "[斷線] fd=" << client_fd << "\n";
            break;
        }

        buffer.append(tmp, n);

        // 從累積的 buffer 裡找以 \n 結尾的完整訊息
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string message = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (message.empty()) continue;

            try {
                json j = json::parse(message);
                std::string line = to_line_protocol(j);
                std::cout << "[收到] " << message << "\n";

                if (writer->write(line)) {
                    std::cout << "[DB]   " << line << "\n";
                }
            } catch (const json::exception& e) {
                std::cerr << "[錯誤] JSON parse 失敗：" << e.what()
                          << " | 原始：" << message << "\n";
            }
        }
    }

    close(client_fd);
}
