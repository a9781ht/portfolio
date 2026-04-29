cmake_minimum_required(VERSION 3.16)
project(influx_subscriber_v4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(influx_writer_lib STATIC influx_writer.cpp)
target_link_libraries(influx_writer_lib CURL::libcurl)

add_executable(influx_subscriber main.cpp)
target_link_libraries(influx_subscriber
    influx_writer_lib
    nlohmann_json::nlohmann_json
    rabbitmq
)
