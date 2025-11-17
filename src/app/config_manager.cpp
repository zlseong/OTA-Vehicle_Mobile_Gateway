/**
 * @file config_manager.cpp
 * @brief Configuration Management Module Implementation
 */

#include "config_manager.hpp"
#include <fstream>
#include <iostream>

ConfigManager::ConfigManager(const std::string& config_file)
    : config_file_(config_file), loaded_(false) {
}

bool ConfigManager::load() {
    std::ifstream file(config_file_);
    if (!file.is_open()) {
        std::cerr << "[CONFIG] Failed to open: " << config_file_ << std::endl;
        return false;
    }
    
    try {
        file >> config_;
        loaded_ = true;
        std::cout << "[CONFIG] ✓ Loaded from " << config_file_ << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[CONFIG] Parse error: " << e.what() << std::endl;
        return false;
    }
}

// ========================================
// Server Configuration
// ========================================

std::string ConfigManager::getServerHost() const {
    return config_["server"]["host"];
}

int ConfigManager::getHttpPort() const {
    return config_["server"]["http"]["port"];
}

int ConfigManager::getMqttPort() const {
    return config_["server"]["mqtt"]["port"];
}

bool ConfigManager::useHttps() const {
    return config_["server"]["http"]["use_https"];
}

bool ConfigManager::useMqttTls() const {
    return config_["server"]["mqtt"]["use_tls"];
}

std::string ConfigManager::getApiBase() const {
    return config_["server"]["http"]["api_base"];
}

int ConfigManager::getMqttKeepAlive() const {
    return config_["server"]["mqtt"]["keep_alive_sec"];
}

bool ConfigManager::getMqttCleanSession() const {
    return config_["server"]["mqtt"]["clean_session"];
}

int ConfigManager::getMqttQos() const {
    return config_["server"]["mqtt"]["qos"];
}

// Topics
std::string ConfigManager::getCommandTopic(const std::string& device_id) const {
    std::string topic = config_["server"]["mqtt"]["topics"]["command"];
    return replacePlaceholder(topic, "{device_id}", device_id);
}

std::string ConfigManager::getStatusTopic(const std::string& device_id) const {
    std::string topic = config_["server"]["mqtt"]["topics"]["status"];
    return replacePlaceholder(topic, "{device_id}", device_id);
}

std::string ConfigManager::getOtaTopic(const std::string& device_id) const {
    std::string topic = config_["server"]["mqtt"]["topics"]["ota"];
    return replacePlaceholder(topic, "{device_id}", device_id);
}

std::string ConfigManager::getVciTopic(const std::string& device_id) const {
    std::string topic = config_["server"]["mqtt"]["topics"]["vci"];
    return replacePlaceholder(topic, "{device_id}", device_id);
}

std::string ConfigManager::getReadinessTopic(const std::string& device_id) const {
    std::string topic = config_["server"]["mqtt"]["topics"]["readiness"];
    return replacePlaceholder(topic, "{device_id}", device_id);
}

// Endpoints
std::string ConfigManager::getHealthEndpoint() const {
    return config_["server"]["http"]["endpoints"]["health"];
}

std::string ConfigManager::getVciUploadEndpoint() const {
    return config_["server"]["http"]["endpoints"]["vci_upload"];
}

std::string ConfigManager::getOtaCheckEndpoint() const {
    return config_["server"]["http"]["endpoints"]["ota_check"];
}

std::string ConfigManager::getOtaDownloadEndpoint(const std::string& package_id) const {
    std::string endpoint = config_["server"]["http"]["endpoints"]["ota_download"];
    return replacePlaceholder(endpoint, "{package_id}", package_id);
}

std::string ConfigManager::getOtaStatusEndpoint() const {
    return config_["server"]["http"]["endpoints"]["ota_status"];
}

// ========================================
// Vehicle Configuration
// ========================================

std::string ConfigManager::getVin() const {
    return config_["vehicle"]["vin"];
}

std::string ConfigManager::getVehicleModel() const {
    return config_["vehicle"]["model"];
}

int ConfigManager::getModelYear() const {
    return config_["vehicle"]["model_year"];
}

// ========================================
// Device Configuration
// ========================================

std::string ConfigManager::getDeviceId() const {
    return config_["device"]["id"];
}

std::string ConfigManager::getDeviceName() const {
    return config_["device"]["name"];
}

std::string ConfigManager::getDeviceType() const {
    return config_["device"]["type"];
}

std::string ConfigManager::getHardwareVersion() const {
    return config_["device"]["hardware_version"];
}

std::string ConfigManager::getSoftwareVersion() const {
    return config_["device"]["software_version"];
}

// ========================================
// ZGW Configuration
// ========================================

std::string ConfigManager::getZgwIp() const {
    return config_["zgw"]["ip_address"];
}

int ConfigManager::getZgwDoipPort() const {
    return config_["zgw"]["doip_port"];
}

uint16_t ConfigManager::getZgwLogicalAddress() const {
    return config_["zgw"]["logical_address"];
}

uint16_t ConfigManager::getVciDid() const {
    return config_["zgw"]["uds"]["read_vci_did"];
}

uint16_t ConfigManager::getReadinessDid() const {
    return config_["zgw"]["uds"]["read_readiness_did"];
}

// ========================================
// TLS Configuration
// ========================================

bool ConfigManager::verifyPeer() const {
    return config_["tls"]["verify_peer"];
}

std::string ConfigManager::getCaCert() const {
    return config_["tls"]["ca_cert"];
}

std::string ConfigManager::getClientCert() const {
    return config_["tls"]["client_cert"];
}

std::string ConfigManager::getClientKey() const {
    return config_["tls"]["client_key"];
}

// ========================================
// PQC Configuration
// ========================================

bool ConfigManager::isPqcEnabled() const {
    return config_["pqc"]["enabled"];
}

std::string ConfigManager::getKemAlgorithm() const {
    return config_["pqc"]["kem_algorithm"];
}

std::string ConfigManager::getSigAlgorithm() const {
    return config_["pqc"]["signature_algorithm"];
}

bool ConfigManager::isHybridMode() const {
    return config_["pqc"]["hybrid_mode"];
}

// ========================================
// Monitoring Configuration
// ========================================

bool ConfigManager::isHeartbeatEnabled() const {
    return config_["monitoring"]["heartbeat_enabled"];
}

int ConfigManager::getHeartbeatInterval() const {
    return config_["monitoring"]["heartbeat_interval_sec"];
}

bool ConfigManager::isAdaptiveHeartbeat() const {
    return config_["monitoring"]["adaptive_heartbeat"];
}

bool ConfigManager::isEventDrivenReporting() const {
    return config_["monitoring"]["event_driven_reporting"];
}

int ConfigManager::getHeartbeatInterval(const std::string& state) const {
    return config_["monitoring"]["states"][state];
}

// ========================================
// Readiness Configuration
// ========================================

int ConfigManager::getMinBatteryPercent() const {
    return config_["readiness"]["min_battery_percent"];
}

int ConfigManager::getMinFreeSpaceMb() const {
    return config_["readiness"]["min_free_space_mb"];
}

int ConfigManager::getMaxTemperatureCelsius() const {
    return config_["readiness"]["max_temperature_celsius"];
}

bool ConfigManager::checkEngineOff() const {
    return config_["readiness"]["check_engine_off"];
}

bool ConfigManager::checkParkingBrake() const {
    return config_["readiness"]["check_parking_brake"];
}

bool ConfigManager::checkNetworkStable() const {
    return config_["readiness"]["check_network_stable"];
}

// ========================================
// OTA Configuration
// ========================================

std::string ConfigManager::getOtaDownloadPath() const {
    return config_["ota"]["download_path"];
}

std::string ConfigManager::getOtaInstallPath() const {
    return config_["ota"]["install_path"];
}

std::string ConfigManager::getOtaBackupPath() const {
    return config_["ota"]["backup_path"];
}

int ConfigManager::getMaxPackageSizeMb() const {
    return config_["ota"]["max_package_size_mb"];
}

std::string ConfigManager::getPartitionAPath() const {
    return config_["ota"]["dual_partition"]["partition_a_path"];
}

std::string ConfigManager::getPartitionBPath() const {
    return config_["ota"]["dual_partition"]["partition_b_path"];
}

std::string ConfigManager::getBootStatusPath() const {
    return config_["ota"]["dual_partition"]["boot_flag_path"];
}

// ========================================
// Logging Configuration
// ========================================

std::string ConfigManager::getLogLevel() const {
    return config_["logging"]["level"];
}

std::string ConfigManager::getLogFile() const {
    return config_["logging"]["file"];
}

bool ConfigManager::isConsoleOutputEnabled() const {
    return config_["logging"]["console_output"];
}

// ========================================
// Helper Functions
// ========================================

std::string ConfigManager::replacePlaceholder(const std::string& str, const std::string& placeholder, const std::string& value) const {
    std::string result = str;
    size_t pos = result.find(placeholder);
    if (pos != std::string::npos) {
        result.replace(pos, placeholder.length(), value);
    }
    return result;
}

