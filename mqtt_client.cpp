#include "mqtt_client.hpp"
#include <iostream>
#include <cstring>

MqttClient::MqttClient(const std::string& broker_host, 
                       int broker_port,
                       const std::string& client_id,
                       bool use_tls,
                       bool verify_peer)
    : broker_host_(broker_host)
    , broker_port_(broker_port)
    , client_id_(client_id)
    , use_tls_(use_tls)
    , verify_peer_(verify_peer)
    , connected_(false)
    , mosq_(nullptr) {
    
    mosquitto_lib_init();
    mosq_ = mosquitto_new(client_id.c_str(), true, this);
    
    if (!mosq_) {
        throw std::runtime_error("Failed to create mosquitto instance");
    }
    
    mosquitto_connect_callback_set(mosq_, onConnect);
    mosquitto_disconnect_callback_set(mosq_, onDisconnect);
    mosquitto_message_callback_set(mosq_, onMessage);
    
    if (use_tls_) {
        // Configure TLS for insecure development mode
        if (!verify_peer_) {
            // Disable hostname and certificate verification
            mosquitto_tls_insecure_set(mosq_, true);
            std::cout << "[MQTT] TLS insecure mode enabled (development only)" << std::endl;
        }
        // Note: For production, set CA certificate with mosquitto_tls_set()
    }
}

MqttClient::~MqttClient() {
    disconnect();
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
    mosquitto_lib_cleanup();
}

bool MqttClient::connect() {
    if (!mosq_) {
        return false;
    }
    
    std::cout << "[MQTT] Connecting to " << broker_host_ << ":" << broker_port_;
    if (use_tls_) {
        std::cout << " (TLS)";
    }
    std::cout << std::endl;
    
    int ret = mosquitto_connect(mosq_, broker_host_.c_str(), broker_port_, 60);
    
    if (ret != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Connection failed: " << mosquitto_strerror(ret) << std::endl;
        return false;
    }
    
    // Start network loop
    ret = mosquitto_loop_start(mosq_);
    if (ret != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Loop start failed: " << mosquitto_strerror(ret) << std::endl;
        return false;
    }
    
    return true;
}

void MqttClient::disconnect() {
    if (mosq_ && connected_) {
        mosquitto_loop_stop(mosq_, false);
        mosquitto_disconnect(mosq_);
        connected_ = false;
    }
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos) {
    if (!connected_) {
        std::cerr << "[MQTT] Not connected, cannot publish" << std::endl;
        return false;
    }
    
    int ret = mosquitto_publish(mosq_, nullptr, topic.c_str(), 
                                payload.length(), payload.c_str(), qos, false);
    
    if (ret != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Publish failed: " << mosquitto_strerror(ret) << std::endl;
        return false;
    }
    
    std::cout << "[MQTT] Published to " << topic << ": " << payload << std::endl;
    return true;
}

bool MqttClient::subscribe(const std::string& topic, int qos) {
    if (!mosq_) {
        return false;
    }
    
    int ret = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), qos);
    
    if (ret != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Subscribe failed: " << mosquitto_strerror(ret) << std::endl;
        return false;
    }
    
    std::cout << "[MQTT] Subscribed to " << topic << std::endl;
    return true;
}

void MqttClient::loop(int timeout_ms) {
    if (mosq_) {
        mosquitto_loop(mosq_, timeout_ms, 1);
    }
}

void MqttClient::onConnect(struct mosquitto* mosq, void* obj, int result) {
    MqttClient* client = static_cast<MqttClient*>(obj);
    
    if (result == 0) {
        std::cout << "[MQTT] ✅ Connected successfully" << std::endl;
        client->connected_ = true;
    } else {
        std::cerr << "[MQTT] ❌ Connection failed with code: " << result << std::endl;
        client->connected_ = false;
    }
}

void MqttClient::onDisconnect(struct mosquitto* mosq, void* obj, int result) {
    MqttClient* client = static_cast<MqttClient*>(obj);
    client->connected_ = false;
    
    if (result == 0) {
        std::cout << "[MQTT] Disconnected cleanly" << std::endl;
    } else {
        std::cerr << "[MQTT] Unexpected disconnection" << std::endl;
    }
}

void MqttClient::onMessage(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message) {
    MqttClient* client = static_cast<MqttClient*>(obj);
    
    if (message->payloadlen > 0 && client->message_callback_) {
        std::string topic(message->topic);
        std::string payload(static_cast<char*>(message->payload), message->payloadlen);
        
        std::cout << "[MQTT] Received from " << topic << ": " << payload << std::endl;
        client->message_callback_(topic, payload);
    }
}

