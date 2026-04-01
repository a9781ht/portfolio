#include "base_sensor.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>

class GpsSensor : public BaseSensor {
public:
    explicit GpsSensor(const std::string& id)
        : BaseSensor(id, "127.0.0.1", 8888, 3)
        , base_lat_(25.0478)   // 台北市中心
        , base_lng_(121.5319)
    {
        srand(static_cast<unsigned>(time(nullptr)) + 2);
    }

protected:
    std::string build_message() override {
        // 在基準點附近模擬小幅漂移（±0.005 度，約 500 公尺）
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
    if (!sensor.connect_to_server()) return 1;
    sensor.run();
    return 0;
}
