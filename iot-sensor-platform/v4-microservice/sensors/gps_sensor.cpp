#include "base_sensor.h"
#include <iomanip>
#include <sstream>

class GpsSensor : public BaseSensor {
public:
    explicit GpsSensor(const std::string& id)
        : BaseSensor(id, "rabbitmq", 1883, "sensors/gps", 3)   // 因為 Docker 的 Network 設定，所以可以直接用 rabbitmq 連到 RabbitMQ
        , base_lng_(121.5319)
    {
        srand(static_cast<unsigned>(time(nullptr)) + 2);
    }

protected:
    std::string build_message() override {
        double lat = base_lat_ + (rand() % 1001 - 500) / 100000.0;
        double lng = base_lng_ + (rand() % 1001 - 500) / 100000.0;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
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
    GpsSensor sensor("gps_01");
    if (!sensor.connect_to_broker()) return 1;
    sensor.run();
    return 0;
}
