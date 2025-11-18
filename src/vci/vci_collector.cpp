/**
 * @file vci_collector.cpp
 * @brief VCI Collection Implementation
 * 
 * Parallel design with ZGW: Libraries/DataCollection/vci_manager.c
 */

#include "vci_collector.hpp"
#include <iostream>
#include <ctime>
#include <cstring>

VCICollector::VCICollector(const ConfigManager& config, HttpClient& http_client,
                           std::shared_ptr<DoIPClient> doip_client)
    : config_(config), http_client_(http_client), doip_client_(doip_client) {
}

bool VCICollector::collect() {
    std::cout << "[VCI] Collecting VCI from ZGW...\n";
    
    // TODO: Implement actual DoIP/UDS communication
    // For now, use mock data
    return queryZgwVci();
}

bool VCICollector::upload() {
    if (vci_data_.empty()) {
        std::cerr << "[VCI] No VCI data to upload\n";
        return false;
    }
    
    std::cout << "[VCI] Uploading VCI to server...\n";
    
    std::string endpoint = config_.getVciUploadEndpoint();
    auto response = http_client_.postJson(endpoint, vci_data_.dump());
    
    if (response.success) {
        std::cout << "[VCI] ✓ VCI uploaded successfully\n";
        return true;
    } else {
        std::cerr << "[VCI] ✗ Upload failed: " << response.error << "\n";
        return false;
    }
}

bool VCICollector::collectAndUpload(const std::string& trigger) {
    std::cout << "[VCI] Starting VCI collection (trigger: " << trigger << ")...\n";
    
    // Generate mock data with trigger info
    vci_data_ = generateMockVci(trigger);
    
    if (vci_data_.empty()) {
        std::cerr << "[VCI] ✗ Collection failed\n";
        return false;
    }
    
    return upload();
}

bool VCICollector::queryZgwVci() {
    std::cout << "[VCI] Querying ZGW via DoIP/UDS...\n";
    
    if (!doip_client_) {
        std::cerr << "[VCI] ✗ DoIP client not available\n";
        return false;
    }
    
    // Check if DoIP is active
    if (!doip_client_->isActive()) {
        std::cout << "[VCI] Connecting to ZGW...\n";
        if (!doip_client_->connect()) {
            std::cerr << "[VCI] ✗ Failed to connect to ZGW\n";
            // Fallback to mock data
            std::cout << "[VCI] Using mock data as fallback\n";
            vci_data_ = generateMockVci("doip_failure");
            return true;  // Continue with mock data
        }
    }
    
    // Step 1: Request VCI Collection (RID = 0xF001)
    std::cout << "[VCI] Step 1: Requesting VCI collection...\n";
    if (!doip_client_->requestVCICollection()) {
        std::cerr << "[VCI] ✗ VCI collection request failed\n";
        vci_data_ = generateMockVci("collection_failed");
        return true;  // Continue with mock data
    }
    
    // Step 2: Request VCI Report (RID = 0xF002)
    std::cout << "[VCI] Step 2: Requesting VCI report...\n";
    std::vector<VCIInfo> vci_list;
    
    if (!doip_client_->requestVCIReport(vci_list)) {
        std::cerr << "[VCI] ✗ VCI report request failed\n";
        vci_data_ = generateMockVci("report_failed");
        return true;  // Continue with mock data
    }
    
    if (vci_list.empty()) {
        std::cerr << "[VCI] ✗ No VCI data received\n";
        vci_data_ = generateMockVci("empty_report");
        return true;  // Continue with mock data
    }
    
    // Convert binary VCI to JSON
    vci_data_ = convertVciToJson(vci_list);
    
    std::cout << "[VCI] ✓ VCI data collected successfully (" 
              << vci_list.size() << " ECUs)\n";
    
    return true;
}

nlohmann::json VCICollector::convertVciToJson(const std::vector<VCIInfo>& vci_list) {
    nlohmann::json vci = {
        {"device_id", config_.getDeviceId()},
        {"vin", config_.getVin()},
        {"timestamp", std::time(nullptr)},
        {"trigger", "doip_actual"},
        {"ecus", nlohmann::json::array()}
    };
    
    for (const auto& vci_info : vci_list) {
        // Extract strings from fixed-size char arrays
        std::string ecu_id(vci_info.ecu_id, 
            strnlen(vci_info.ecu_id, sizeof(vci_info.ecu_id)));
        std::string sw_version(vci_info.sw_version, 
            strnlen(vci_info.sw_version, sizeof(vci_info.sw_version)));
        std::string hw_version(vci_info.hw_version, 
            strnlen(vci_info.hw_version, sizeof(vci_info.hw_version)));
        std::string serial_num(vci_info.serial_num, 
            strnlen(vci_info.serial_num, sizeof(vci_info.serial_num)));
        
        nlohmann::json ecu = {
            {"ecu_id", ecu_id},
            {"sw_version", sw_version},
            {"hw_version", hw_version},
            {"serial_number", serial_num},
            {"part_number", "95910-S9000"},  // TODO: Get from ECU if available
            {"supplier", "HYUNDAI MOBIS"}    // TODO: Get from ECU if available
        };
        
        vci["ecus"].push_back(ecu);
        
        std::cout << "[VCI]   - " << ecu_id << " (SW: " << sw_version 
                  << ", HW: " << hw_version << ")\n";
    }
    
    return vci;
}

nlohmann::json VCICollector::generateMockVci(const std::string& trigger) {
    nlohmann::json vci = {
        {"device_id", config_.getDeviceId()},
        {"vin", config_.getVin()},
        {"timestamp", std::time(nullptr)},
        {"trigger", trigger},
        {"ecus", nlohmann::json::array({
            {
                {"ecu_id", "ECU_011"},
                {"sw_version", "1.1.2"},
                {"hw_version", "2.0"},
                {"part_number", "95910-S9000"},
                {"supplier", "HYUNDAI MOBIS"}
            },
            {
                {"ecu_id", "ECU_021"},
                {"sw_version", "1.0.5"},
                {"hw_version", "1.5"},
                {"part_number", "95910-S9010"},
                {"supplier", "HYUNDAI MOBIS"}
            },
            {
                {"ecu_id", "ECU_031"},
                {"sw_version", "2.3.1"},
                {"hw_version", "3.0"},
                {"part_number", "95910-S9020"},
                {"supplier", "LG ELECTRONICS"}
            }
        })}
    };
    
    return vci;
}

