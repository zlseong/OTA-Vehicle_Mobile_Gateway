#include "http_client.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    std::cout << "=== Vehicle Mobile Gateway v2.0 ===" << std::endl;
    std::cout << "Simple HTTPS client for Linux Gateway" << std::endl;
    std::cout << std::endl;

    // Load configuration
    std::string config_file = "config.json";
    if (argc > 1) {
        config_file = argv[1];
    }

    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        return 1;
    }

    json config;
    file >> config;

    // Build base URL
    std::string protocol = config["server"]["use_https"].get<bool>() ? "https" : "http";
    std::string host = config["server"]["host"].get<std::string>();
    int port = config["server"]["port"].get<int>();
    std::string api_base = config["server"]["api_base"].get<std::string>();
    
    std::string base_url = protocol + "://" + host + ":" + std::to_string(port) + api_base;
    bool verify_ssl = config["tls"]["verify_peer"].get<bool>();

    std::cout << "[Config] Server: " << base_url << std::endl;
    std::cout << "[Config] Device: " << config["device"]["name"].get<std::string>() 
              << " (" << config["device"]["id"].get<std::string>() << ")" << std::endl;
    std::cout << "[Config] Type: " << config["device"]["type"].get<std::string>() << std::endl;
    std::cout << "[Config] SSL Verify: " << (verify_ssl ? "enabled" : "disabled") << std::endl;
    std::cout << std::endl;

    // Create HTTP client
    HttpClient client(base_url, verify_ssl);

    // Test 1: Health check
    std::cout << "[Test 1] Health Check..." << std::endl;
    auto health_response = client.get("/health");
    
    if (health_response.success) {
        std::cout << "✅ Health check OK" << std::endl;
        std::cout << "   Response: " << health_response.body << std::endl;
    } else {
        std::cout << "❌ Health check failed" << std::endl;
        std::cout << "   Error: " << health_response.error << std::endl;
        std::cout << "   Status: " << health_response.status_code << std::endl;
    }
    std::cout << std::endl;

    // Test 2: Device registration
    std::cout << "[Test 2] Device Registration..." << std::endl;
    json device_info = {
        {"device_id", config["device"]["id"]},
        {"device_name", config["device"]["name"]},
        {"device_type", config["device"]["type"]},
        {"timestamp", std::time(nullptr)}
    };
    
    auto register_response = client.postJson("/devices/register", device_info.dump());
    
    if (register_response.success) {
        std::cout << "✅ Device registered" << std::endl;
        std::cout << "   Response: " << register_response.body << std::endl;
    } else {
        std::cout << "❌ Registration failed" << std::endl;
        std::cout << "   Error: " << register_response.error << std::endl;
        std::cout << "   Status: " << register_response.status_code << std::endl;
    }
    std::cout << std::endl;

    // Test 3: Status update
    std::cout << "[Test 3] Status Update..." << std::endl;
    json status_data = {
        {"device_id", config["device"]["id"]},
        {"status", "online"},
        {"uptime", 42},
        {"timestamp", std::time(nullptr)}
    };
    
    auto status_response = client.postJson("/devices/status", status_data.dump());
    
    if (status_response.success) {
        std::cout << "✅ Status updated" << std::endl;
        std::cout << "   Response: " << status_response.body << std::endl;
    } else {
        std::cout << "❌ Status update failed" << std::endl;
        std::cout << "   Error: " << status_response.error << std::endl;
        std::cout << "   Status: " << status_response.status_code << std::endl;
    }
    std::cout << std::endl;

    std::cout << "=== Test Complete ===" << std::endl;
    return 0;
}

