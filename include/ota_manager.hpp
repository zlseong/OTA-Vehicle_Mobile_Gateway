/**
 * @file ota_manager.hpp
 * @brief OTA Manager - OTA Package Download, Verification, and Installation
 * 
 * Design Philosophy (from ZGW):
 *   - Simple, modular functions
 *   - Clear separation of concerns
 *   - Error handling at every step
 *   - Progress reporting
 * 
 * @version 1.0
 * @date 2024-11-15
 */

#ifndef OTA_MANAGER_HPP
#define OTA_MANAGER_HPP

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include "partition_manager.hpp"
#include "http_client.hpp"
#include "mqtt_client.hpp"
#include "config_manager.hpp"
#include "vehicle_package.hpp"
#include "zone_package.hpp"
#include "doip_client.hpp"

// ==================== Constants ====================

#define OTA_DOWNLOAD_CHUNK_SIZE     (64 * 1024)     // 64KB chunks (configurable)
#define OTA_MAX_RETRY_ATTEMPTS      3               // Maximum download retry
#define OTA_PROGRESS_REPORT_INTERVAL 5              // Report every 5% progress

// ==================== Type Definitions ====================

/**
 * @brief OTA Update State (parallel to ZGW update flow)
 */
enum class OTAState : uint8_t {
    OTA_IDLE = 0,              /* No OTA in progress */
    OTA_DOWNLOADING = 1,       /* Downloading package */
    OTA_VERIFYING = 2,         /* Verifying package integrity */
    OTA_INSTALLING = 3,        /* Installing to standby partition */
    OTA_READY = 4,             /* Ready to reboot */
    OTA_ERROR = 5,             /* Error occurred */
    OTA_COMPLETED = 6          /* OTA completed successfully */
};

/**
 * @brief OTA Progress Information
 */
struct OTAProgress {
    OTAState state;                 /* Current OTA state */
    uint32_t total_bytes;           /* Total package size */
    uint32_t downloaded_bytes;      /* Downloaded bytes */
    uint8_t  percentage;            /* Progress percentage (0-100) */
    std::string current_step;       /* Current step description */
    std::string error_message;      /* Error message (if any) */
};

/**
 * @brief OTA Package Metadata (from server)
 */
struct OTAPackageInfo {
    std::string campaign_id;        /* Campaign ID */
    std::string package_url;        /* Download URL */
    uint32_t package_size;          /* Package size in bytes */
    uint32_t firmware_version;      /* Firmware version (0xAABBCCDD) */
    std::string sha256_hash;        /* Expected SHA256 hash (hex string) */
    std::string target_partition;   /* Target partition (A or B) */
};

// ==================== Class Definition ====================

/**
 * @brief OTA Manager Class
 * 
 * Manages the entire OTA update process
 * Design based on ZGW's modular approach
 */
class OTAManager {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager
     * @param http_client HTTP client for downloads
     * @param mqtt_client MQTT client for progress reporting
     * @param partition_mgr Partition manager (for VMG self-update)
     * @param doip_clients DoIP clients for each ZGW
     */
    OTAManager(
        ConfigManager& config,
        HttpClient* http_client,
        MqttClient* mqtt_client,
        std::shared_ptr<PartitionManager> partition_mgr,
        std::vector<std::shared_ptr<DoIPClient>> doip_clients = {}
    );
    
    /**
     * @brief Initialize OTA manager
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Start OTA update process (Legacy: single package)
     * @param package_info OTA package information
     * @return true if OTA started successfully
     */
    bool startOTA(const OTAPackageInfo& package_info);
    
    /**
     * @brief Start Vehicle-level OTA update (3-layer package)
     * @param package_info OTA package information (contains Vehicle Package URL)
     * @return true if OTA started successfully
     * 
     * Flow:
     *   1. Download Vehicle Package from Server (HTTPS)
     *   2. Parse Vehicle Package metadata
     *   3. Verify VIN, Model, Year
     *   4. Extract Zone Packages
     *   5. Send each Zone Package to target ZGW (DoIP/UDS)
     */
    bool startVehicleOTA(const OTAPackageInfo& package_info);
    
    /**
     * @brief Get current OTA state
     */
    OTAState getState() const { return current_state_; }
    
    /**
     * @brief Get current progress
     */
    OTAProgress getProgress() const { return progress_; }
    
    /**
     * @brief Check if OTA is in progress
     */
    bool isOTAInProgress() const {
        return current_state_ != OTAState::OTA_IDLE &&
               current_state_ != OTAState::OTA_COMPLETED &&
               current_state_ != OTAState::OTA_ERROR;
    }
    
    /**
     * @brief Cancel ongoing OTA
     * @return true if cancelled successfully
     */
    bool cancelOTA();
    
    /**
     * @brief Set progress callback (for real-time updates)
     * @param callback Progress callback function
     */
    void setProgressCallback(std::function<void(const OTAProgress&)> callback) {
        progress_callback_ = callback;
    }

private:
    // Dependencies
    ConfigManager& config_;
    HttpClient* http_client_;
    MqttClient* mqtt_client_;
    std::shared_ptr<PartitionManager> partition_mgr_;
    std::vector<std::shared_ptr<DoIPClient>> doip_clients_;
    
    // State
    OTAState current_state_;
    OTAProgress progress_;
    OTAPackageInfo package_info_;
    std::function<void(const OTAProgress&)> progress_callback_;
    
    // Configuration
    std::string download_path_;
    std::string install_path_;
    uint32_t chunk_size_;
    uint32_t max_retries_;
    
    // Vehicle Package processing
    std::unique_ptr<VehiclePackageParser> vehicle_parser_;
    std::vector<ZonePackageInfo> zone_packages_;
    
    /**
     * @brief Download OTA package (with chunked download)
     * @return true if successful
     */
    bool downloadPackage();
    
    /**
     * @brief Download a single chunk with retry
     * @param url Download URL
     * @param start Start byte
     * @param end End byte
     * @param output_file Output file stream
     * @return true if successful
     */
    bool downloadChunk(const std::string& url, size_t start, size_t end, std::ofstream& output_file);
    
    /**
     * @brief Verify downloaded package integrity
     * @return true if valid
     */
    bool verifyPackage();
    
    /**
     * @brief Install package to standby partition
     * @return true if successful
     */
    bool installPackage();
    
    /**
     * @brief Calculate SHA256 hash of file
     * @param file_path File path
     * @param hash Output hash (32 bytes)
     * @return true if successful
     */
    bool calculateSHA256(const std::string& file_path, uint8_t* hash);
    
    /**
     * @brief Convert hex string to binary
     * @param hex_string Hex string (64 chars for SHA256)
     * @param binary Output binary (32 bytes)
     * @return true if successful
     */
    bool hexToBinary(const std::string& hex_string, uint8_t* binary);
    
    /**
     * @brief Update OTA state and report progress
     * @param state New state
     * @param step_description Step description
     */
    void updateState(OTAState state, const std::string& step_description);
    
    /**
     * @brief Update progress and report via MQTT
     * @param downloaded Downloaded bytes
     * @param total Total bytes
     */
    void updateProgress(uint32_t downloaded, uint32_t total);
    
    /**
     * @brief Report error
     * @param error_message Error message
     */
    void reportError(const std::string& error_message);
    
    /**
     * @brief Send progress report to server via MQTT
     */
    void sendProgressReport();
    
    // ==================== Vehicle Package Processing ====================
    
    /**
     * @brief Download and parse Vehicle Package
     * @return true if successful
     */
    bool downloadVehiclePackage();
    
    /**
     * @brief Verify Vehicle Package target (VIN, Model, Year)
     * @return true if match
     */
    bool verifyVehiclePackageTarget();
    
    /**
     * @brief Extract Zone Packages from Vehicle Package
     * @return true if successful
     */
    bool extractZonePackages();
    
    /**
     * @brief Send a Zone Package to target ZGW via DoIP/UDS
     * @param zone_info Zone Package information
     * @return true if successful
     */
    bool sendZonePackageToZGW(const ZonePackageInfo& zone_info);
    
    /**
     * @brief Send Zone Package using UDS 0x34/0x36/0x37
     * @param doip_client DoIP client connected to ZGW
     * @param zone_package_path Path to Zone Package file
     * @return true if successful
     */
    bool transferZonePackageViaUDS(DoIPClient* doip_client, 
                                     const std::string& zone_package_path);
    
    /**
     * @brief Get or create DoIP client for target ZGW
     * @param zgw_ip ZGW IP address
     * @param zgw_port ZGW DoIP port
     * @return DoIP client pointer
     */
    DoIPClient* getDoIPClientForZGW(const std::string& zgw_ip, uint16_t zgw_port);
};

#endif // OTA_MANAGER_HPP


