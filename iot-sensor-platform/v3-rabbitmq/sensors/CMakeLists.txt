cmake_minimum_required(VERSION 3.16)
project(iot_sensors_v3)

set(CMAKE_CXX_STANDARD 17)

add_executable(temperature_sensor temperature_sensor.cpp)
target_link_libraries(temperature_sensor mosquitto)

add_executable(humidity_sensor humidity_sensor.cpp)
target_link_libraries(humidity_sensor mosquitto)

add_executable(gps_sensor gps_sensor.cpp)
target_link_libraries(gps_sensor mosquitto)
