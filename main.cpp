#include "http_client.hpp"
#include "mqtt_client.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    std::cout << "=== Vehicle Mobile Gateway v2.0 ===" << std::endl;
    std::cout << "HTTPS + MQTT client for Linux Gateway" << std::endl;
    std::cout << "PQC-Ready (Post-Quantum Cryptography)" << std::endl;
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

    // Build HTTP base URL
    std::string protocol = config["server"]["http"]["use_https"].get<bool>() ? "https" : "http";
    std::string host = config["server"]["host"].get<std::string>();
    int http_port = config["server"]["http"]["port"].get<int>();
    std::string api_base = config["server"]["http"]["api_base"].get<std::string>();
    
    std::string base_url = protocol + "://" + host + ":" + std::to_string(http_port) + api_base;
    bool verify_ssl = config["tls"]["verify_peer"].get<bool>();
    std::string tls_version = config["tls"]["version"].get<std::string>();

    // PQC configuration
    bool pqc_enabled = config["pqc"]["enabled"].get<bool>();
    std::string kem_algo = config["pqc"]["kem_algorithm"].get<std::string>();
    std::string sig_algo = config["pqc"]["signature_algorithm"].get<std::string>();

    // MQTT configuration
    int mqtt_port = config["server"]["mqtt"]["port"].get<int>();
    bool mqtt_tls = config["server"]["mqtt"]["use_tls"].get<bool>();
    std::string command_topic = config["server"]["mqtt"]["topics"]["command"].get<std::string>();
    std::string status_topic = config["server"]["mqtt"]["topics"]["status"].get<std::string>();
    std::string ota_topic = config["server"]["mqtt"]["topics"]["ota"].get<std::string>();

    std::cout << "[Config] Server: " << host << std::endl;
    std::cout << "[Config] Device: " << config["device"]["name"].get<std::string>() 
              << " (" << config["device"]["id"].get<std::string>() << ")" << std::endl;
    std::cout << "[Config] Type: " << config["device"]["type"].get<std::string>() << std::endl;
    std::cout << "[Config] HTTP: " << base_url << std::endl;
    std::cout << "[Config] MQTT: " << host << ":" << mqtt_port << (mqtt_tls ? " (TLS)" : "") << std::endl;
    std::cout << "[Config] TLS Version: " << tls_version << " | Verify: " << (verify_ssl ? "enabled" : "disabled") << std::endl;
    std::cout << "[Config] PQC: " << (pqc_enabled ? "enabled" : "disabled");
    if (pqc_enabled) {
        std::cout << " | KEM: " << kem_algo << " | Sig: " << sig_algo;
    }
    std::cout << std::endl;
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

    // Test 4: MQTT Connection
    std::cout << "[Test 4] MQTT Connection..." << std::endl;
    
    try {
        std::string client_id = config["device"]["id"].get<std::string>() + "_mqtt";
        MqttClient mqtt_client(host, mqtt_port, client_id, mqtt_tls, verify_ssl);
        
        // Set message callback
        mqtt_client.setMessageCallback([](const std::string& topic, const std::string& payload) {
            std::cout << "[MQTT Callback] Topic: " << topic << " | Payload: " << payload << std::endl;
        });
        
        // Connect
        if (mqtt_client.connect()) {
            std::cout << "✅ MQTT connected" << std::endl;
            
            // Wait for connection to establish
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Subscribe to command topic
            if (mqtt_client.subscribe(command_topic)) {
                std::cout << "✅ Subscribed to commands" << std::endl;
            }
            
            // Publish status
            json mqtt_status = {
                {"device_id", config["device"]["id"]},
                {"status", "online"},
                {"timestamp", std::time(nullptr)}
            };
            
            if (mqtt_client.publish(status_topic, mqtt_status.dump())) {
                std::cout << "✅ Status published via MQTT" << std::endl;
            }
            
            // Keep alive for a bit
            std::cout << "[MQTT] Listening for messages (3 seconds)..." << std::endl;
            for (int i = 0; i < 3; i++) {
                mqtt_client.loop(1000);
            }
            
            std::cout << "✅ MQTT test complete" << std::endl;
        } else {
            std::cout << "❌ MQTT connection failed" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ MQTT error: " << e.what() << std::endl;
    }
    std::cout << std::endl;

    std::cout << "=== All Tests Complete ===" << std::endl;
    
    if (pqc_enabled) {
        std::cout << "\n⚠️  PQC is enabled in config but requires liboqs library." << std::endl;
        std::cout << "    Install: brew install liboqs (macOS) or apt install liboqs (Linux)" << std::endl;
        std::cout << "    PQC algorithms: KEM=" << kem_algo << ", Signature=" << sig_algo << std::endl;
    }
    
    return 0;
}

