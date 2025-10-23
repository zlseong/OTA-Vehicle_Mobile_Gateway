#include "http_client.hpp"
#include <iostream>
#include <fstream>
#include <ctime>

HttpClient::HttpClient(const std::string& base_url, bool verify_ssl)
    : base_url_(base_url), verify_ssl_(verify_ssl) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

size_t HttpClient::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

HttpResponse HttpClient::performRequest(const std::string& method, const std::string& path, const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {0, "", false, "Failed to initialize CURL"};
    }

    std::string url = base_url_ + path;
    std::string response_body;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "VMG-Client/2.0");

    // SSL configuration
    if (!verify_ssl_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    HttpResponse result;
    result.status_code = static_cast<int>(http_code);
    result.body = response_body;
    result.success = (res == CURLE_OK && http_code >= 200 && http_code < 300);
    
    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
    } else if (http_code >= 400) {
        result.error = "HTTP " + std::to_string(http_code);
    }

    return result;
}

HttpResponse HttpClient::get(const std::string& path) {
    return performRequest("GET", path);
}

HttpResponse HttpClient::post(const std::string& path, const std::string& body) {
    return performRequest("POST", path, body);
}

HttpResponse HttpClient::postJson(const std::string& path, const std::string& json_body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {0, "", false, "Failed to initialize CURL"};
    }

    std::string url = base_url_ + path;
    std::string response_body;
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "VMG-Client/2.0");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    
    if (!verify_ssl_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    HttpResponse result;
    result.status_code = static_cast<int>(http_code);
    result.body = response_body;
    result.success = (res == CURLE_OK && http_code >= 200 && http_code < 300);
    
    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
    }

    return result;
}

HttpResponse HttpClient::postFile(const std::string& path, const std::string& filepath) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {0, "", false, "Failed to initialize CURL"};
    }

    // Read file content
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        curl_easy_cleanup(curl);
        return {0, "", false, "Failed to open file: " + filepath};
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read file content
    std::string file_content(file_size, '\0');
    file.read(&file_content[0], file_size);
    file.close();
    
    // Get filename from path
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    
    // Create multipart form data
    std::string boundary = "----WebKitFormBoundary" + std::to_string(time(nullptr));
    std::string form_data = "--" + boundary + "\r\n";
    form_data += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
    form_data += "Content-Type: text/plain\r\n\r\n";
    form_data += file_content + "\r\n";
    form_data += "--" + boundary + "--\r\n";
    
    std::string url = base_url_ + path;
    std::string response_body;
    
    struct curl_slist* headers = nullptr;
    std::string content_type = "multipart/form-data; boundary=" + boundary;
    headers = curl_slist_append(headers, ("Content-Type: " + content_type).c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "VMG-Client/2.0");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, form_data.length());
    
    if (!verify_ssl_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    HttpResponse result;
    result.status_code = static_cast<int>(http_code);
    result.body = response_body;
    result.success = (res == CURLE_OK && http_code >= 200 && http_code < 300);
    
    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
    }

    return result;
}

