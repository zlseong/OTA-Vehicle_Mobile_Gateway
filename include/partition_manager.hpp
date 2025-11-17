/**
 * @file partition_manager.hpp
 * @brief Partition Manager - Dual Partition Management for VMG
 * 
 * Design based on ZGW FlashBankManager (TC375)
 * 
 * Linux Partition Layout:
 *   - /dev/mmcblk0p1 (Partition A): Active partition
 *   - /dev/mmcblk0p2 (Partition B): Standby partition
 *   - /boot/boot_status.dat: Boot flag storage
 * 
 * @version 1.0
 * @date 2024-11-15
 */

#ifndef PARTITION_MANAGER_HPP
#define PARTITION_MANAGER_HPP

#include <string>
#include <cstdint>

// ==================== Constants ====================

// Partition paths (configurable via config.json)
// 3-Sector Layout: Boot + Partition A (rootfs) + Partition B (rootfs) + Data (persistent)
#define DEFAULT_PARTITION_A_PATH    "/dev/mmcblk0p2"    // rootfs A (OS + VMG app)
#define DEFAULT_PARTITION_B_PATH    "/dev/mmcblk0p3"    // rootfs B (OS + VMG app)
#define DEFAULT_DATA_PARTITION_PATH "/dev/mmcblk0p4"    // 공용 데이터 파티션
#define DEFAULT_DATA_MOUNT_POINT    "/mnt/data"         // 데이터 파티션 마운트 위치
#define DEFAULT_BOOT_STATUS_PATH    "/mnt/data/boot_status.dat"

// Simulation mode (for testing without real partitions)
#define DEFAULT_SIM_PARTITION_A     "/tmp/vmg_partitions/partition_a"
#define DEFAULT_SIM_PARTITION_B     "/tmp/vmg_partitions/partition_b"
#define DEFAULT_SIM_DATA_PARTITION  "/tmp/vmg_partitions/data"
#define DEFAULT_SIM_BOOT_STATUS     "/tmp/vmg_partitions/data/boot_status.dat"

// Magic number for validation (parallel to ZGW: 0x42414E4B "BANK")
#define PARTITION_MAGIC_NUMBER      0x564D4750  /* "VMGP" */

// ==================== Type Definitions ====================

/**
 * @brief Partition Identifier
 */
enum class PartitionId : uint8_t {
    PARTITION_A = 0,
    PARTITION_B = 1,
    PARTITION_UNKNOWN = 0xFF
};

/**
 * @brief Partition State (parallel to ZGW BankStatus_t)
 */
enum class PartitionState : uint8_t {
    STATE_UNKNOWN = 0x00,      /* Initial state */
    STATE_EMPTY = 0x01,        /* No valid firmware */
    STATE_READY = 0x02,        /* Valid, ready to boot */
    STATE_ACTIVE = 0x03,       /* Currently running */
    STATE_UPDATING = 0x04,     /* Update in progress */
    STATE_ERROR = 0x05,        /* CRC/integrity error */
    STATE_ROLLBACK = 0x06      /* Rolled back due to failure */
};

/**
 * @brief Partition Metadata (stored at the beginning of each partition)
 * 
 * Parallel to ZGW BankMetadata_t
 * Location:
 *   - Partition A: First 1KB reserved
 *   - Partition B: First 1KB reserved
 */
struct PartitionMetadata {
    uint32_t magic_number;           /* 0x564D4750 ("VMGP") */
    uint32_t firmware_version;       /* 0xAABBCCDD (vAA.BB.CC.DD) */
    uint32_t build_timestamp;        /* Unix timestamp */
    uint32_t total_size;             /* Firmware size in bytes */
    uint8_t  sha256_hash[32];        /* SHA256 hash (instead of CRC32) */
    PartitionState state;            /* Current partition state */
    uint8_t  reserved[959];          /* Padding to 1KB */
} __attribute__((packed));

/**
 * @brief Boot Status (parallel to ZGW FlashBankStatus_t)
 * 
 * Stored in /boot/boot_status.dat (or simulation path)
 */
struct BootStatus {
    uint32_t magic_number;           /* 0x564D4750 ("VMGP") */
    PartitionId boot_target;         /* Target partition for next boot */
    PartitionState state_a;          /* Partition A state */
    PartitionState state_b;          /* Partition B state */
    uint32_t boot_count;             /* Boot attempt counter (for rollback) */
    uint32_t last_boot_timestamp;    /* Last successful boot time */
    uint8_t  reserved[236];          /* Padding to 256 bytes */
} __attribute__((packed));

// ==================== Class Definition ====================

/**
 * @brief Partition Manager Class
 * 
 * Manages dual partition for OTA updates
 * Design based on ZGW FlashBankManager
 */
class PartitionManager {
public:
    /**
     * @brief Constructor
     * @param partition_a_path Path to partition A (rootfs)
     * @param partition_b_path Path to partition B (rootfs)
     * @param data_partition_path Path to data partition (persistent storage)
     * @param data_mount_point Mount point for data partition
     * @param boot_status_path Path to boot status file
     * @param simulation_mode Enable simulation mode for testing
     */
    PartitionManager(
        const std::string& partition_a_path = DEFAULT_PARTITION_A_PATH,
        const std::string& partition_b_path = DEFAULT_PARTITION_B_PATH,
        const std::string& data_partition_path = DEFAULT_DATA_PARTITION_PATH,
        const std::string& data_mount_point = DEFAULT_DATA_MOUNT_POINT,
        const std::string& boot_status_path = DEFAULT_BOOT_STATUS_PATH,
        bool simulation_mode = false
    );
    
    /**
     * @brief Initialize partition manager
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Get active partition ID
     */
    PartitionId getActivePartition() const { return active_partition_; }
    
    /**
     * @brief Get standby partition ID
     */
    PartitionId getStandbyPartition() const;
    
    /**
     * @brief Get partition state
     * @param partition Partition ID
     * @return Partition state
     */
    PartitionState getPartitionState(PartitionId partition) const;
    
    /**
     * @brief Set partition state
     * @param partition Partition ID
     * @param state New state
     * @return true if successful
     */
    bool setPartitionState(PartitionId partition, PartitionState state);
    
    /**
     * @brief Read partition metadata
     * @param partition Partition ID
     * @param metadata Output metadata
     * @return true if successful
     */
    bool readMetadata(PartitionId partition, PartitionMetadata& metadata);
    
    /**
     * @brief Write partition metadata
     * @param partition Partition ID
     * @param metadata Metadata to write
     * @return true if successful
     */
    bool writeMetadata(PartitionId partition, const PartitionMetadata& metadata);
    
    /**
     * @brief Verify partition integrity
     * @param partition Partition ID
     * @return true if partition is valid
     */
    bool verifyPartition(PartitionId partition);
    
    /**
     * @brief Switch boot target (parallel to ZGW FlashBank_SwitchBank)
     * @param target Target partition for next boot
     * @return true if successful
     */
    bool switchBootTarget(PartitionId target);
    
    /**
     * @brief Increment boot count (for rollback detection)
     * @return Current boot count
     */
    uint32_t incrementBootCount();
    
    /**
     * @brief Reset boot count (after successful boot)
     * @return true if successful
     */
    bool resetBootCount();
    
    /**
     * @brief Check if rollback is needed (boot count >= 3)
     * @return true if rollback needed
     */
    bool isRollbackNeeded();
    
    /**
     * @brief Perform rollback to previous partition
     * @return true if successful
     */
    bool performRollback();
    
    /**
     * @brief Get partition path
     * @param partition Partition ID
     * @return Partition path
     */
    std::string getPartitionPath(PartitionId partition) const;
    
    /**
     * @brief Get data partition mount point
     * @return Data mount point
     */
    std::string getDataMountPoint() const { return data_mount_point_; }
    
    /**
     * @brief Mount data partition
     * @return true if successful
     */
    bool mountDataPartition();
    
    /**
     * @brief Check if data partition is mounted
     * @return true if mounted
     */
    bool isDataPartitionMounted() const;

private:
    // Configuration
    std::string partition_a_path_;
    std::string partition_b_path_;
    std::string data_partition_path_;
    std::string data_mount_point_;
    std::string boot_status_path_;
    bool simulation_mode_;
    bool data_mounted_;
    
    // Runtime state
    PartitionId active_partition_;
    BootStatus boot_status_;
    
    /**
     * @brief Read boot status from file
     * @return true if successful
     */
    bool readBootStatus();
    
    /**
     * @brief Write boot status to file
     * @return true if successful
     */
    bool writeBootStatus();
    
    /**
     * @brief Create simulation directories (if simulation_mode)
     * @return true if successful
     */
    bool createSimulationEnvironment();
};

#endif // PARTITION_MANAGER_HPP


