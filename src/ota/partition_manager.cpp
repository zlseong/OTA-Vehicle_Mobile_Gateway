/**
 * @file partition_manager.cpp
 * @brief Partition Manager Implementation (3-Sector Layout)
 * 
 * 3-Sector Layout:
 *   - /dev/mmcblk0p1: Boot (U-Boot, Kernel)
 *   - /dev/mmcblk0p2: Partition A (rootfs, read-only)
 *   - /dev/mmcblk0p3: Partition B (rootfs, read-only)
 *   - /dev/mmcblk0p4: Data (persistent storage, read-write)
 */

#include "partition_manager.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <openssl/sha.h>

PartitionManager::PartitionManager(
    const std::string& partition_a_path,
    const std::string& partition_b_path,
    const std::string& data_partition_path,
    const std::string& data_mount_point,
    const std::string& boot_status_path,
    bool simulation_mode
) : partition_a_path_(partition_a_path),
    partition_b_path_(partition_b_path),
    data_partition_path_(data_partition_path),
    data_mount_point_(data_mount_point),
    boot_status_path_(boot_status_path),
    simulation_mode_(simulation_mode),
    active_partition_(PartitionId::PARTITION_UNKNOWN),
    data_mounted_(false)
{
    std::memset(&boot_status_, 0, sizeof(BootStatus));
}

// ==================== Initialization ====================

bool PartitionManager::initialize() {
    std::cout << "[PARTITION] Initializing Partition Manager (3-Sector Layout)...\n";
    std::cout << "[PARTITION]   Partition A: " << partition_a_path_ << " (rootfs)\n";
    std::cout << "[PARTITION]   Partition B: " << partition_b_path_ << " (rootfs)\n";
    std::cout << "[PARTITION]   Data: " << data_partition_path_ << " → " << data_mount_point_ << "\n";
    
    // Create simulation environment if needed
    if (simulation_mode_) {
        if (!createSimulationEnvironment()) {
            std::cerr << "[PARTITION] Failed to create simulation environment\n";
            return false;
        }
    }
    
    // Mount data partition
    std::cout << "[PARTITION] Mounting data partition...\n";
    if (!mountDataPartition()) {
        std::cerr << "[PARTITION] Failed to mount data partition\n";
        return false;
    }
    std::cout << "[PARTITION] ✓ Data partition mounted at " << data_mount_point_ << "\n";
    
    // Read boot status
    if (!readBootStatus()) {
        std::cout << "[PARTITION] No valid boot status found, initializing...\n";
        
        // Initialize boot status
        boot_status_.magic_number = PARTITION_MAGIC_NUMBER;
        boot_status_.boot_target = PartitionId::PARTITION_A;
        boot_status_.state_a = PartitionState::STATE_ACTIVE;
        boot_status_.state_b = PartitionState::STATE_EMPTY;
        boot_status_.boot_count = 0;
        boot_status_.last_boot_timestamp = time(nullptr);
        
        if (!writeBootStatus()) {
            std::cerr << "[PARTITION] Failed to write initial boot status\n";
            return false;
        }
    }
    
    // Determine active partition
    active_partition_ = boot_status_.boot_target;
    
    std::cout << "[PARTITION] ✓ Active Partition: " 
              << (active_partition_ == PartitionId::PARTITION_A ? "A" : "B") << "\n";
    std::cout << "[PARTITION] ✓ Partition A State: " 
              << static_cast<int>(boot_status_.state_a) << "\n";
    std::cout << "[PARTITION] ✓ Partition B State: " 
              << static_cast<int>(boot_status_.state_b) << "\n";
    
    return true;
}

// ==================== Partition State Management ====================

PartitionId PartitionManager::getStandbyPartition() const {
    return (active_partition_ == PartitionId::PARTITION_A) 
           ? PartitionId::PARTITION_B 
           : PartitionId::PARTITION_A;
}

PartitionState PartitionManager::getPartitionState(PartitionId partition) const {
    if (partition == PartitionId::PARTITION_A) {
        return boot_status_.state_a;
    } else if (partition == PartitionId::PARTITION_B) {
        return boot_status_.state_b;
    }
    return PartitionState::STATE_UNKNOWN;
}

bool PartitionManager::setPartitionState(PartitionId partition, PartitionState state) {
    if (partition == PartitionId::PARTITION_A) {
        boot_status_.state_a = state;
    } else if (partition == PartitionId::PARTITION_B) {
        boot_status_.state_b = state;
    } else {
        return false;
    }
    
    return writeBootStatus();
}

// ==================== Metadata Management ====================

bool PartitionManager::readMetadata(PartitionId partition, PartitionMetadata& metadata) {
    std::string path = getPartitionPath(partition);
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[PARTITION] Failed to open partition: " << path << "\n";
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&metadata), sizeof(PartitionMetadata));
    if (!file.good()) {
        std::cerr << "[PARTITION] Failed to read metadata from: " << path << "\n";
        return false;
    }
    
    file.close();
    
    // Verify magic number
    if (metadata.magic_number != PARTITION_MAGIC_NUMBER) {
        std::cout << "[PARTITION] Invalid magic number in metadata\n";
        return false;
    }
    
    return true;
}

bool PartitionManager::writeMetadata(PartitionId partition, const PartitionMetadata& metadata) {
    std::string path = getPartitionPath(partition);
    
    std::ofstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        std::cerr << "[PARTITION] Failed to open partition for writing: " << path << "\n";
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(&metadata), sizeof(PartitionMetadata));
    if (!file.good()) {
        std::cerr << "[PARTITION] Failed to write metadata to: " << path << "\n";
        return false;
    }
    
    file.close();
    
    std::cout << "[PARTITION] ✓ Metadata written to partition " 
              << (partition == PartitionId::PARTITION_A ? "A" : "B") << "\n";
    return true;
}

// ==================== Partition Verification ====================

bool PartitionManager::verifyPartition(PartitionId partition) {
    std::cout << "[PARTITION] Verifying partition " 
              << (partition == PartitionId::PARTITION_A ? "A" : "B") << "...\n";
    
    // Read metadata
    PartitionMetadata metadata;
    if (!readMetadata(partition, metadata)) {
        std::cerr << "[PARTITION] Failed to read metadata for verification\n";
        return false;
    }
    
    // Read partition data (excluding metadata)
    std::string path = getPartitionPath(partition);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[PARTITION] Failed to open partition: " << path << "\n";
        return false;
    }
    
    // Skip metadata
    file.seekg(sizeof(PartitionMetadata));
    
    // Calculate SHA256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    SHA256_Final(hash, &sha256);
    
    file.close();
    
    // Compare hashes
    if (std::memcmp(hash, metadata.sha256_hash, SHA256_DIGEST_LENGTH) != 0) {
        std::cerr << "[PARTITION] ✗ Hash mismatch! Partition corrupted\n";
        return false;
    }
    
    std::cout << "[PARTITION] ✓ Partition verified successfully\n";
    return true;
}

// ==================== Boot Target Management ====================

bool PartitionManager::switchBootTarget(PartitionId target) {
    std::cout << "[PARTITION] Switching boot target to partition " 
              << (target == PartitionId::PARTITION_A ? "A" : "B") << "\n";
    
    boot_status_.boot_target = target;
    boot_status_.boot_count = 0;  // Reset boot count
    
    return writeBootStatus();
}

uint32_t PartitionManager::incrementBootCount() {
    boot_status_.boot_count++;
    writeBootStatus();
    std::cout << "[PARTITION] Boot count: " << boot_status_.boot_count << "\n";
    return boot_status_.boot_count;
}

bool PartitionManager::resetBootCount() {
    boot_status_.boot_count = 0;
    return writeBootStatus();
}

bool PartitionManager::isRollbackNeeded() {
    // Rollback if boot count >= 3
    return boot_status_.boot_count >= 3;
}

bool PartitionManager::performRollback() {
    std::cout << "[PARTITION] ⚠️ Performing rollback...\n";
    
    // Get current and previous partitions
    PartitionId current = boot_status_.boot_target;
    PartitionId previous = (current == PartitionId::PARTITION_A) 
                          ? PartitionId::PARTITION_B 
                          : PartitionId::PARTITION_A;
    
    // Mark current partition as error
    setPartitionState(current, PartitionState::STATE_ROLLBACK);
    
    // Switch to previous partition
    boot_status_.boot_target = previous;
    boot_status_.boot_count = 0;
    
    if (!writeBootStatus()) {
        std::cerr << "[PARTITION] Failed to write boot status during rollback\n";
        return false;
    }
    
    std::cout << "[PARTITION] ✓ Rollback completed. Boot target: " 
              << (previous == PartitionId::PARTITION_A ? "A" : "B") << "\n";
    
    return true;
}

// ==================== Helper Functions ====================

std::string PartitionManager::getPartitionPath(PartitionId partition) const {
    if (partition == PartitionId::PARTITION_A) {
        return partition_a_path_;
    } else if (partition == PartitionId::PARTITION_B) {
        return partition_b_path_;
    }
    return "";
}

bool PartitionManager::readBootStatus() {
    std::ifstream file(boot_status_path_, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&boot_status_), sizeof(BootStatus));
    file.close();
    
    // Verify magic number
    if (boot_status_.magic_number != PARTITION_MAGIC_NUMBER) {
        return false;
    }
    
    return true;
}

bool PartitionManager::writeBootStatus() {
    std::ofstream file(boot_status_path_, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[PARTITION] Failed to open boot status file: " << boot_status_path_ << "\n";
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(&boot_status_), sizeof(BootStatus));
    if (!file.good()) {
        std::cerr << "[PARTITION] Failed to write boot status\n";
        return false;
    }
    
    file.close();
    return true;
}

// ==================== Data Partition Management ====================

bool PartitionManager::mountDataPartition() {
    if (simulation_mode_) {
        // Simulation mode: just create directory
        std::string cmd = "mkdir -p " + data_mount_point_;
        system(cmd.c_str());
        data_mounted_ = true;
        return true;
    }
    
    // Check if already mounted
    if (isDataPartitionMounted()) {
        std::cout << "[PARTITION] Data partition already mounted\n";
        data_mounted_ = true;
        return true;
    }
    
    // Create mount point
    std::string mkdir_cmd = "mkdir -p " + data_mount_point_;
    system(mkdir_cmd.c_str());
    
    // Mount data partition
    std::string mount_cmd = "mount " + data_partition_path_ + " " + data_mount_point_;
    int result = system(mount_cmd.c_str());
    
    if (result != 0) {
        std::cerr << "[PARTITION] Failed to mount data partition: " << data_partition_path_ << "\n";
        return false;
    }
    
    data_mounted_ = true;
    return true;
}

bool PartitionManager::isDataPartitionMounted() const {
    // Check /proc/mounts
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(data_mount_point_) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool PartitionManager::createSimulationEnvironment() {
    std::cout << "[PARTITION] Creating simulation environment (3-sector)...\n";
    
    // Create partition directories
    system("mkdir -p /tmp/vmg_partitions");
    system("mkdir -p /tmp/vmg_partitions/data");
    system("mkdir -p /tmp/vmg_partitions/data/ota/downloads");
    system("mkdir -p /tmp/vmg_partitions/data/ota/zones");
    system("mkdir -p /tmp/vmg_partitions/data/log");
    
    // Create empty partition files (100MB each for simulation)
    const size_t partition_size = 100 * 1024 * 1024;  // 100MB
    
    // Create Partition A
    std::ofstream part_a(DEFAULT_SIM_PARTITION_A, std::ios::binary | std::ios::trunc);
    if (part_a.is_open()) {
        std::vector<uint8_t> zeros(partition_size, 0);
        part_a.write(reinterpret_cast<const char*>(zeros.data()), partition_size);
        part_a.close();
    }
    
    // Create Partition B
    std::ofstream part_b(DEFAULT_SIM_PARTITION_B, std::ios::binary | std::ios::trunc);
    if (part_b.is_open()) {
        std::vector<uint8_t> zeros(partition_size, 0);
        part_b.write(reinterpret_cast<const char*>(zeros.data()), partition_size);
        part_b.close();
    }
    
    std::cout << "[PARTITION] ✓ Simulation environment created\n";
    return true;
}
