/**
 * @file zone_package.hpp
 * @brief Zone Package Structure (VMG → ZGW)
 * 
 * Zone Package contains multiple ECU firmware packages for a specific zone.
 * This is the middle layer in the 3-layer hierarchy.
 * 
 * @version 1.0
 * @date 2024-11-17
 */

#ifndef ZONE_PACKAGE_HPP
#define ZONE_PACKAGE_HPP

#include <string>
#include <vector>
#include <cstdint>

// ==================== Constants ====================

#define ZONE_PACKAGE_MAGIC  0x5A4F4E45  // "ZONE"
#define MAX_ECUS_IN_ZONE    16

// ==================== Zone Package Metadata ====================

/**
 * @brief ECU Entry in Zone Package Table
 */
struct ZoneECUEntry {
    char     ecu_id[16];            // "ECU_091"
    uint32_t offset;                // Offset in Zone Package
    uint32_t size;                  // Total ECU Package size (metadata + firmware)
    uint32_t metadata_size;         // ECU Metadata size (256 bytes)
    uint32_t firmware_size;         // Firmware binary size
    uint32_t firmware_version;      // 0x00010203 (v1.2.3)
    uint32_t crc32;                 // ECU Package CRC32
    uint8_t  priority;              // Update priority (0 = highest)
    uint8_t  reserved[23];
} __attribute__((packed));  // 64 bytes

/**
 * @brief Zone Package Header
 * 
 * Size: 1KB (0x400 bytes)
 * Location: Offset 0x0000 in Zone Package file
 */
struct ZonePackageHeader {
    // Basic Info (256 bytes)
    uint32_t magic_number;          // 0x5A4F4E45 ("ZONE")
    uint32_t version;               // Zone Package format version (0x00010000)
    uint32_t total_size;            // Total Zone Package size
    
    char     zone_id[16];           // "Zone_Front_Left"
    uint8_t  zone_number;           // Zone number (1~16)
    uint8_t  package_count;         // Number of ECUs (1~16)
    uint8_t  reserved1[2];
    
    uint32_t zone_crc32;            // CRC32 of Zone Package (excluding this header)
    uint32_t timestamp;             // Package creation timestamp
    
    char     zone_name[32];         // Human-readable name
    uint8_t  reserved2[188];
    
    // ECU Table (768 bytes = 64 bytes × 12 entries, padded to 16)
    ZoneECUEntry ecu_table[MAX_ECUS_IN_ZONE];
    
} __attribute__((packed));  // Total: 1024 bytes (1KB)

// ==================== ECU Package Metadata ====================

/**
 * @brief ECU Dependency Entry
 */
struct ECUDependency {
    char     ecu_id[16];            // Required ECU ID
    uint32_t min_version;           // Minimum required version
    uint8_t  reserved[12];
} __attribute__((packed));  // 32 bytes

/**
 * @brief ECU Package Metadata
 * 
 * Size: 256 bytes
 * Location: Start of each ECU Package within Zone Package
 */
struct ECUMetadata {
    // Basic Info (112 bytes)
    uint32_t magic_number;          // 0x4543554D ("ECUM")
    
    char     ecu_id[16];            // "ECU_091"
    uint32_t sw_version;            // 0x00010203 (v1.2.3)
    uint32_t hw_version;            // 0x00010000 (HW v1.0.0)
    uint32_t firmware_size;         // Firmware binary size
    uint32_t firmware_crc32;        // Firmware CRC32
    uint32_t build_timestamp;       // Build time
    
    char     version_string[32];    // "v1.2.3-20241117"
    
    uint8_t  dependency_count;      // Number of dependencies (0~8)
    uint8_t  reserved1[3];
    
    // Dependency Info (256 bytes = 32 bytes × 8)
    ECUDependency dependencies[8];
    
    // Reserved (144 bytes)
    uint8_t  reserved2[144];
    
} __attribute__((packed));  // Total: 256 bytes

// ==================== Zone Package Parser ====================

/**
 * @brief Zone Package Parser Class
 * 
 * Parse and validate Zone Package before sending to ZGW
 */
class ZonePackageParser {
public:
    /**
     * @brief Constructor
     * @param package_path Path to Zone Package file
     */
    explicit ZonePackageParser(const std::string& package_path);
    
    /**
     * @brief Parse Zone Package header
     * @return true if successful
     */
    bool parse();
    
    /**
     * @brief Verify Zone Package integrity
     * @return true if valid
     */
    bool verify();
    
    /**
     * @brief Get parsed header
     */
    const ZonePackageHeader& getHeader() const { return header_; }
    
    /**
     * @brief Get ECU count
     */
    uint8_t getECUCount() const { return header_.package_count; }
    
    /**
     * @brief Get total package size
     */
    uint32_t getTotalSize() const { return header_.total_size; }
    
    /**
     * @brief Print Zone Package summary
     */
    void printSummary() const;
    
    /**
     * @brief Get ECU list
     */
    std::vector<std::string> getECUList() const;

private:
    std::string package_path_;
    ZonePackageHeader header_;
    bool parsed_;
    
    /**
     * @brief Calculate CRC32
     */
    uint32_t calculateCRC32(const uint8_t* data, size_t size) const;
};

#endif // ZONE_PACKAGE_HPP

