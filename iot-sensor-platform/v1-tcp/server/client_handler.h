#pragma once
#include "influx_writer.h"

// 每條連線的處理函式，在獨立的 std::thread 裡執行
void handle_client(int client_fd, InfluxWriter* writer);
