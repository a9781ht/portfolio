#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include "client_handler.h"
#include "influx_writer.h"

static const int    PORT       = 8888;
static const char*  INFLUX_HOST = "localhost";
static const int    INFLUX_PORT = 8086;
static const char*  INFLUX_ORG  = "iot_org";
static const char*  INFLUX_BUCKET = "sensors";
static const char*  INFLUX_TOKEN  = "mytoken123";

int main() {
    InfluxWriter writer(INFLUX_HOST, INFLUX_PORT,
                        INFLUX_ORG, INFLUX_BUCKET, INFLUX_TOKEN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    // 讓 Server 重啟後可以立即重用同一個 port
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen"); return 1;
    }

    std::cout << "[Server] 啟動，監聽 port " << PORT << "...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        // 每條連線開一條獨立 thread，主執行緒馬上回來等下一個
        std::thread(handle_client, client_fd, &writer).detach();
    }

    close(server_fd);
    return 0;
}
