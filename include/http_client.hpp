/**
 * @file http_client.hpp
 * @brief HTTP Client for VMG (libcurl)
 * 
 * Implements OTA-Server HTTP API:
 * - GET  /health
 * - POST /api/vehicles/{vin}/vci
 * - POST /api/vehicles/{vin}/readiness
 * - GET  /packages/{campaign_id}/full_package.bin
 */

#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <string>
#include <functional>
#include <map>
#include <memory>

struct HttpResponse {
    bool success;
    int status_code;
    std::string body;
    std::string error;
    std::map<std::string, std::string> headers;
};

class HttpClient {
public:
    HttpClient(const std::string& base_url, bool verify_ssl = true);
    ~HttpClient();
    
    /**
     * @brief HTTP GET request
     */
    HttpResponse get(const std::string& endpoint);
    
    /**
     * @brief HTTP POST with JSON body
     */
    HttpResponse postJson(const std::string& endpoint, const std::string& json_data);
    
    /**
     * @brief HTTP POST with form data
     */
    HttpResponse postForm(const std::string& endpoint, const std::map<std::string, std::string>& form_data);
    
    /**
     * @brief Download file with progress callback
     */
    bool downloadFile(const std::string& url, const std::string& output_path,
                     std::function<void(int, int)> progress_callback = nullptr);
    
    /**
     * @brief Set custom headers
     */
    void setHeaders(const std::map<std::string, std::string>& headers);
    
    /**
     * @brief Set authentication token
     */
    void setAuthToken(const std::string& token);
    
private:
    std::string base_url_;
    bool verify_ssl_;
    std::map<std::string, std::string> custom_headers_;
    
    /**
     * @brief Perform HTTP request
     */
    HttpResponse performRequest(const std::string& method, const std::string& url,
                                const std::string& body = "");
    
    /**
     * @brief CURL write callback
     */
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    
    /**
     * @brief CURL header callback
     */
    static size_t headerCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

#endif // HTTP_CLIENT_HPP
