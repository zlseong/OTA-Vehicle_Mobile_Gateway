/**
 * @file config_manager.hpp
 * @brief Configuration Management Module
 * 
 * Loads and manages VMG configuration from JSON file
 */

#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <nlohmann/json.hpp>

/**
 * @brief Configuration Manager Class
 */
class ConfigManager {
public:
    /**
     * @brief Constructor
     * @param config_file Path to config.json
     */
    explicit ConfigManager(const std::string& config_file = "config.json");
    
    /**
     * @brief Load configuration from file
     * @return true if successful, false otherwise
     */
    bool load();
    
    /**
     * @brief Check if configuration is loaded
     */
    bool isLoaded() const { return loaded_; }
    
    // ========================================
    // Server Configuration
    // ========================================
    
    std::string getServerHost() const;
    int getHttpPort() const;
    int getMqttPort() const;
    bool useHttps() const;
    bool useMqttTls() const;
    std::string getApiBase() const;
    
    // MQTT Settings
    int getMqttKeepAlive() const;
    bool getMqttCleanSession() const;
    int getMqttQos() const;
    
    // Topics
    std::string getCommandTopic(const std::string& device_id) const;
    std::string getStatusTopic(const std::string& device_id) const;
    std::string getOtaTopic(const std::string& device_id) const;
    std::string getVciTopic(const std::string& device_id) const;
    std::string getReadinessTopic(const std::string& device_id) const;
    
    // Endpoints
    std::string getHealthEndpoint() const;
    std::string getVciUploadEndpoint() const;
    std::string getOtaCheckEndpoint() const;
    std::string getOtaDownloadEndpoint(const std::string& package_id) const;
    std::string getOtaStatusEndpoint() const;
    
    // ========================================
    // Vehicle Configuration
    // ========================================
    
    std::string getVin() const;
    std::string getVehicleModel() const;
    int getModelYear() const;
    
    // ========================================
    // Device Configuration
    // ========================================
    
    std::string getDeviceId() const;
    std::string getDeviceName() const;
    std::string getDeviceType() const;
    std::string getHardwareVersion() const;
    std::string getSoftwareVersion() const;
    
    // ========================================
    // ZGW Configuration
    // ========================================
    
    std::string getZgwIp() const;
    int getZgwDoipPort() const;
    uint16_t getZgwLogicalAddress() const;
    uint16_t getVciDid() const;
    uint16_t getReadinessDid() const;
    
    // ========================================
    // TLS Configuration
    // ========================================
    
    bool verifyPeer() const;
    std::string getCaCert() const;
    std::string getClientCert() const;
    std::string getClientKey() const;
    
    // ========================================
    // PQC Configuration
    // ========================================
    
    bool isPqcEnabled() const;
    std::string getKemAlgorithm() const;
    std::string getSigAlgorithm() const;
    bool isHybridMode() const;
    
    // ========================================
    // Monitoring Configuration
    // ========================================
    
    bool isHeartbeatEnabled() const;
    int getHeartbeatInterval() const;
    bool isAdaptiveHeartbeat() const;
    bool isEventDrivenReporting() const;
    
    // State-specific intervals
    int getHeartbeatInterval(const std::string& state) const;
    
    // ========================================
    // Readiness Configuration
    // ========================================
    
    int getMinBatteryPercent() const;
    int getMinFreeSpaceMb() const;
    int getMaxTemperatureCelsius() const;
    bool checkEngineOff() const;
    bool checkParkingBrake() const;
    bool checkNetworkStable() const;
    
    // ========================================
    // OTA Configuration
    // ========================================
    
    std::string getOtaDownloadPath() const;
    std::string getOtaInstallPath() const;
    std::string getOtaBackupPath() const;
    int getMaxPackageSizeMb() const;
    
    // Dual Partition paths (simulation mode)
    std::string getPartitionAPath() const;
    std::string getPartitionBPath() const;
    std::string getBootStatusPath() const;
    
    // ========================================
    // Logging Configuration
    // ========================================
    
    std::string getLogLevel() const;
    std::string getLogFile() const;
    bool isConsoleOutputEnabled() const;
    
    // ========================================
    // Raw JSON Access (for advanced use)
    // ========================================
    
    const nlohmann::json& getRawConfig() const { return config_; }

private:
    std::string config_file_;
    nlohmann::json config_;
    bool loaded_;
    
    std::string replacePlaceholder(const std::string& str, const std::string& placeholder, const std::string& value) const;
};

#endif // CONFIG_MANAGER_HPP

