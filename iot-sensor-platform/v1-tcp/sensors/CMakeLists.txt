cmake_minimum_required(VERSION 3.16)
project(iot_sensors)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)

add_executable(temperature_sensor temperature_sensor.cpp)
target_link_libraries(temperature_sensor Threads::Threads)

add_executable(humidity_sensor humidity_sensor.cpp)
target_link_libraries(humidity_sensor Threads::Threads)

add_executable(gps_sensor gps_sensor.cpp)
target_link_libraries(gps_sensor Threads::Threads)
