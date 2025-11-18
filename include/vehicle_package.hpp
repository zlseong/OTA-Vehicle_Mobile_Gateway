/**
 * @file vehicle_package.hpp
 * @brief Vehicle Package Structure (Server → VMG)
 * 
 * 3-Layer Package Hierarchy:
 *   Vehicle Package (Top Level)
 *     └─ Zone Packages (Middle Level)
 *          └─ ECU Packages (Bottom Level)
 * 
 * @version 1.0
 * @date 2024-11-17
 */

#ifndef VEHICLE_PACKAGE_HPP
#define VEHICLE_PACKAGE_HPP

#include <string>
#include <vector>
#include <cstdint>

// ==================== Constants ====================

#define VEHICLE_PACKAGE_MAGIC   0x5650504B  // "VPPK" (Vehicle Package)
#define MAX_ZONES_IN_VEHICLE    16
#define MAX_ECUS_IN_VEHICLE     256

// ==================== Vehicle Package Metadata ====================

/**
 * @brief Zone Reference Entry in Vehicle Package
 */
struct ZoneReference {
    char     zone_id[16];           // "Zone_Front_Left"
    uint32_t offset;                // Offset in Vehicle Package file
    uint32_t size;                  // Zone Package total size
    uint8_t  zone_number;           // Zone number (1~16)
    uint8_t  ecu_count;             // Number of ECUs in this zone
    uint8_t  reserved[6];
} __attribute__((packed));  // 32 bytes

/**
 * @brief ECU Quick Reference Entry
 */
struct ECUReference {
    char     ecu_id[16];            // "ECU_091"
    uint8_t  zone_number;           // Which zone this ECU belongs to
    uint32_t firmware_version;      // 0x00010203 (v1.2.3)
    uint8_t  reserved[11];
} __attribute__((packed));  // 32 bytes

/**
 * @brief Vehicle Package Metadata (Top-Level Header)
 * 
 * Size: 12KB (0x3000 bytes)
 * Location: Offset 0x0000 in Vehicle Package file
 */
struct VehiclePackageMetadata {
    // Basic Info (64 bytes)
    uint32_t magic_number;          // 0x5650504B ("VPPK")
    uint32_t version;               // Package format version (0x00010000 = v1.0)
    uint32_t total_size;            // Total Vehicle Package size (bytes)
    
    // Vehicle Target Info (64 bytes)
    char     vin[17];               // Vehicle VIN (16 chars + null)
    char     model[32];             // Vehicle model (e.g., "Genesis GV80")
    uint16_t model_year;            // Model year (e.g., 2024)
    uint8_t  region;                // Region code
    uint8_t  reserved1[12];
    
    // Master SW Version (48 bytes)
    uint32_t master_sw_version;     // 0xAABBCCDD (vAA.BB.CC.DD)
    char     master_sw_string[32];  // "v2.0.0"
    uint8_t  reserved2[12];
    
    // Package Counts (16 bytes)
    uint8_t  zone_count;            // Number of Zone Packages (1~16)
    uint8_t  total_ecu_count;       // Total ECUs across all zones (1~256)
    uint8_t  reserved3[14];
    
    // CRC (16 bytes)
    uint32_t vehicle_crc32;         // CRC32 of entire Vehicle Package (excluding this field)
    uint32_t metadata_crc32;        // CRC32 of this metadata structure
    uint8_t  reserved4[8];
    
    // Zone References (512 bytes = 32 bytes × 16)
    ZoneReference zone_refs[MAX_ZONES_IN_VEHICLE];
    
    // ECU Quick Reference Table (8192 bytes = 32 bytes × 256)
    ECUReference ecu_refs[MAX_ECUS_IN_VEHICLE];
    
    // Reserved (3072 bytes)
    uint8_t  reserved5[3072];
    
} __attribute__((packed));  // Total: 12KB (0x3000 bytes)

// ==================== Zone Package Info ====================

/**
 * @brief Zone Package Information (Extracted from Vehicle Package)
 */
struct ZonePackageInfo {
    std::string zone_id;            // "Zone_Front_Left"
    uint8_t zone_number;            // Zone number (1~16)
    uint32_t offset;                // Offset in Vehicle Package file
    uint32_t size;                  // Zone Package size
    uint8_t ecu_count;              // Number of ECUs
    std::string target_zgw_ip;      // Target ZGW IP address
    uint16_t target_zgw_port;       // Target ZGW DoIP port
    std::string extracted_path;     // Path to extracted Zone Package file
};

// ==================== Vehicle Package Parser ====================

/**
 * @brief Vehicle Package Parser Class
 * 
 * Responsibilities:
 *   1. Parse Vehicle Package received from Server
 *   2. Verify integrity (CRC32)
 *   3. Extract Zone Packages
 *   4. Determine routing for each Zone Package
 */
class VehiclePackageParser {
public:
    /**
     * @brief Constructor
     * @param package_path Path to Vehicle Package file
     */
    explicit VehiclePackageParser(const std::string& package_path);
    
    /**
     * @brief Parse Vehicle Package header
     * @return true if successful
     */
    bool parse();
    
    /**
     * @brief Verify Vehicle Package integrity
     * @return true if valid
     */
    bool verify();
    
    /**
     * @brief Check if this package is for the correct vehicle
     * @param vin Expected VIN
     * @param model Expected model
     * @param model_year Expected model year
     * @return true if match
     */
    bool verifyVehicleTarget(const std::string& vin, 
                             const std::string& model,
                             uint16_t model_year);
    
    /**
     * @brief Get parsed metadata
     */
    const VehiclePackageMetadata& getMetadata() const { return metadata_; }
    
    /**
     * @brief Get list of Zone Packages
     */
    const std::vector<ZonePackageInfo>& getZonePackages() const { 
        return zone_packages_; 
    }
    
    /**
     * @brief Extract a specific Zone Package to file
     * @param zone_number Zone number (1~16)
     * @param output_path Output file path
     * @return true if successful
     */
    bool extractZonePackage(uint8_t zone_number, const std::string& output_path);
    
    /**
     * @brief Extract all Zone Packages to directory
     * @param output_dir Output directory
     * @return true if successful
     */
    bool extractAllZonePackages(const std::string& output_dir);
    
    /**
     * @brief Get Zone Package routing information
     * @param zone_number Zone number
     * @return Zone Package info with routing details
     */
    ZonePackageInfo getZoneRoutingInfo(uint8_t zone_number) const;
    
    /**
     * @brief Print package summary
     */
    void printSummary() const;

private:
    std::string package_path_;
    VehiclePackageMetadata metadata_;
    std::vector<ZonePackageInfo> zone_packages_;
    bool parsed_;
    
    /**
     * @brief Calculate CRC32 of data
     */
    uint32_t calculateCRC32(const uint8_t* data, size_t size) const;
    
    /**
     * @brief Determine target ZGW for a zone
     * @param zone_number Zone number
     * @return ZGW IP address and port
     */
    std::pair<std::string, uint16_t> determineZoneTarget(uint8_t zone_number) const;
};

#endif // VEHICLE_PACKAGE_HPP

