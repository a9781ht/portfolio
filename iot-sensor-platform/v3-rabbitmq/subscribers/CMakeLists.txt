cmake_minimum_required(VERSION 3.16)
project(iot_subscribers_v3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# common 共用函式庫（InfluxWriter）
add_library(influx_writer_lib STATIC
    common/influx_writer.cpp
)
target_include_directories(influx_writer_lib PUBLIC common)
target_link_libraries(influx_writer_lib CURL::libcurl)

# influx_subscriber：解析 JSON 寫入 InfluxDB
add_executable(influx_subscriber
    influx_subscriber/main.cpp
)
target_include_directories(influx_subscriber PRIVATE common)
target_link_libraries(influx_subscriber
    influx_writer_lib
    nlohmann_json::nlohmann_json
    rabbitmq                       # librabbitmq（rabbitmq-c）
)

# alert_subscriber：閾值警報
add_executable(alert_subscriber
    alert_subscriber/main.cpp
)
target_link_libraries(alert_subscriber
    nlohmann_json::nlohmann_json
    rabbitmq
)

# logger_subscriber：原始 JSON 寫入檔案
add_executable(logger_subscriber
    logger_subscriber/main.cpp
)
target_link_libraries(logger_subscriber
    rabbitmq
)
