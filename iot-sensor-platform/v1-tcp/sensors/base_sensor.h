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
    BaseSensor(const std::string& sensor_id,
               const std::string& server_ip,
               int port,
               int interval_sec)
        : sensor_id_(sensor_id)
        , server_ip_(server_ip)
        , port_(port)
        , interval_sec_(interval_sec)
        , fd_(-1)
    {}

    virtual ~BaseSensor() {
        if (fd_ >= 0) close(fd_);
    }

    bool connect_to_server() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) { perror("socket"); return false; }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port_);
        inet_pton(AF_INET, server_ip_.c_str(), &addr.sin_addr);

        if (connect(fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect");
            return false;
        }
        std::cout << "[" << sensor_id_ << "] 已連線到 Server\n";
        return true;
    }

    // count = -1 表示無限傳送
    void run(int count = -1) {
        int i = 0;
        while (count < 0 || i < count) {
            std::string msg = build_message() + "\n";
            int n = send(fd_, msg.c_str(), msg.size(), 0);
            if (n < 0) { perror("send"); break; }

            // 印出時不印結尾 \n，避免多一行空白
            std::cout << "[" << sensor_id_ << "] 送出：" << build_message() << "\n";

            std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));
            i++;
        }
        std::cout << "[" << sensor_id_ << "] 結束\n";
    }

protected:
    // 子類別實作：回傳一條 JSON 字串（不含結尾 \n）
    virtual std::string build_message() = 0;

    long long current_timestamp() const {
        return static_cast<long long>(std::time(nullptr));
    }

    std::string sensor_id_;

private:
    std::string server_ip_;
    int port_;
    int interval_sec_;
    int fd_;
};
