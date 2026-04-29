#pragma once
#include <mosquitto.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>

class BaseSensor {
public:
    BaseSensor(const std::string& sensor_id,
               const std::string& broker_host,    // broker 的 IP 地址，用於連線到 broker
               int broker_port,                   // broker 的埠號，用於連線到 broker
               const std::string& topic,          // topic 的名稱，用於發送訊息到 broker
               int interval_sec)
        : sensor_id_(sensor_id)
        , broker_host_(broker_host)
        , broker_port_(broker_port)
        , topic_(topic)
        , interval_sec_(interval_sec)
        , mosq_(nullptr)                          // mosquitto 的指標，直接先初始化為 nullptr，表示還沒有連線
    {}

    virtual ~BaseSensor() {
        if (mosq_) {
            mosquitto_disconnect(mosq_);          // 如果 mosquitto 的指標不為 nullptr，則斷開與 broker 的連線
            mosquitto_destroy(mosq_);             // 如果 mosquitto 的指標不為 nullptr，則釋放 mosquitto 的記憶體
        }
        mosquitto_lib_cleanup();                  // 釋放 mosquitto 的記憶體
    }

    bool connect_to_broker() {
        mosquitto_lib_init();                                                         // 初始化 mosquitto 的記憶體

        mosq_ = mosquitto_new(sensor_id_.c_str(), true, nullptr);                     // 建立 mosquitto 的物件，sensor_id_ 為 sensor 的 ID，true 表示使用 persistent 連線，nullptr 表示不使用 userdata
        if (!mosq_) {                                                                 // clean_session 為 true 表示使用乾淨會話，每次重連都當新 client，不保留上次的訂閱狀態與離線佇列訊息（
            std::cerr << "[" << sensor_id_ << "] mosquitto_new 失敗\n";               // userdata 為 nullptr，表示 callback 沒有綁定自訂上下文，不帶額外資料
            return false;
        }

        int rc = mosquitto_connect(mosq_, broker_host_.c_str(), broker_port_, 60);   // 連線到 broker，mosq_ 為 mosquitto 的物件，broker_host_ 為 broker 的 IP 地址，broker_port_ 為 broker 的埠號，60 表示超時時間為 60 秒
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "[" << sensor_id_ << "] 連線失敗："
                      << mosquitto_strerror(rc) << "\n";
            return false;
        }
        return true;
    }

    void run(int count = -1) {
        int i = 0;
        while (count < 0 || i < count) {
            std::string payload = build_message();

            int rc = mosquitto_publish(     // 發送訊息到 broker
                mosq_,                      // mosquitto 的物件
                nullptr,                    // 訊息的 ID，如果為 nullptr，則由 broker 生成
                topic_.c_str(),             // topic 的名稱
                (int)payload.size(),        // 訊息的長度
                payload.c_str(),            // 訊息的內容
                0,                          // QoS 0，表示最多發送一次，如果發送失敗，則不會重試
                false                       // 不使用 retain flag，表示 broker 不保留最後一筆給新訂閱者，不會在重連後發送給新訂閱者
            );

            if (rc != MOSQ_ERR_SUCCESS) {
                std::cerr << "[" << sensor_id_ << "] publish 失敗："
                          << mosquitto_strerror(rc) << "\n";
                break;
            }

            mosquitto_loop(mosq_, 0, 1);    // 處理 broker 的回應，mosq_ 為 mosquitto 的物件，0 表示不等待回應，1 表示只處理一個事件
            std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));
            i++;
        }
    }

protected:
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
