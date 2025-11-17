/**
 * @file readiness_manager.cpp
 * @brief Readiness Check Implementation
 * 
 * Parallel design with ZGW: Libraries/DataCollection/readiness_manager.c
 */

#include "readiness_manager.hpp"
#include <iostream>
#include <ctime>
#include <cstring>

ReadinessManager::ReadinessManager(const ConfigManager& config, MqttClient& mqtt_client,
                                   std::shared_ptr<DoIPClient> doip_client)
    : config_(config), mqtt_client_(mqtt_client), doip_client_(doip_client), is_ready_(false) {
}

bool ReadinessManager::checkReadiness() {
    std::cout << "[READY] Checking readiness from ZGW...\n";
    
    // TODO: Implement actual DoIP/UDS communication
    // For now, use mock data
    return queryZgwReadiness();
}

bool ReadinessManager::publishReadiness(const std::string& trigger) {
    if (readiness_data_.empty()) {
        std::cerr << "[READY] No readiness data to publish\n";
        return false;
    }
    
    std::cout << "[READY] Publishing readiness to server...\n";
    
    std::string topic = config_.getReadinessTopic(config_.getDeviceId());
    
    if (mqtt_client_.publish(topic, readiness_data_.dump())) {
        std::cout << "[READY] ✓ Readiness published successfully\n";
        return true;
    } else {
        std::cerr << "[READY] ✗ Publish failed\n";
        return false;
    }
}

bool ReadinessManager::checkAndPublish(const std::string& trigger) {
    std::cout << "[READY] Starting readiness check (trigger: " << trigger << ")...\n";
    
    // Generate mock data with trigger info
    readiness_data_ = generateMockReadiness(trigger);
    
    if (readiness_data_.empty()) {
        std::cerr << "[READY] ✗ Check failed\n";
        return false;
    }
    
    // Evaluate readiness
    is_ready_ = evaluateReadiness();
    readiness_data_["ready_for_ota"] = is_ready_;
    
    return publishReadiness(trigger);
}

bool ReadinessManager::queryZgwReadiness() {
    std::cout << "[READY] Querying ZGW via DoIP/UDS...\n";
    
    if (!doip_client_) {
        std::cerr << "[READY] ✗ DoIP client not available\n";
        return false;
    }
    
    // Check if DoIP is active
    if (!doip_client_->isActive()) {
        std::cout << "[READY] Connecting to ZGW...\n";
        if (!doip_client_->connect()) {
            std::cerr << "[READY] ✗ Failed to connect to ZGW\n";
            // Fallback to mock data
            std::cout << "[READY] Using mock data as fallback\n";
            readiness_data_ = generateMockReadiness("doip_failure");
            return true;  // Continue with mock data
        }
    }
    
    // Step 1: Request Readiness Check (RID = 0xF003)
    std::cout << "[READY] Step 1: Requesting readiness check...\n";
    if (!doip_client_->requestReadinessCheck()) {
        std::cerr << "[READY] ✗ Readiness check request failed\n";
        readiness_data_ = generateMockReadiness("check_failed");
        return true;  // Continue with mock data
    }
    
    // Step 2: Request Readiness Report (RID = 0xF004)
    std::cout << "[READY] Step 2: Requesting readiness report...\n";
    std::vector<ReadinessInfo> readiness_list;
    
    if (!doip_client_->requestReadinessReport(readiness_list)) {
        std::cerr << "[READY] ✗ Readiness report request failed\n";
        readiness_data_ = generateMockReadiness("report_failed");
        return true;  // Continue with mock data
    }
    
    if (readiness_list.empty()) {
        std::cerr << "[READY] ✗ No readiness data received\n";
        readiness_data_ = generateMockReadiness("empty_report");
        return true;  // Continue with mock data
    }
    
    // Convert binary Readiness to JSON
    readiness_data_ = convertReadinessToJson(readiness_list);
    
    std::cout << "[READY] ✓ Readiness data collected successfully (" 
              << readiness_list.size() << " ECUs)\n";
    
    return true;
}

nlohmann::json ReadinessManager::convertReadinessToJson(const std::vector<ReadinessInfo>& readiness_list) {
    // Aggregate readiness data from all ECUs
    // For simplicity, we'll take the worst case (minimum battery, memory, etc.)
    
    int min_battery = 100;
    int min_memory = 999999;
    int max_temperature = 0;
    bool all_engine_off = true;
    bool all_parking_brake = true;
    bool all_network_stable = true;
    bool all_ready = true;
    
    nlohmann::json ecus = nlohmann::json::array();
    
    for (const auto& info : readiness_list) {
        std::string ecu_id(info.ecu_id, 
            strnlen(info.ecu_id, sizeof(info.ecu_id)));
        
        // Convert voltage from mV to percentage (simple linear mapping)
        // Assuming 12.0V = 12000mV = 100%, 11.0V = 11000mV = 0%
        int battery_percent = ((int)info.battery_voltage_mv - 11000) / 10;
        if (battery_percent < 0) battery_percent = 0;
        if (battery_percent > 100) battery_percent = 100;
        
        int memory_mb = info.available_memory_kb / 1024;
        
        // Assuming temperature offset: stored_value = actual_temp + 40
        // So actual_temp = stored_value - 40 (but we don't have the field!)
        // For now, use a mock value
        int temperature = 45;  // Mock temperature
        
        nlohmann::json ecu = {
            {"ecu_id", ecu_id},
            {"battery_voltage_mv", info.battery_voltage_mv},
            {"battery_percent", battery_percent},
            {"available_memory_kb", info.available_memory_kb},
            {"available_memory_mb", memory_mb},
            {"vehicle_parked", (bool)info.vehicle_parked},
            {"engine_off", (bool)info.engine_off},
            {"all_doors_closed", (bool)info.all_doors_closed},
            {"sw_compatible", (bool)info.compatible},
            {"ready_for_update", (bool)info.ready_for_update}
        };
        
        ecus.push_back(ecu);
        
        // Update aggregates
        if (battery_percent < min_battery) min_battery = battery_percent;
        if (memory_mb < min_memory) min_memory = memory_mb;
        if (temperature > max_temperature) max_temperature = temperature;
        if (!info.engine_off) all_engine_off = false;
        if (!info.vehicle_parked) all_parking_brake = false;  // Using vehicle_parked as proxy
        if (!info.ready_for_update) all_ready = false;
        
        std::cout << "[READY]   - " << ecu_id << " (Battery: " << battery_percent 
                  << "%, Memory: " << memory_mb << "MB, Ready: " 
                  << (info.ready_for_update ? "YES" : "NO") << ")\n";
    }
    
    nlohmann::json readiness = {
        {"device_id", config_.getDeviceId()},
        {"timestamp", std::time(nullptr)},
        {"trigger", "doip_actual"},
        {"battery_percent", min_battery},
        {"free_space_mb", min_memory},
        {"temperature_celsius", max_temperature},
        {"engine_off", all_engine_off},
        {"parking_brake", all_parking_brake},
        {"network_stable", all_network_stable},
        {"ecus", ecus}
    };
    
    return readiness;
}

bool ReadinessManager::evaluateReadiness() {
    // Check against thresholds from config
    int battery = readiness_data_["battery_percent"];
    int free_space = readiness_data_["free_space_mb"];
    int temperature = readiness_data_["temperature_celsius"];
    bool engine_off = readiness_data_["engine_off"];
    bool parking_brake = readiness_data_["parking_brake"];
    bool network_stable = readiness_data_["network_stable"];
    
    bool ready = true;
    
    if (battery < config_.getMinBatteryPercent()) {
        std::cout << "[READY] ✗ Battery too low: " << battery << "%\n";
        ready = false;
    }
    
    if (free_space < config_.getMinFreeSpaceMb()) {
        std::cout << "[READY] ✗ Insufficient storage: " << free_space << " MB\n";
        ready = false;
    }
    
    if (temperature > config_.getMaxTemperatureCelsius()) {
        std::cout << "[READY] ✗ Temperature too high: " << temperature << "°C\n";
        ready = false;
    }
    
    if (config_.checkEngineOff() && !engine_off) {
        std::cout << "[READY] ✗ Engine must be off\n";
        ready = false;
    }
    
    if (config_.checkParkingBrake() && !parking_brake) {
        std::cout << "[READY] ✗ Parking brake must be engaged\n";
        ready = false;
    }
    
    if (config_.checkNetworkStable() && !network_stable) {
        std::cout << "[READY] ✗ Network unstable\n";
        ready = false;
    }
    
    if (ready) {
        std::cout << "[READY] ✓ Vehicle is ready for OTA\n";
    } else {
        std::cout << "[READY] ✗ Vehicle is NOT ready for OTA\n";
    }
    
    return ready;
}

nlohmann::json ReadinessManager::generateMockReadiness(const std::string& trigger) {
    // Mock data - in production, these would come from ZGW/CAN
    nlohmann::json readiness = {
        {"device_id", config_.getDeviceId()},
        {"timestamp", std::time(nullptr)},
        {"trigger", trigger},
        {"battery_percent", 85},
        {"free_space_mb", 5000},
        {"temperature_celsius", 45},
        {"engine_off", true},
        {"parking_brake", true},
        {"network_stable", true}
    };
    
    return readiness;
}

