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
    long long ts_ns = j.at("timestamp").get<long long>() * 1000000000LL;  // Unix timestamp（秒）轉成奈秒，InfluxDB precision=ns

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
        char tmp[4096]{};                                 // 建立一個大小為 4096 的 char 陣列，用於接收 client 的訊息
        int n = recv(client_fd, tmp, sizeof(tmp), 0);     // 使用 recv 函數接收 client 的訊息，client_fd 為 socket 的檔案描述符，tmp 為接收訊息的緩衝區，sizeof(tmp) 為接收訊息的長度，0 表示使用預設協定

        if (n < 0) {
            perror("recv");
            break;
        }
        if (n == 0) {
            std::cout << "[斷線] fd=" << client_fd << "\n";
            break;
        }

        buffer.append(tmp, n);                            // 將接收到的訊息 append 到 buffer 中，tmp 為接收訊息的緩衝區，n 為接收訊息的長度

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {   // 從累積的 buffer 裡找以 \n 結尾的完整訊息
            std::string message = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);                              // 將已經處理完的訊息從 buffer 中移除，pos 為找到的 \n 的位置，+1 是為了移除 \n 本身，避免下次還找到相同的 \n

            if (message.empty()) continue;

            try {
                json j = json::parse(message);                     // 把字串轉成 JSON 物件
                std::string line = to_line_protocol(j);            // 把 JSON 封包轉成 InfluxDB Line Protocol 格式
                std::cout << "[收到] " << message << "\n";

                if (writer->write(line)) {
                    std::cout << "[DB]   " << line << "\n";        // 如果寫入成功，則印出 InfluxDB Line Protocol 格式
                }
            } catch (const json::exception& e) {
                std::cerr << "[錯誤] JSON parse 失敗：" << e.what()
                          << " | 原始：" << message << "\n";
            }
        }
    }

    close(client_fd);
}
