#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

class BaseSensor {
public:
    // C++ 建構函數的初始化列表，在進入{}函數體前，就先把成員變數初始化，比在{}函數體內初始化更有效率
    BaseSensor(const std::string& sensor_id,   // 感測器 ID，用於識別感測器
               const std::string& server_ip,   // 伺服器 IP 地址
               int port,                       // 伺服器端口
               int interval_sec)               // 發送間隔（秒）
        : sensor_id_(sensor_id)
        , server_ip_(server_ip)
        , port_(port)
        , interval_sec_(interval_sec)
        , fd_(-1)                              // File Descriptor，用於表示 socket 的檔案描述符，直接先初始化為 -1，表示還沒有連線
    {}

    virtual ~BaseSensor() {
        if (fd_ >= 0) close(fd_);              // 如果 fd_ 大於或等於 0，表示已經連線成功，則關閉 socket
    }

    bool connect_to_server() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);                     // 建立 socket，AF_INET 表示 IPv4 網路，SOCK_STREAM 表示 TCP 傳輸，0 表示使用預設協定
        if (fd_ < 0) { perror("socket"); return false; }

        sockaddr_in addr{};                                        // netinet/in.h 已定義一個 sockaddr_in 的 struct，我們填上對應的資訊
        addr.sin_family = AF_INET;                                 // 填上 sin_family 為 IPv4
        addr.sin_port   = htons(port_);                            // 填上 sin_port 為埠號
        inet_pton(AF_INET, server_ip_.c_str(), &addr.sin_addr);    // 填上 sin_addr 為伺服器 IP 地址，並透過 inet_pton 將「字串格式 IP」轉成「二進位網路格式 IP」

        if (connect(fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {    // 使用 connect 函數連線到伺服器，fd_ 為 socket 的檔案描述符，(sockaddr*)&addr 為伺服器地址，sizeof(addr) 為伺服器地址的長度
            // connect 介面設計成吃 sockaddr*，是為了同一個 API 同時支援
            // IPv4(sockaddr_in)、IPv6(sockaddr_in6)、Unix domain(sockaddr_un) 等
            // sockaddr_in 是「具體型別」，而 connect 要的是「通用基底型別指標」，所以要轉型為 sockaddr*
            perror("connect");
            return false;
        }
        std::cout << "[" << sensor_id_ << "] 已連線到 Server\n";
        return true;
    }

    void run(int count = -1) {                                         // 呼叫 run 時
        int i = 0;                                                     // 當不傳參數，count 會預設為 -1，表示無限傳送。例如 run() -> 無限迴圈
        while (count < 0 || i < count) {                               // 當傳入參數時，count 為傳入的參數，表示傳送次數。例如 run(10) -> 送 10 次後就結束
            std::string msg = build_message() + "\n";
            int n = send(fd_, msg.c_str(), msg.size(), 0);             // 使用 send 函數發送訊息，fd_ 為 socket 的檔案描述符，msg.c_str() 為訊息的內容，msg.size() 為訊息的長度，0 表示使用預設協定
            if (n < 0) { perror("send"); break; }

            std::cout << "[" << sensor_id_ << "] 送出：" << build_message() << "\n";

            std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));  // 等待 interval_sec_ 秒，表示下次發送的間隔
            i++;                                                               // 遞增 i，表示已經送出一次
        }
        std::cout << "[" << sensor_id_ << "] 結束\n";
    }

protected:
    // 純虛擬函數，子類別必須實作
    virtual std::string build_message() = 0;                  // 回傳一條 JSON 字串（不含結尾 \n）

    long long current_timestamp() const {
        return static_cast<long long>(std::time(nullptr));    // 回傳當前時間戳記，單位為秒
    }

    std::string sensor_id_;

private:
    std::string server_ip_;
    int port_;
    int interval_sec_;
    int fd_;
};
