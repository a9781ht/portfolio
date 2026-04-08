#include "influx_writer.h"
#include <curl/curl.h>
#include <iostream>

// libcurl 要求的 write callback，把回應內容丟掉不印出
static size_t discard_response(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

InfluxWriter::InfluxWriter(const std::string& host, int port,
                           const std::string& org, const std::string& bucket,
                           const std::string& token)
    : token_(token)
{
    url_ = "http://" + host + ":" + std::to_string(port) +
           "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=ns";
}

bool InfluxWriter::write(const std::string& line_protocol) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[InfluxDB] curl_easy_init 失敗\n";
        return false;
    }

    struct curl_slist* headers = nullptr;
    std::string auth_header = "Authorization: Token " + token_;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, line_protocol.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)line_protocol.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_response);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[InfluxDB] 網路錯誤：" << curl_easy_strerror(res) << "\n";
        return false;
    }
    // InfluxDB v2 寫入成功回傳 204 No Content
    if (http_code != 204) {
        std::cerr << "[InfluxDB] HTTP 錯誤碼：" << http_code << "\n";
        return false;
    }
    return true;
}
