/**
 * @file zone_package_parser.cpp
 * @brief Zone Package Parser Implementation
 * 
 * Parse and validate Zone Package before sending to ZGW
 * 
 * @version 1.0
 * @date 2024-11-17
 */

#include "zone_package.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <zlib.h>

ZonePackageParser::ZonePackageParser(const std::string& package_path)
    : package_path_(package_path), parsed_(false) {
    std::memset(&header_, 0, sizeof(ZonePackageHeader));
}

bool ZonePackageParser::parse() {
    std::cout << "[ZonePackage] Parsing Zone Package: " << package_path_ << "\n";
    
    // Open package file
    std::ifstream file(package_path_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ZonePackage] ✗ Failed to open package file\n";
        return false;
    }
    
    // Read header (first 1KB)
    file.read(reinterpret_cast<char*>(&header_), sizeof(ZonePackageHeader));
    if (!file.good()) {
        std::cerr << "[ZonePackage] ✗ Failed to read header\n";
        return false;
    }
    
    // Verify magic number
    if (header_.magic_number != ZONE_PACKAGE_MAGIC) {
        std::cerr << "[ZonePackage] ✗ Invalid magic number: 0x" 
                  << std::hex << header_.magic_number << std::dec << "\n";
        return false;
    }
    
    std::cout << "[ZonePackage] ✓ Magic number valid: 0x5A4F4E45 (\"ZONE\")\n";
    std::cout << "[ZonePackage]   Zone: " << header_.zone_name << " (Zone #" 
              << (int)header_.zone_number << ")\n";
    std::cout << "[ZonePackage]   ECU Count: " << (int)header_.package_count << "\n";
    std::cout << "[ZonePackage]   Total Size: " << header_.total_size << " bytes\n";
    
    // Print ECU list
    for (uint8_t i = 0; i < header_.package_count; i++) {
        const auto& ecu = header_.ecu_table[i];
        std::string ecu_id = std::string(ecu.ecu_id, 16);
        ecu_id.erase(ecu_id.find('\0'));
        
        std::cout << "[ZonePackage]     [" << (i+1) << "] " << ecu_id 
                  << " (v" << ((ecu.firmware_version >> 16) & 0xFF) << "."
                  << ((ecu.firmware_version >> 8) & 0xFF) << "."
                  << (ecu.firmware_version & 0xFF)
                  << ", " << ecu.firmware_size << " bytes, priority=" << (int)ecu.priority << ")\n";
    }
    
    file.close();
    parsed_ = true;
    
    std::cout << "[ZonePackage] ✓ Zone Package parsed successfully\n";
    return true;
}

bool ZonePackageParser::verify() {
    if (!parsed_) {
        std::cerr << "[ZonePackage] ✗ Package not parsed yet\n";
        return false;
    }
    
    std::cout << "[ZonePackage] Verifying package integrity...\n";
    
    // Open file
    std::ifstream file(package_path_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ZonePackage] ✗ Failed to open file for verification\n";
        return false;
    }
    
    // Calculate CRC32 of package data (excluding header)
    file.seekg(sizeof(ZonePackageHeader), std::ios::beg);
    
    size_t data_size = header_.total_size - sizeof(ZonePackageHeader);
    std::vector<uint8_t> package_data(data_size);
    file.read(reinterpret_cast<char*>(package_data.data()), data_size);
    
    uint32_t calculated_crc = calculateCRC32(package_data.data(), data_size);
    
    if (calculated_crc != header_.zone_crc32) {
        std::cerr << "[ZonePackage] ✗ CRC32 mismatch\n";
        std::cerr << "  Expected: 0x" << std::hex << header_.zone_crc32 << "\n";
        std::cerr << "  Calculated: 0x" << calculated_crc << std::dec << "\n";
        return false;
    }
    
    std::cout << "[ZonePackage] ✓ CRC32 valid: 0x" << std::hex << calculated_crc << std::dec << "\n";
    
    file.close();
    return true;
}

void ZonePackageParser::printSummary() const {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Zone Package Summary\n";
    std::cout << "========================================\n";
    std::cout << "Zone ID:       " << header_.zone_id << "\n";
    std::cout << "Zone Number:   " << (int)header_.zone_number << "\n";
    std::cout << "Zone Name:     " << header_.zone_name << "\n";
    std::cout << "Total Size:    " << header_.total_size << " bytes\n";
    std::cout << "ECU Count:     " << (int)header_.package_count << "\n";
    std::cout << "Timestamp:     " << header_.timestamp << "\n";
    std::cout << "\nECU Packages:\n";
    
    for (uint8_t i = 0; i < header_.package_count; i++) {
        const auto& ecu = header_.ecu_table[i];
        std::string ecu_id = std::string(ecu.ecu_id, 16);
        ecu_id.erase(ecu_id.find('\0'));
        
        std::cout << "  [" << (i+1) << "] " << ecu_id << "\n";
        std::cout << "      Version: v" 
                  << ((ecu.firmware_version >> 16) & 0xFF) << "."
                  << ((ecu.firmware_version >> 8) & 0xFF) << "."
                  << (ecu.firmware_version & 0xFF) << "\n";
        std::cout << "      Size: " << ecu.size << " bytes (FW: " 
                  << ecu.firmware_size << " bytes)\n";
        std::cout << "      Priority: " << (int)ecu.priority << "\n";
        std::cout << "      CRC32: 0x" << std::hex << ecu.crc32 << std::dec << "\n";
    }
    
    std::cout << "========================================\n\n";
}

std::vector<std::string> ZonePackageParser::getECUList() const {
    std::vector<std::string> ecu_list;
    
    for (uint8_t i = 0; i < header_.package_count; i++) {
        std::string ecu_id = std::string(header_.ecu_table[i].ecu_id, 16);
        ecu_id.erase(ecu_id.find('\0'));
        ecu_list.push_back(ecu_id);
    }
    
    return ecu_list;
}

uint32_t ZonePackageParser::calculateCRC32(const uint8_t* data, size_t size) const {
    return crc32(0L, data, size);
}

