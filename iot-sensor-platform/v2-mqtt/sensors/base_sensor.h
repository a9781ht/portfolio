#pragma once
#include <mosquitto.h>
#include <ctime>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <cstdlib>

class BaseSensor {
public:
    BaseSensor(const std::string& sensor_id,
               const std::string& broker_host,
               int broker_port,
               const std::string& topic,
               int interval_sec)
        : sensor_id_(sensor_id)
        , broker_host_(broker_host)
        , broker_port_(broker_port)
        , topic_(topic)
        , interval_sec_(interval_sec)
        , mosq_(nullptr)
    {}

    virtual ~BaseSensor() {
        if (mosq_) {
            mosquitto_disconnect(mosq_);
            mosquitto_destroy(mosq_);
        }
        mosquitto_lib_cleanup();
    }

    bool connect_to_broker() {
        mosquitto_lib_init();

        // client ID 用 sensor_id，clean_session=true 表示不保留舊 session
        mosq_ = mosquitto_new(sensor_id_.c_str(), true, nullptr);
        if (!mosq_) {
            std::cerr << "[" << sensor_id_ << "] mosquitto_new 失敗\n";
            return false;
        }

        int rc = mosquitto_connect(mosq_, broker_host_.c_str(), broker_port_, 60);
        // keepalive=60 秒：每 60 秒送一次 PINGREQ 確認連線存活
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "[" << sensor_id_ << "] 連線失敗："
                      << mosquitto_strerror(rc) << "\n";
            return false;
        }

        std::cout << "[" << sensor_id_ << "] 已連線到 Broker "
                  << broker_host_ << ":" << broker_port_ << "\n";
        return true;
    }

    // count = -1 表示無限傳送
    void run(int count = -1) {
        int i = 0;
        while (count < 0 || i < count) {
            std::string payload = build_message();

            // QoS 0：發了就算，不等確認（對感測器資料夠用）
            int rc = mosquitto_publish(
                mosq_,
                nullptr,               // message id（QoS 0 不需要）
                topic_.c_str(),        // topic
                (int)payload.size(),   // payload 長度
                payload.c_str(),       // payload 內容
                0,                     // QoS 0
                false                  // retain = false
            );

            if (rc != MOSQ_ERR_SUCCESS) {
                std::cerr << "[" << sensor_id_ << "] publish 失敗："
                          << mosquitto_strerror(rc) << "\n";
                break;
            }

            std::cout << "[" << sensor_id_ << "] 發布到 " << topic_
                      << " → " << payload << "\n";

            // 讓 libmosquitto 處理網路 I/O（發送緩衝區裡等待送出的封包）
            mosquitto_loop(mosq_, 0, 1);

            std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));
            i++;
        }
        std::cout << "[" << sensor_id_ << "] 結束\n";
    }

protected:
    // 子類別實作：回傳 JSON payload 字串
    virtual std::string build_message() = 0;

    long long current_timestamp() const {
        return static_cast<long long>(std::time(nullptr));
    }

    std::string sensor_id_;

private:
    std::string broker_host_;
    int broker_port_;
    std::string topic_;
    int interval_sec_;
    struct mosquitto* mosq_;
};
