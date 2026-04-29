cmake_minimum_required(VERSION 3.16)
project(iot_subscriber)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(subscriber
    main.cpp
    influx_writer.cpp
)

target_link_libraries(subscriber
    CURL::libcurl
    nlohmann_json::nlohmann_json
    mosquitto          # libmosquitto
)
