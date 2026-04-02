cmake_minimum_required(VERSION 3.16)
project(iot_sensor_platform_v3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(sensors)
add_subdirectory(subscribers)
