/**
 * @file ota_manager_vehicle.cpp
 * @brief OTA Manager - Vehicle Package Processing Implementation
 * 
 * VMG's role as Package Distributor:
 *   1. Receive Vehicle Package from Server (HTTPS)
 *   2. Parse metadata and extract Zone Packages
 *   3. Route each Zone Package to target ZGW (DoIP/UDS)
 * 
 * @version 1.0
 * @date 2024-11-17
 */

#include "ota_manager.hpp"
#include "zone_package.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

// ==================== Vehicle OTA Flow ====================

bool OTAManager::startVehicleOTA(const OTAPackageInfo& package_info) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          Vehicle OTA Update (3-Layer Package)             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "[VehicleOTA] Campaign ID: " << package_info.campaign_id << "\n";
    std::cout << "[VehicleOTA] Package URL: " << package_info.package_url << "\n";
    std::cout << "[VehicleOTA] Package Size: " << package_info.package_size << " bytes\n";
    std::cout << "════════════════════════════════════════════════════════════\n\n";
    
    // Check if OTA already in progress
    if (isOTAInProgress()) {
        std::cerr << "[VehicleOTA] ✗ OTA already in progress\n";
        return false;
    }
    
    // Store package info
    package_info_ = package_info;
    
    // Reset progress
    std::memset(&progress_, 0, sizeof(OTAProgress));
    progress_.total_bytes = package_info.package_size;
    
    // Step 1: Download Vehicle Package from Server
    updateState(OTAState::OTA_DOWNLOADING, "Downloading Vehicle Package from Server");
    if (!downloadVehiclePackage()) {
        reportError("Failed to download Vehicle Package");
        return false;
    }
    
    // Step 2: Parse Vehicle Package metadata
    updateState(OTAState::OTA_VERIFYING, "Parsing Vehicle Package metadata");
    std::string vehicle_package_path = download_path_ + "/" + package_info_.campaign_id + ".bin";
    vehicle_parser_ = std::make_unique<VehiclePackageParser>(vehicle_package_path);
    
    if (!vehicle_parser_->parse()) {
        reportError("Failed to parse Vehicle Package");
        return false;
    }
    
    // Step 3: Verify Vehicle Package integrity
    if (!vehicle_parser_->verify()) {
        reportError("Vehicle Package integrity check failed");
        return false;
    }
    
    // Step 4: Verify target vehicle (VIN, Model, Year)
    if (!verifyVehiclePackageTarget()) {
        reportError("Vehicle Package target mismatch");
        return false;
    }
    
    // Step 5: Extract Zone Packages
    updateState(OTAState::OTA_INSTALLING, "Extracting Zone Packages");
    if (!extractZonePackages()) {
        reportError("Failed to extract Zone Packages");
        return false;
    }
    
    // Step 6: Send each Zone Package to target ZGW
    zone_packages_ = vehicle_parser_->getZonePackages();
    
    std::cout << "\n[VehicleOTA] Sending Zone Packages to ZGWs...\n";
    std::cout << "════════════════════════════════════════════════════════════\n";
    
    for (size_t i = 0; i < zone_packages_.size(); i++) {
        const auto& zone = zone_packages_[i];
        
        std::cout << "\n[VehicleOTA] [" << (i+1) << "/" << zone_packages_.size() << "] "
                  << "Sending Zone " << (int)zone.zone_number << " (" << zone.zone_id << ")...\n";
        std::cout << "[VehicleOTA]   Target: " << zone.target_zgw_ip << ":" << zone.target_zgw_port << "\n";
        std::cout << "[VehicleOTA]   ECUs: " << (int)zone.ecu_count << "\n";
        std::cout << "[VehicleOTA]   Size: " << zone.size << " bytes\n";
        
        if (!sendZonePackageToZGW(zone)) {
            std::cerr << "[VehicleOTA] ✗ Failed to send Zone " << (int)zone.zone_number << "\n";
            reportError("Failed to send Zone Package to ZGW");
            return false;
        }
        
        std::cout << "[VehicleOTA] ✓ Zone " << (int)zone.zone_number << " sent successfully\n";
        
        // Report progress
        uint32_t zones_completed = i + 1;
        uint8_t percentage = (zones_completed * 100) / zone_packages_.size();
        progress_.percentage = percentage;
        sendProgressReport();
    }
    
    // Step 7: OTA Completed
    updateState(OTAState::OTA_COMPLETED, "All Zone Packages sent to ZGWs");
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║        ✓ Vehicle OTA Completed Successfully!              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "[VehicleOTA] Zone Packages sent: " << zone_packages_.size() << "\n";
    std::cout << "[VehicleOTA] Total ECUs updated: " << (int)vehicle_parser_->getMetadata().total_ecu_count << "\n";
    std::cout << "════════════════════════════════════════════════════════════\n\n";
    
    current_state_ = OTAState::OTA_COMPLETED;
    return true;
}

// ==================== Download Vehicle Package ====================

bool OTAManager::downloadVehiclePackage() {
    std::cout << "[VehicleOTA] Downloading Vehicle Package from Server...\n";
    std::cout << "[VehicleOTA]   URL: " << package_info_.package_url << "\n";
    
    // Use existing downloadPackage() logic
    // This downloads the entire Vehicle Package binary from Server via HTTPS
    return downloadPackage();
}

// ==================== Verify Vehicle Target ====================

bool OTAManager::verifyVehiclePackageTarget() {
    std::cout << "[VehicleOTA] Verifying Vehicle Package target...\n";
    
    // Get vehicle info from config
    std::string expected_vin = config_.getVin();
    std::string expected_model = config_.getVehicleModel();
    uint16_t expected_year = config_.getModelYear();
    
    std::cout << "[VehicleOTA]   Expected VIN: " << expected_vin << "\n";
    std::cout << "[VehicleOTA]   Expected Model: " << expected_model << " (" << expected_year << ")\n";
    
    // Verify using parser
    if (!vehicle_parser_->verifyVehicleTarget(expected_vin, expected_model, expected_year)) {
        std::cerr << "[VehicleOTA] ✗ Vehicle target mismatch\n";
        return false;
    }
    
    std::cout << "[VehicleOTA] ✓ Vehicle target verified\n";
    return true;
}

// ==================== Extract Zone Packages ====================

bool OTAManager::extractZonePackages() {
    std::cout << "[VehicleOTA] Extracting Zone Packages...\n";
    
    // Create extraction directory
    std::string extract_dir = download_path_ + "/zones";
    
    // Extract all Zone Packages
    if (!vehicle_parser_->extractAllZonePackages(extract_dir)) {
        std::cerr << "[VehicleOTA] ✗ Failed to extract Zone Packages\n";
        return false;
    }
    
    std::cout << "[VehicleOTA] ✓ All Zone Packages extracted\n";
    return true;
}

// ==================== Send Zone Package to ZGW ====================

bool OTAManager::sendZonePackageToZGW(const ZonePackageInfo& zone_info) {
    std::cout << "[ZoneTransfer] Sending Zone Package to ZGW...\n";
    std::cout << "[ZoneTransfer]   Zone: " << zone_info.zone_id 
              << " (Zone #" << (int)zone_info.zone_number << ")\n";
    std::cout << "[ZoneTransfer]   Target ZGW: " << zone_info.target_zgw_ip 
              << ":" << zone_info.target_zgw_port << "\n";
    
    // Get DoIP client for this ZGW
    DoIPClient* doip_client = getDoIPClientForZGW(zone_info.target_zgw_ip, 
                                                    zone_info.target_zgw_port);
    if (!doip_client) {
        std::cerr << "[ZoneTransfer] ✗ Failed to get DoIP client\n";
        return false;
    }
    
    // Connect to ZGW if not already connected
    if (!doip_client->isActive()) {
        std::cout << "[ZoneTransfer] Connecting to ZGW...\n";
        if (!doip_client->connect()) {
            std::cerr << "[ZoneTransfer] ✗ Failed to connect to ZGW\n";
            return false;
        }
        std::cout << "[ZoneTransfer] ✓ Connected to ZGW\n";
    }
    
    // Parse Zone Package before sending
    ZonePackageParser zone_parser(zone_info.extracted_path);
    if (!zone_parser.parse()) {
        std::cerr << "[ZoneTransfer] ✗ Failed to parse Zone Package\n";
        return false;
    }
    
    if (!zone_parser.verify()) {
        std::cerr << "[ZoneTransfer] ✗ Zone Package integrity check failed\n";
        return false;
    }
    
    // Print Zone Package summary
    zone_parser.printSummary();
    
    // Transfer Zone Package via DoIP/UDS (0x34/0x36/0x37)
    if (!transferZonePackageViaUDS(doip_client, zone_info.extracted_path)) {
        std::cerr << "[ZoneTransfer] ✗ Failed to transfer Zone Package\n";
        return false;
    }
    
    std::cout << "[ZoneTransfer] ✓ Zone Package sent successfully\n";
    return true;
}

// ==================== Transfer Zone Package via UDS ====================

bool OTAManager::transferZonePackageViaUDS(DoIPClient* doip_client,
                                            const std::string& zone_package_path) {
    std::cout << "[UDS] Transferring Zone Package via UDS (0x34/0x36/0x37)...\n";
    
    // Open Zone Package file
    std::ifstream file(zone_package_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[UDS] ✗ Failed to open Zone Package file\n";
        return false;
    }
    
    // Get file size
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::cout << "[UDS] Zone Package size: " << file_size << " bytes\n";
    
    // Read entire file
    std::vector<uint8_t> zone_data(file_size);
    file.read(reinterpret_cast<char*>(zone_data.data()), file_size);
    file.close();
    
    // Step 1: Request Download (0x34)
    std::cout << "[UDS] Step 1: Request Download (0x34)...\n";
    
    // Build UDS 0x34 payload: [0x34] [total_size: 4 bytes]
    std::vector<uint8_t> request_download_payload;
    request_download_payload.push_back(0x34);  // Service ID
    
    // Add total size (big-endian)
    uint32_t total_size = static_cast<uint32_t>(file_size);
    request_download_payload.push_back((total_size >> 24) & 0xFF);
    request_download_payload.push_back((total_size >> 16) & 0xFF);
    request_download_payload.push_back((total_size >> 8) & 0xFF);
    request_download_payload.push_back(total_size & 0xFF);
    
    auto response = doip_client->sendDiagnosticMessage(0x34, request_download_payload);
    
    if (response.empty() || response[0] != 0x74) {  // 0x74 = positive response to 0x34
        std::cerr << "[UDS] ✗ Request Download failed\n";
        return false;
    }
    
    std::cout << "[UDS] ✓ Request Download accepted\n";
    
    // Step 2: Transfer Data (0x36) - Chunked
    std::cout << "[UDS] Step 2: Transfer Data (0x36) in chunks...\n";
    
    const size_t chunk_size = 1024;  // 1KB chunks
    uint8_t block_sequence = 1;
    size_t total_sent = 0;
    
    while (total_sent < file_size) {
        size_t remaining = file_size - total_sent;
        size_t current_chunk_size = std::min(chunk_size, remaining);
        
        // Build UDS 0x36 payload: [0x36] [block_sequence: 1 byte] [data: N bytes]
        std::vector<uint8_t> transfer_payload;
        transfer_payload.push_back(0x36);  // Service ID
        transfer_payload.push_back(block_sequence);
        
        // Append chunk data
        transfer_payload.insert(transfer_payload.end(),
                                zone_data.begin() + total_sent,
                                zone_data.begin() + total_sent + current_chunk_size);
        
        // Send chunk
        auto chunk_response = doip_client->sendDiagnosticMessage(0x36, transfer_payload);
        
        if (chunk_response.empty() || chunk_response[0] != 0x76) {  // 0x76 = positive response to 0x36
            std::cerr << "[UDS] ✗ Transfer Data failed at block " << (int)block_sequence << "\n";
            return false;
        }
        
        total_sent += current_chunk_size;
        block_sequence = (block_sequence + 1) % 256;
        
        // Print progress
        uint8_t progress = (total_sent * 100) / file_size;
        std::cout << "[UDS] Progress: " << (int)progress << "% (" 
                  << total_sent << "/" << file_size << " bytes)\r" << std::flush;
    }
    
    std::cout << "\n[UDS] ✓ All data blocks transferred\n";
    
    // Step 3: Request Transfer Exit (0x37)
    std::cout << "[UDS] Step 3: Request Transfer Exit (0x37)...\n";
    
    std::vector<uint8_t> exit_payload;
    exit_payload.push_back(0x37);  // Service ID
    
    auto exit_response = doip_client->sendDiagnosticMessage(0x37, exit_payload);
    
    if (exit_response.empty() || exit_response[0] != 0x77) {  // 0x77 = positive response to 0x37
        std::cerr << "[UDS] ✗ Transfer Exit failed\n";
        return false;
    }
    
    std::cout << "[UDS] ✓ Transfer Exit accepted\n";
    std::cout << "[UDS] ✓ Zone Package transfer completed\n";
    
    return true;
}

// ==================== Get DoIP Client for ZGW ====================

DoIPClient* OTAManager::getDoIPClientForZGW(const std::string& zgw_ip, uint16_t zgw_port) {
    // Search for existing client
    for (auto& client : doip_clients_) {
        // TODO: Add method to get IP/Port from DoIPClient
        // For now, create new client if not found
    }
    
    // Create new DoIP client
    std::cout << "[DoIP] Creating new DoIP client for " << zgw_ip << ":" << zgw_port << "\n";
    auto new_client = std::make_shared<DoIPClient>(zgw_ip, zgw_port);
    doip_clients_.push_back(new_client);
    
    return new_client.get();
}

