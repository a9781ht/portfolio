cmake_minimum_required(VERSION 3.16)
project(iot_server)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

add_executable(server
    main.cpp
    client_handler.cpp
    influx_writer.cpp
)

target_link_libraries(server
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
)
