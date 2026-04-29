#pragma once
#include <string>

class InfluxWriter {
public:
    InfluxWriter(const std::string& host, int port,
                 const std::string& org, const std::string& bucket,
                 const std::string& token);

    bool write(const std::string& line_protocol);

private:
    std::string url_;
    std::string token_;
};
