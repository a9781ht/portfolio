#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include "client_handler.h"
#include "influx_writer.h"

static const int    PORT          = 8888;              // 伺服器埠號
static const char*  INFLUX_HOST   = "localhost";       // InfluxDB 主機地址
static const int    INFLUX_PORT   = 8086;              // InfluxDB 埠號
static const char*  INFLUX_ORG    = "iot_org";         // InfluxDB 組織
static const char*  INFLUX_BUCKET = "sensors";         // InfluxDB 資料庫
static const char*  INFLUX_TOKEN  = "mytoken123";      // InfluxDB 權限

int main() {
    InfluxWriter writer(INFLUX_HOST, INFLUX_PORT,         // 建立 InfluxWriter 物件
                        INFLUX_ORG, INFLUX_BUCKET, INFLUX_TOKEN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);      // 建立 socket，AF_INET 表示 IPv4 網路，SOCK_STREAM 表示 TCP 傳輸，0 表示使用預設協定
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;                                                         // 把 opt 設為 1，表示設定 socket 可重用
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  // 設定 socket 可重用，避免重啟後需要等待一段時間才能重新使用同一個 port

    sockaddr_in addr{};                                           // netinet/in.h 已定義一個 sockaddr_in 的 struct，我們填上對應的資訊
    addr.sin_family      = AF_INET;                               // 填上 sin_family 為 IPv4
    addr.sin_port        = htons(PORT);                           // 填上 sin_port 為埠號
    addr.sin_addr.s_addr = INADDR_ANY;                            // 填上 sin_addr.s_addr 為 INADDR_ANY，表示 server 監聽本機所有網卡介面，等同於綁在 0.0.0.0

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {    // 使用 bind 函數綁定 socket 到指定的 IP 地址和埠號，server_fd 為 socket 的檔案描述符，(sockaddr*)&addr 為伺服器地址，sizeof(addr) 為伺服器地址的長度
        perror("bind"); return 1;
    }
    if (listen(server_fd, 10) < 0) {                              // 使用 listen 函數監聽 socket，server_fd 為 socket 的檔案描述符，10 表示最大等待連線數
        perror("listen"); return 1;
    }

    std::cout << "[Server] 啟動，監聽 port " << PORT << "...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);       // 使用 accept 函數接受 client 的連線，server_fd 為 socket 的檔案描述符，nullptr 表示不關心 client 的 IP 地址和埠號
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        std::thread(handle_client, client_fd, &writer).detach();   // 每條連線開一條獨立 thread，主執行緒馬上回來等下一個 client 的連線，因為 detach 表示分離執行緒，不會阻塞主執行緒
    }

    close(server_fd);
    return 0;
}
