/**
 * @file vehicle_package_parser.cpp
 * @brief Vehicle Package Parser Implementation
 * 
 * Parse Vehicle Package received from Server and extract Zone Packages
 * 
 * @version 1.0
 * @date 2024-11-17
 */

#include "vehicle_package.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

VehiclePackageParser::VehiclePackageParser(const std::string& package_path)
    : package_path_(package_path), parsed_(false) {
    std::memset(&metadata_, 0, sizeof(VehiclePackageMetadata));
}

bool VehiclePackageParser::parse() {
    std::cout << "[VehiclePackage] Parsing Vehicle Package: " << package_path_ << "\n";
    
    // Open package file
    std::ifstream file(package_path_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[VehiclePackage] ✗ Failed to open package file\n";
        return false;
    }
    
    // Read metadata (first 12KB)
    file.read(reinterpret_cast<char*>(&metadata_), sizeof(VehiclePackageMetadata));
    if (!file.good()) {
        std::cerr << "[VehiclePackage] ✗ Failed to read metadata\n";
        return false;
    }
    
    // Verify magic number
    if (metadata_.magic_number != VEHICLE_PACKAGE_MAGIC) {
        std::cerr << "[VehiclePackage] ✗ Invalid magic number: 0x" 
                  << std::hex << metadata_.magic_number << std::dec << "\n";
        return false;
    }
    
    std::cout << "[VehiclePackage] ✓ Magic number valid: 0x5650504B (\"VPPK\")\n";
    
    // Parse Zone References
    zone_packages_.clear();
    for (uint8_t i = 0; i < metadata_.zone_count; i++) {
        const ZoneReference& zone_ref = metadata_.zone_refs[i];
        
        ZonePackageInfo zone_info;
        zone_info.zone_id = std::string(zone_ref.zone_id, 16);
        zone_info.zone_id.erase(zone_info.zone_id.find('\0')); // Remove null padding
        zone_info.zone_number = zone_ref.zone_number;
        zone_info.offset = zone_ref.offset;
        zone_info.size = zone_ref.size;
        zone_info.ecu_count = zone_ref.ecu_count;
        
        // Determine target ZGW
        auto [zgw_ip, zgw_port] = determineZoneTarget(zone_ref.zone_number);
        zone_info.target_zgw_ip = zgw_ip;
        zone_info.target_zgw_port = zgw_port;
        
        zone_packages_.push_back(zone_info);
        
        std::cout << "[VehiclePackage]   Zone " << (int)zone_ref.zone_number 
                  << ": " << zone_info.zone_id 
                  << " (" << zone_ref.ecu_count << " ECUs, " 
                  << zone_ref.size << " bytes)\n";
        std::cout << "[VehiclePackage]      Target: " << zgw_ip << ":" << zgw_port << "\n";
    }
    
    file.close();
    parsed_ = true;
    
    std::cout << "[VehiclePackage] ✓ Vehicle Package parsed successfully\n";
    std::cout << "[VehiclePackage]   VIN: " << metadata_.vin << "\n";
    std::cout << "[VehiclePackage]   Model: " << metadata_.model << " (" << metadata_.model_year << ")\n";
    std::cout << "[VehiclePackage]   Master SW: " << metadata_.master_sw_string << "\n";
    std::cout << "[VehiclePackage]   Zones: " << (int)metadata_.zone_count << "\n";
    std::cout << "[VehiclePackage]   Total ECUs: " << (int)metadata_.total_ecu_count << "\n";
    
    return true;
}

bool VehiclePackageParser::verify() {
    if (!parsed_) {
        std::cerr << "[VehiclePackage] ✗ Package not parsed yet\n";
        return false;
    }
    
    std::cout << "[VehiclePackage] Verifying package integrity...\n";
    
    // Open file
    std::ifstream file(package_path_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[VehiclePackage] ✗ Failed to open file for verification\n";
        return false;
    }
    
    // Calculate CRC32 of entire package (excluding metadata CRC32 fields)
    file.seekg(sizeof(VehiclePackageMetadata), std::ios::beg);
    
    std::vector<uint8_t> package_data;
    package_data.resize(metadata_.total_size - sizeof(VehiclePackageMetadata));
    file.read(reinterpret_cast<char*>(package_data.data()), package_data.size());
    
    uint32_t calculated_crc = calculateCRC32(package_data.data(), package_data.size());
    
    if (calculated_crc != metadata_.vehicle_crc32) {
        std::cerr << "[VehiclePackage] ✗ CRC32 mismatch\n";
        std::cerr << "  Expected: 0x" << std::hex << metadata_.vehicle_crc32 << "\n";
        std::cerr << "  Calculated: 0x" << calculated_crc << std::dec << "\n";
        return false;
    }
    
    std::cout << "[VehiclePackage] ✓ CRC32 valid: 0x" << std::hex << calculated_crc << std::dec << "\n";
    
    file.close();
    return true;
}

bool VehiclePackageParser::verifyVehicleTarget(const std::string& vin,
                                                const std::string& model,
                                                uint16_t model_year) {
    std::cout << "[VehiclePackage] Verifying vehicle target...\n";
    
    std::string package_vin = std::string(metadata_.vin, 17);
    package_vin.erase(package_vin.find('\0'));
    
    std::string package_model = std::string(metadata_.model, 32);
    package_model.erase(package_model.find('\0'));
    
    if (package_vin != vin) {
        std::cerr << "[VehiclePackage] ✗ VIN mismatch\n";
        std::cerr << "  Expected: " << vin << "\n";
        std::cerr << "  Package: " << package_vin << "\n";
        return false;
    }
    
    if (package_model != model) {
        std::cerr << "[VehiclePackage] ✗ Model mismatch\n";
        std::cerr << "  Expected: " << model << "\n";
        std::cerr << "  Package: " << package_model << "\n";
        return false;
    }
    
    if (metadata_.model_year != model_year) {
        std::cerr << "[VehiclePackage] ✗ Model year mismatch\n";
        std::cerr << "  Expected: " << model_year << "\n";
        std::cerr << "  Package: " << metadata_.model_year << "\n";
        return false;
    }
    
    std::cout << "[VehiclePackage] ✓ Vehicle target verified\n";
    return true;
}

bool VehiclePackageParser::extractZonePackage(uint8_t zone_number, 
                                               const std::string& output_path) {
    if (!parsed_) {
        std::cerr << "[VehiclePackage] ✗ Package not parsed yet\n";
        return false;
    }
    
    // Find zone
    const ZoneReference* zone_ref = nullptr;
    for (uint8_t i = 0; i < metadata_.zone_count; i++) {
        if (metadata_.zone_refs[i].zone_number == zone_number) {
            zone_ref = &metadata_.zone_refs[i];
            break;
        }
    }
    
    if (!zone_ref) {
        std::cerr << "[VehiclePackage] ✗ Zone " << (int)zone_number << " not found\n";
        return false;
    }
    
    std::cout << "[VehiclePackage] Extracting Zone " << (int)zone_number 
              << " to " << output_path << "...\n";
    
    // Open source file
    std::ifstream src_file(package_path_, std::ios::binary);
    if (!src_file.is_open()) {
        std::cerr << "[VehiclePackage] ✗ Failed to open source file\n";
        return false;
    }
    
    // Open output file
    std::ofstream dst_file(output_path, std::ios::binary);
    if (!dst_file.is_open()) {
        std::cerr << "[VehiclePackage] ✗ Failed to create output file\n";
        return false;
    }
    
    // Seek to zone offset
    src_file.seekg(zone_ref->offset, std::ios::beg);
    
    // Copy zone data
    std::vector<uint8_t> zone_data(zone_ref->size);
    src_file.read(reinterpret_cast<char*>(zone_data.data()), zone_ref->size);
    dst_file.write(reinterpret_cast<const char*>(zone_data.data()), zone_ref->size);
    
    src_file.close();
    dst_file.close();
    
    std::cout << "[VehiclePackage] ✓ Zone " << (int)zone_number 
              << " extracted (" << zone_ref->size << " bytes)\n";
    
    // Update zone_packages_ with extracted path
    for (auto& zone : zone_packages_) {
        if (zone.zone_number == zone_number) {
            zone.extracted_path = output_path;
            break;
        }
    }
    
    return true;
}

bool VehiclePackageParser::extractAllZonePackages(const std::string& output_dir) {
    if (!parsed_) {
        std::cerr << "[VehiclePackage] ✗ Package not parsed yet\n";
        return false;
    }
    
    std::cout << "[VehiclePackage] Extracting all Zone Packages to " << output_dir << "...\n";
    
    // Create output directory
    mkdir(output_dir.c_str(), 0755);
    
    // Extract each zone
    for (uint8_t i = 0; i < metadata_.zone_count; i++) {
        uint8_t zone_num = metadata_.zone_refs[i].zone_number;
        std::string zone_id = std::string(metadata_.zone_refs[i].zone_id, 16);
        zone_id.erase(zone_id.find('\0'));
        
        std::string output_path = output_dir + "/zone_" + std::to_string((int)zone_num) + ".bin";
        
        if (!extractZonePackage(zone_num, output_path)) {
            std::cerr << "[VehiclePackage] ✗ Failed to extract Zone " << (int)zone_num << "\n";
            return false;
        }
    }
    
    std::cout << "[VehiclePackage] ✓ All Zone Packages extracted\n";
    return true;
}

ZonePackageInfo VehiclePackageParser::getZoneRoutingInfo(uint8_t zone_number) const {
    for (const auto& zone : zone_packages_) {
        if (zone.zone_number == zone_number) {
            return zone;
        }
    }
    
    // Return empty if not found
    ZonePackageInfo empty;
    empty.zone_number = 0;
    return empty;
}

void VehiclePackageParser::printSummary() const {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Vehicle Package Summary\n";
    std::cout << "========================================\n";
    std::cout << "VIN:           " << metadata_.vin << "\n";
    std::cout << "Model:         " << metadata_.model << " (" << metadata_.model_year << ")\n";
    std::cout << "Region:        " << (int)metadata_.region << "\n";
    std::cout << "Master SW:     " << metadata_.master_sw_string << "\n";
    std::cout << "Total Size:    " << metadata_.total_size << " bytes\n";
    std::cout << "Zone Count:    " << (int)metadata_.zone_count << "\n";
    std::cout << "Total ECUs:    " << (int)metadata_.total_ecu_count << "\n";
    std::cout << "\nZone Packages:\n";
    
    for (size_t i = 0; i < zone_packages_.size(); i++) {
        const auto& zone = zone_packages_[i];
        std::cout << "  [" << (i+1) << "] Zone " << (int)zone.zone_number 
                  << ": " << zone.zone_id << "\n";
        std::cout << "      ECUs: " << (int)zone.ecu_count << "\n";
        std::cout << "      Size: " << zone.size << " bytes\n";
        std::cout << "      Target: " << zone.target_zgw_ip << ":" << zone.target_zgw_port << "\n";
    }
    
    std::cout << "========================================\n\n";
}

uint32_t VehiclePackageParser::calculateCRC32(const uint8_t* data, size_t size) const {
    return crc32(0L, data, size);
}

std::pair<std::string, uint16_t> VehiclePackageParser::determineZoneTarget(uint8_t zone_number) const {
    // TODO: This should be configurable via routing table
    // For now, use default ZGW configuration
    
    // Example routing logic:
    // - Zone 1-4: ZGW#1 (192.168.1.10:13400)
    // - Zone 5-8: ZGW#2 (192.168.1.11:13400)
    // - Zone 9+:  ZGW#3 (192.168.1.12:13400)
    
    if (zone_number <= 4) {
        return {"192.168.1.10", 13400};
    } else if (zone_number <= 8) {
        return {"192.168.1.11", 13400};
    } else {
        return {"192.168.1.12", 13400};
    }
}

