#include "base_sensor.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>

class HumiditySensor : public BaseSensor {
public:
    explicit HumiditySensor(const std::string& id)
        : BaseSensor(id, "localhost", 8888, 2)                // 建立濕度感測器，ID 為 hum_01，伺服器 IP 地址為 localhost，埠號為 8888，發送間隔為 2 秒
    {
        srand(static_cast<unsigned>(time(nullptr)) + 1);      // 使用 time(nullptr) 取得當前時間，並加上 1，避免同時啟動時產生相同亂數序列
    }

protected:
    std::string build_message() override {
        double value = 40.0 + (rand() % 5001) / 100.0;        // 模擬 40.0 ~ 90.0 %

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);            // 設定輸出格式：固定小數點格式，且小數點後 2 位
        oss << "{\"sensor_id\":\"" << sensor_id_ << "\","
            << "\"type\":\"humidity\","
            << "\"value\":" << value << ","
            << "\"timestamp\":" << current_timestamp() << "}";
        return oss.str();
    }
};

int main() {
    HumiditySensor sensor("hum_01");               // 建立濕度感測器，ID 為 hum_01
    if (!sensor.connect_to_server()) return 1;     // 連線到伺服器，如果連線失敗，則返回 1
    sensor.run();                                  // 執行感測器，開始發送濕度資料，預設為無限迴圈
    return 0;
}
