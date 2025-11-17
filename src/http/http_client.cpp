/**
 * @file http_client.cpp
 * @brief HTTP Client Implementation using libcurl
 */

#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <sstream>

// ============================================================================
// Callback Functions
// ============================================================================

size_t HttpClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t HttpClient::headerCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string header(static_cast<char*>(contents), total_size);
    
    auto* headers = static_cast<std::map<std::string, std::string>*>(userp);
    
    // Parse "Key: Value" format
    size_t colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);
        
        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        
        (*headers)[key] = value;
    }
    
    return total_size;
}

// ============================================================================
// HttpClient Implementation
// ============================================================================

HttpClient::HttpClient(const std::string& base_url, bool verify_ssl)
    : base_url_(base_url), verify_ssl_(verify_ssl) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

HttpResponse HttpClient::get(const std::string& endpoint) {
    std::string url = base_url_ + endpoint;
    return performRequest("GET", url);
}

HttpResponse HttpClient::postJson(const std::string& endpoint, const std::string& json_data) {
    std::string url = base_url_ + endpoint;
    
    // Add Content-Type header for JSON
    custom_headers_["Content-Type"] = "application/json";
    
    return performRequest("POST", url, json_data);
}

HttpResponse HttpClient::postForm(const std::string& endpoint,
                                  const std::map<std::string, std::string>& form_data) {
    std::string url = base_url_ + endpoint;
    
    // Build form data string
    std::stringstream form_body;
    bool first = true;
    for (const auto& pair : form_data) {
        if (!first) form_body << "&";
        form_body << pair.first << "=" << pair.second;
        first = false;
    }
    
    custom_headers_["Content-Type"] = "application/x-www-form-urlencoded";
    
    return performRequest("POST", url, form_body.str());
}

HttpResponse HttpClient::performRequest(const std::string& method, const std::string& url,
                                       const std::string& body) {
    HttpResponse response;
    response.success = false;
    response.status_code = 0;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL";
        return response;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    
    // Set callbacks
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    
    // SSL verification
    if (!verify_ssl_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Set custom headers
    struct curl_slist* headers = nullptr;
    for (const auto& header : custom_headers_) {
        std::string header_str = header.first + ": " + header.second;
        headers = curl_slist_append(headers, header_str.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
        std::cerr << "[HTTP] Request failed: " << response.error << std::endl;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        response.status_code = static_cast<int>(http_code);
        response.success = (http_code >= 200 && http_code < 300);
        
        std::cout << "[HTTP] " << method << " " << url 
                  << " → " << http_code << " (" << response.body.size() << " bytes)\n";
    }
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    
    return response;
}

bool HttpClient::downloadFile(const std::string& url, const std::string& output_path,
                              std::function<void(int, int)> progress_callback) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[HTTP] Failed to initialize CURL for download\n";
        return false;
    }
    
    // Open output file
    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "[HTTP] Failed to open output file: " << output_path << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }
    
    // Write callback for file
    auto file_write_callback = [](void* ptr, size_t size, size_t nmemb, void* stream) -> size_t {
        auto* outfile = static_cast<std::ofstream*>(stream);
        outfile->write(static_cast<char*>(ptr), size * nmemb);
        return size * nmemb;
    };
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outfile);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    if (!verify_ssl_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Progress callback
    if (progress_callback) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, 
            [](void* clientp, curl_off_t dltotal, curl_off_t dlnow,
               curl_off_t ultotal, curl_off_t ulnow) -> int {
                auto* callback = static_cast<std::function<void(int, int)>*>(clientp);
                if (dltotal > 0) {
                    (*callback)(static_cast<int>(dlnow), static_cast<int>(dltotal));
                }
                return 0;
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_callback);
    }
    
    // Perform download
    CURLcode res = curl_easy_perform(curl);
    
    outfile.close();
    
    if (res != CURLE_OK) {
        std::cerr << "[HTTP] Download failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_easy_cleanup(curl);
    
    if (http_code >= 200 && http_code < 300) {
        std::cout << "[HTTP] ✓ Downloaded to " << output_path << std::endl;
        return true;
    } else {
        std::cerr << "[HTTP] Download failed with status " << http_code << std::endl;
        return false;
    }
}

void HttpClient::setHeaders(const std::map<std::string, std::string>& headers) {
    custom_headers_ = headers;
}

void HttpClient::setAuthToken(const std::string& token) {
    custom_headers_["Authorization"] = "Bearer " + token;
}
