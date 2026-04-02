#include "base_sensor.h"
#include <iomanip>
#include <sstream>

class HumiditySensor : public BaseSensor {
public:
    explicit HumiditySensor(const std::string& id)
        : BaseSensor(id, "rabbitmq", 1883, "sensors/humidity", 2)
    {
        srand(static_cast<unsigned>(time(nullptr)) + 1);
    }

protected:
    std::string build_message() override {
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
    if (!sensor.connect_to_broker()) return 1;
    sensor.run();
    return 0;
}
