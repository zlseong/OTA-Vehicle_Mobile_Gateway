#pragma once

#include <string>
#include <functional>
#include <mosquitto.h>

class MqttClient {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
    
    MqttClient(const std::string& broker_host, 
               int broker_port,
               const std::string& client_id,
               bool use_tls = false,
               bool verify_peer = false);
    ~MqttClient();
    
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }
    
    bool publish(const std::string& topic, const std::string& payload, int qos = 1);
    bool subscribe(const std::string& topic, int qos = 1);
    
    void setMessageCallback(MessageCallback callback) { message_callback_ = callback; }
    
    void loop(int timeout_ms = 1000);

private:
    struct mosquitto* mosq_;
    std::string broker_host_;
    int broker_port_;
    std::string client_id_;
    bool use_tls_;
    bool verify_peer_;
    bool connected_;
    MessageCallback message_callback_;
    
    static void onConnect(struct mosquitto* mosq, void* obj, int result);
    static void onDisconnect(struct mosquitto* mosq, void* obj, int result);
    static void onMessage(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message);
};

