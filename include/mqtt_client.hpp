/**
 * @file mqtt_client.hpp
 * @brief MQTT Client for VMG (Paho MQTT C++)
 * 
 * Implements OTA-Server MQTT API:
 * - Topics: oem/{vin}/* 
 * - Message types: vehicle_wake_up, vci_report, ota_readiness_response, etc.
 */

#ifndef MQTT_CLIENT_HPP
#define MQTT_CLIENT_HPP

#include <string>
#include <functional>
#include <memory>
#include <mqtt/async_client.h>

using MqttMessageCallback = std::function<void(const std::string&, const std::string&)>;

/**
 * @brief MQTT Client wrapper for Paho MQTT C++
 */
class MqttClient {
public:
    /**
     * @brief Constructor
     * @param host MQTT broker host
     * @param port MQTT broker port
     * @param client_id MQTT client ID
     * @param vin Vehicle VIN (for topic generation)
     * @param use_tls Enable TLS
     * @param verify_peer Verify SSL peer
     */
    MqttClient(const std::string& host, int port, const std::string& client_id,
               const std::string& vin = "",
               bool use_tls = false, bool verify_peer = true);
    ~MqttClient();
    
    /**
     * @brief Connect to MQTT broker
     */
    bool connect();
    
    /**
     * @brief Disconnect from broker
     */
    void disconnect();
    
    /**
     * @brief Check if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Subscribe to topic
     */
    bool subscribe(const std::string& topic, int qos = 1);
    
    /**
     * @brief Publish message
     */
    bool publish(const std::string& topic, const std::string& payload, int qos = 1);
    
    /**
     * @brief Process incoming messages (non-blocking)
     */
    void loop(int timeout_ms = 100);
    
    /**
     * @brief Set message callback
     */
    void setMessageCallback(MqttMessageCallback callback);
    
    /**
     * @brief Set VIN (for topic generation)
     */
    void setVin(const std::string& vin) { vin_ = vin; }
    
    // ========================================
    // OTA-Server API Methods
    // ========================================
    
    /**
     * @brief Send vehicle wake-up message
     */
    bool sendWakeUp(const std::string& vmg_sw_version, const std::string& vehicle_state);
    
    /**
     * @brief Send VCI report
     */
    bool sendVciReport(const std::string& vci_json);
    
    /**
     * @brief Send OTA readiness response
     */
    bool sendReadinessResponse(const std::string& readiness_json);
    
    /**
     * @brief Send OTA download progress
     */
    bool sendDownloadProgress(const std::string& campaign_id, int percentage, 
                              int bytes_downloaded, int total_bytes);
    
    /**
     * @brief Send heartbeat (status update)
     */
    bool sendHeartbeat(const std::string& vehicle_state, int uptime_sec);

private:
    std::string host_;
    int port_;
    std::string client_id_;
    std::string vin_;
    bool use_tls_;
    bool verify_peer_;
    
    std::unique_ptr<mqtt::async_client> client_;
    MqttMessageCallback message_callback_;
    
    /**
     * @brief Callback class for Paho MQTT
     */
    class Callback : public virtual mqtt::callback {
    public:
        Callback(MqttClient* parent) : parent_(parent) {}
        
        void connection_lost(const std::string& cause) override;
        void message_arrived(mqtt::const_message_ptr msg) override;
        void delivery_complete(mqtt::delivery_token_ptr token) override;
        
    private:
        MqttClient* parent_;
    };
    
    std::unique_ptr<Callback> callback_;
    
    /**
     * @brief Generate topic with VIN
     */
    std::string getTopic(const std::string& suffix) const;
};

#endif // MQTT_CLIENT_HPP
