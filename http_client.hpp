#pragma once

#include <string>
#include <curl/curl.h>

struct HttpResponse {
    int status_code;
    std::string body;
    bool success;
    std::string error;
};

class HttpClient {
public:
    HttpClient(const std::string& base_url, bool verify_ssl = false);
    ~HttpClient();
    
    HttpResponse get(const std::string& path);
    HttpResponse post(const std::string& path, const std::string& body);
    HttpResponse postJson(const std::string& path, const std::string& json_body);

private:
    std::string base_url_;
    bool verify_ssl_;
    
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    HttpResponse performRequest(const std::string& method, const std::string& path, const std::string& body = "");
};

