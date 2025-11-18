/**
 * @file mqtt_client.cpp
 * @brief MQTT Client Implementation using Paho MQTT C++
 */

#include "mqtt_client.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <ctime>

using json = nlohmann::json;

// ============================================================================
// Callback Implementation
// ============================================================================

void MqttClient::Callback::connection_lost(const std::string& cause) {
    std::cerr << "[MQTT] Connection lost: " << cause << std::endl;
}

void MqttClient::Callback::message_arrived(mqtt::const_message_ptr msg) {
    if (parent_->message_callback_) {
        parent_->message_callback_(msg->get_topic(), msg->to_string());
    }
}

void MqttClient::Callback::delivery_complete(mqtt::delivery_token_ptr token) {
    // Message delivered successfully
}

// ============================================================================
// MqttClient Implementation
// ============================================================================

MqttClient::MqttClient(const std::string& host, int port, const std::string& client_id,
                       const std::string& vin, bool use_tls, bool verify_peer)
    : host_(host), port_(port), client_id_(client_id), vin_(vin),
      use_tls_(use_tls), verify_peer_(verify_peer) {
    
    // Build server URI
    std::string protocol = use_tls ? "ssl" : "tcp";
    std::string server_uri = protocol + "://" + host + ":" + std::to_string(port);
    
    // Create MQTT client
    client_ = std::make_unique<mqtt::async_client>(server_uri, client_id);
    
    // Create callback
    callback_ = std::make_unique<Callback>(this);
    client_->set_callback(*callback_);
}

MqttClient::~MqttClient() {
    if (isConnected()) {
        disconnect();
    }
}

bool MqttClient::connect() {
    try {
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(60);
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);
        
        if (use_tls_) {
            mqtt::ssl_options ssl;
            ssl.set_trust_store("ca.crt");
            
            if (!verify_peer_) {
                ssl.set_enable_server_cert_auth(false);
            }
            
            connOpts.set_ssl(ssl);
        }
        
        std::cout << "[MQTT] Connecting to " << host_ << ":" << port_ << "...\n";
        
        auto tok = client_->connect(connOpts);
        tok->wait();
        
        if (tok->get_reason_code() == mqtt::ReasonCode::SUCCESS) {
            std::cout << "[MQTT] ✓ Connected successfully\n";
            return true;
        } else {
            std::cerr << "[MQTT] ✗ Connection failed\n";
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Connection error: " << e.what() << std::endl;
        return false;
    }
}

void MqttClient::disconnect() {
    try {
        if (client_ && isConnected()) {
            std::cout << "[MQTT] Disconnecting...\n";
            client_->disconnect()->wait();
            std::cout << "[MQTT] ✓ Disconnected\n";
        }
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Disconnect error: " << e.what() << std::endl;
    }
}

bool MqttClient::isConnected() const {
    return client_ && client_->is_connected();
}

bool MqttClient::subscribe(const std::string& topic, int qos) {
    try {
        if (!isConnected()) {
            std::cerr << "[MQTT] Not connected\n";
            return false;
        }
        
        std::cout << "[MQTT] Subscribing to " << topic << " (QoS " << qos << ")...\n";
        
        auto tok = client_->subscribe(topic, qos);
        tok->wait();
        
        std::cout << "[MQTT] ✓ Subscribed to " << topic << "\n";
        return true;
        
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Subscribe error: " << e.what() << std::endl;
        return false;
    }
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos) {
    try {
        if (!isConnected()) {
            std::cerr << "[MQTT] Not connected\n";
            return false;
        }
        
        auto msg = mqtt::make_message(topic, payload, qos, false);
        client_->publish(msg);
        
        std::cout << "[MQTT] Published to " << topic << " (" << payload.size() << " bytes)\n";
        return true;
        
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Publish error: " << e.what() << std::endl;
        return false;
    }
}

void MqttClient::loop(int timeout_ms) {
    // Paho MQTT C++ handles message processing internally via callbacks
    // This is just for API compatibility
    (void)timeout_ms;
}

void MqttClient::setMessageCallback(MqttMessageCallback callback) {
    message_callback_ = callback;
}

std::string MqttClient::getTopic(const std::string& suffix) const {
    return "oem/" + vin_ + "/" + suffix;
}

// ============================================================================
// OTA-Server API Methods
// ============================================================================

bool MqttClient::sendWakeUp(const std::string& vmg_sw_version, const std::string& vehicle_state) {
    json payload = {
        {"msg_type", "vehicle_wake_up"},
        {"timestamp", std::time(nullptr)},
        {"vin", vin_},
        {"vmg_info", {
            {"sw_version", vmg_sw_version},
            {"hw_version", "2.0"}
        }},
        {"vehicle_state", {
            {"state", vehicle_state},
            {"ignition", vehicle_state == "DRIVING"}
        }}
    };
    
    return publish(getTopic("wake_up"), payload.dump(), 1);
}

bool MqttClient::sendVciReport(const std::string& vci_json) {
    json vci_data = json::parse(vci_json);
    
    json payload = {
        {"msg_type", "vci_report"},
        {"timestamp", std::time(nullptr)},
        {"vin", vin_},
        {"vmg", vci_data.value("vmg", json::object())},
        {"zgw", vci_data.value("zgw", json::object())},
        {"zones", vci_data.value("zones", json::array())}
    };
    
    return publish(getTopic("vci"), payload.dump(), 1);
}

bool MqttClient::sendReadinessResponse(const std::string& readiness_json) {
    json readiness_data = json::parse(readiness_json);
    
    json payload = {
        {"msg_type", "ota_readiness_response"},
        {"timestamp", std::time(nullptr)},
        {"vin", vin_},
        {"overall_status", readiness_data.value("ready_for_ota", false) ? "ready" : "not_ready"},
        {"ecu_readiness", readiness_data.value("ecu_readiness", json::array())}
    };
    
    return publish(getTopic("response"), payload.dump(), 1);
}

bool MqttClient::sendDownloadProgress(const std::string& campaign_id, int percentage,
                                      int bytes_downloaded, int total_bytes) {
    json payload = {
        {"msg_type", "ota_download_progress"},
        {"timestamp", std::time(nullptr)},
        {"vin", vin_},
        {"campaign_id", campaign_id},
        {"progress", {
            {"percentage", percentage},
            {"bytes_downloaded", bytes_downloaded},
            {"total_bytes", total_bytes}
        }}
    };
    
    return publish(getTopic("ota/status"), payload.dump(), 0);
}

bool MqttClient::sendHeartbeat(const std::string& vehicle_state, int uptime_sec) {
    json payload = {
        {"msg_type", "telemetry"},
        {"timestamp", std::time(nullptr)},
        {"vin", vin_},
        {"vehicle_state", vehicle_state},
        {"uptime_sec", uptime_sec}
    };
    
    return publish(getTopic("telemetry"), payload.dump(), 0);
}
