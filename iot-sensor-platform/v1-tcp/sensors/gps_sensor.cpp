#include "base_sensor.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>

class GpsSensor : public BaseSensor {
public:
    explicit GpsSensor(const std::string& id)           
        : BaseSensor(id, "localhost", 8888, 3)              // 建立 GPS 感測器，ID 為 gps_01，伺服器 IP 地址為 localhost，埠號為 8888，發送間隔為 3 秒
        , base_lat_(25.0478)
        , base_lng_(121.5319)
    {
        srand(static_cast<unsigned>(time(nullptr)) + 2);    // 使用 time(nullptr) 取得當前時間，並加上 2，避免同時啟動時產生相同亂數序列
    }

protected:
    std::string build_message() override {
        double lat = base_lat_ + (rand() % 1001 - 500) / 100000.0;    // 在基準點附近模擬小幅漂移（±0.005 度，約 500 公尺）
        double lng = base_lng_ + (rand() % 1001 - 500) / 100000.0;    // 在基準點附近模擬小幅漂移（±0.005 度，約 500 公尺）

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);                    // 設定輸出格式：固定小數點格式，且小數點後保留 6 位
        oss << "{\"sensor_id\":\"" << sensor_id_ << "\","
            << "\"type\":\"gps\","
            << "\"lat\":" << lat << ","
            << "\"lng\":" << lng << ","
            << "\"timestamp\":" << current_timestamp() << "}";
        return oss.str();
    }

private:
    double base_lat_;
    double base_lng_;
};

int main() {
    GpsSensor sensor("gps_01");                  // 建立 GPS 感測器，ID 為 gps_01
    if (!sensor.connect_to_server()) return 1;   // 連線到伺服器，如果連線失敗，則返回 1
    sensor.run();                                // 執行感測器，開始發送 GPS 資料，預設為無限迴圈
    return 0;
}
