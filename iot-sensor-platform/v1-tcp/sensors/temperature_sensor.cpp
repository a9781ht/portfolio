#include "base_sensor.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>

class TemperatureSensor : public BaseSensor {
public:
    explicit TemperatureSensor(const std::string& id)
        : BaseSensor(id, "127.0.0.1", 8888, 1)
    {
        srand(static_cast<unsigned>(time(nullptr)));
    }

protected:
    std::string build_message() override {
        // 模擬 20.0 ~ 40.0 °C
        double value = 20.0 + (rand() % 2001) / 100.0;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "{\"sensor_id\":\"" << sensor_id_ << "\","
            << "\"type\":\"temperature\","
            << "\"value\":" << value << ","
            << "\"timestamp\":" << current_timestamp() << "}";
        return oss.str();
    }
};

int main() {
    TemperatureSensor sensor("temp_01");
    if (!sensor.connect_to_server()) return 1;
    sensor.run();
    return 0;
}
