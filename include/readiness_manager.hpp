/**
 * @file readiness_manager.hpp
 * @brief Readiness Check Module
 * 
 * Checks vehicle readiness for OTA updates
 * Parallel design with ZGW: Libraries/DataCollection/readiness_manager.c
 */

#ifndef READINESS_MANAGER_HPP
#define READINESS_MANAGER_HPP

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "config_manager.hpp"
#include "mqtt_client.hpp"
#include "doip_client.hpp"

/**
 * @brief Readiness Manager Class
 */
class ReadinessManager {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager
     * @param mqtt_client MQTT client for publishing readiness
     * @param doip_client DoIP client for ZGW communication
     */
    ReadinessManager(const ConfigManager& config, MqttClient& mqtt_client,
                     std::shared_ptr<DoIPClient> doip_client);
    
    /**
     * @brief Check vehicle readiness for OTA
     * @return true if ready for OTA
     */
    bool checkReadiness();
    
    /**
     * @brief Publish readiness status to server via MQTT
     * @param trigger Trigger reason (e.g., "external_request")
     * @return true if successful
     */
    bool publishReadiness(const std::string& trigger = "manual");
    
    /**
     * @brief Check and publish readiness (convenience method)
     * @param trigger Trigger reason
     * @return true if successful
     */
    bool checkAndPublish(const std::string& trigger = "manual");
    
    /**
     * @brief Get last readiness result
     */
    bool isReady() const { return is_ready_; }
    
    /**
     * @brief Get detailed readiness data
     */
    const nlohmann::json& getReadinessData() const { return readiness_data_; }

private:
    const ConfigManager& config_;
    MqttClient& mqtt_client_;
    std::shared_ptr<DoIPClient> doip_client_;
    bool is_ready_;
    nlohmann::json readiness_data_;
    
    /**
     * @brief Query ZGW for readiness parameters via DoIP/UDS
     * @details Sends UDS Routine Control (0x31 01 F003/F004) to ZGW
     *          and receives Readiness Report (0x9001)
     */
    bool queryZgwReadiness();
    
    /**
     * @brief Convert binary Readiness data to JSON format
     * @param readiness_list Binary readiness data from ZGW
     * @return JSON formatted readiness data
     */
    nlohmann::json convertReadinessToJson(const std::vector<ReadinessInfo>& readiness_list);
    
    /**
     * @brief Evaluate readiness based on thresholds
     */
    bool evaluateReadiness();
    
    /**
     * @brief Generate mock readiness data for testing (fallback)
     */
    nlohmann::json generateMockReadiness(const std::string& trigger);
};

#endif // READINESS_MANAGER_HPP

