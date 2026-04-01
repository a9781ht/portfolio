#include "base_sensor.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>

class HumiditySensor : public BaseSensor {
public:
    explicit HumiditySensor(const std::string& id)
        : BaseSensor(id, "127.0.0.1", 8888, 2)
    {
        // +1 讓種子與溫度感測器不同，避免同時啟動時產生相同亂數序列
        srand(static_cast<unsigned>(time(nullptr)) + 1);
    }

protected:
    std::string build_message() override {
        // 模擬 40.0 ~ 90.0 %
        double value = 40.0 + (rand() % 5001) / 100.0;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "{\"sensor_id\":\"" << sensor_id_ << "\","
            << "\"type\":\"humidity\","
            << "\"value\":" << value << ","
            << "\"timestamp\":" << current_timestamp() << "}";
        return oss.str();
    }
};

int main() {
    HumiditySensor sensor("hum_01");
    if (!sensor.connect_to_server()) return 1;
    sensor.run();
    return 0;
}
