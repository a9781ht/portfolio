#include "base_sensor.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>

class TemperatureSensor : public BaseSensor {
public:
    explicit TemperatureSensor(const std::string& id)
        : BaseSensor(id, "localhost", 8888, 1)          // 建立溫度感測器，ID 為 temp_01，伺服器 IP 地址為 localhost，埠號為 8888，發送間隔為 1 秒
    {
        srand(static_cast<unsigned>(time(nullptr)));    // 使用 time(nullptr) 取得當前時間，避免同時啟動時產生相同亂數序列
    }

protected:
    std::string build_message() override {
        double value = 20.0 + (rand() % 2001) / 100.0;        // 模擬 20.0 ~ 40.0 °C

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);            // 設定輸出格式：固定小數點格式，且小數點後 2 位
        oss << "{\"sensor_id\":\"" << sensor_id_ << "\","
            << "\"type\":\"temperature\","
            << "\"value\":" << value << ","
            << "\"timestamp\":" << current_timestamp() << "}";
        return oss.str();
    }
};

int main() {
    TemperatureSensor sensor("temp_01");             // 建立溫度感測器，ID 為 temp_01
    if (!sensor.connect_to_server()) return 1;       // 連線到伺服器，如果連線失敗，則返回 1
    sensor.run();                                    // 執行感測器，開始發送溫度資料，預設為無限迴圈
    return 0;
}
