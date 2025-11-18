/**
 * @file ota_manager.cpp
 * @brief OTA Manager Implementation
 * 
 * Based on ZGW modular design principles
 */

#include "ota_manager.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

// ==================== Constructor ====================

OTAManager::OTAManager(
    ConfigManager& config,
    HttpClient* http_client,
    MqttClient* mqtt_client,
    std::shared_ptr<PartitionManager> partition_mgr,
    std::vector<std::shared_ptr<DoIPClient>> doip_clients
) : config_(config),
    http_client_(http_client),
    mqtt_client_(mqtt_client),
    partition_mgr_(partition_mgr),
    doip_clients_(doip_clients),
    current_state_(OTAState::OTA_IDLE),
    chunk_size_(OTA_DOWNLOAD_CHUNK_SIZE),
    max_retries_(OTA_MAX_RETRY_ATTEMPTS)
{
    std::memset(&progress_, 0, sizeof(OTAProgress));
    progress_.state = OTAState::OTA_IDLE;
}

// ==================== Initialization ====================

bool OTAManager::initialize() {
    std::cout << "[OTA] Initializing OTA Manager...\n";
    
    // Get paths from config
    download_path_ = config_.getOtaDownloadPath();
    install_path_ = config_.getOtaInstallPath();
    
    // Create directories if they don't exist
    system(("mkdir -p " + download_path_).c_str());
    system(("mkdir -p " + install_path_).c_str());
    
    std::cout << "[OTA] ✓ Download path: " << download_path_ << "\n";
    std::cout << "[OTA] ✓ Install path: " << install_path_ << "\n";
    std::cout << "[OTA] ✓ OTA Manager initialized\n";
    
    return true;
}

// ==================== OTA Process ====================

bool OTAManager::startOTA(const OTAPackageInfo& package_info) {
    std::cout << "\n[OTA] ========================================\n";
    std::cout << "[OTA] Starting OTA Update\n";
    std::cout << "[OTA] ========================================\n";
    std::cout << "[OTA] Campaign ID: " << package_info.campaign_id << "\n";
    std::cout << "[OTA] Package Size: " << package_info.package_size << " bytes\n";
    std::cout << "[OTA] Firmware Version: 0x" << std::hex << package_info.firmware_version << std::dec << "\n";
    std::cout << "[OTA] ========================================\n\n";
    
    // Check if OTA already in progress
    if (isOTAInProgress()) {
        std::cerr << "[OTA] ✗ OTA already in progress\n";
        return false;
    }
    
    // Store package info
    package_info_ = package_info;
    
    // Reset progress
    std::memset(&progress_, 0, sizeof(OTAProgress));
    progress_.total_bytes = package_info.package_size;
    
    // Step 1: Download package
    updateState(OTAState::OTA_DOWNLOADING, "Downloading OTA package");
    if (!downloadPackage()) {
        reportError("Download failed");
        return false;
    }
    
    // Step 2: Verify package
    updateState(OTAState::OTA_VERIFYING, "Verifying package integrity");
    if (!verifyPackage()) {
        reportError("Verification failed");
        return false;
    }
    
    // Step 3: Install to standby partition
    updateState(OTAState::OTA_INSTALLING, "Installing to standby partition");
    if (!installPackage()) {
        reportError("Installation failed");
        return false;
    }
    
    // Step 4: Ready to reboot
    updateState(OTAState::OTA_READY, "OTA completed, ready to reboot");
    std::cout << "\n[OTA] ========================================\n";
    std::cout << "[OTA] ✓ OTA Update Completed Successfully\n";
    std::cout << "[OTA] ⚠️  Reboot required to apply changes\n";
    std::cout << "[OTA] ========================================\n\n";
    
    current_state_ = OTAState::OTA_COMPLETED;
    return true;
}

// ==================== Download ====================

bool OTAManager::downloadPackage() {
    std::cout << "[OTA] Downloading package from: " << package_info_.package_url << "\n";
    
    // Prepare download path
    std::string download_file = download_path_ + "/" + package_info_.campaign_id + ".bin";
    
    // Open output file
    std::ofstream output_file(download_file, std::ios::binary);
    if (!output_file.is_open()) {
        std::cerr << "[OTA] ✗ Failed to create download file: " << download_file << "\n";
        return false;
    }
    
    // Download in chunks (with Range Request support)
    size_t total_size = package_info_.package_size;
    size_t downloaded = 0;
    uint8_t last_reported_percentage = 0;
    
    while (downloaded < total_size) {
        size_t chunk_start = downloaded;
        size_t chunk_end = std::min(downloaded + chunk_size_ - 1, total_size - 1);
        
        // Download chunk with retry
        if (!downloadChunk(package_info_.package_url, chunk_start, chunk_end, output_file)) {
            std::cerr << "[OTA] ✗ Failed to download chunk: " << chunk_start << "-" << chunk_end << "\n";
            output_file.close();
            return false;
        }
        
        downloaded = chunk_end + 1;
        
        // Update progress
        updateProgress(downloaded, total_size);
        
        // Report progress every 5%
        uint8_t current_percentage = (downloaded * 100) / total_size;
        if (current_percentage >= last_reported_percentage + OTA_PROGRESS_REPORT_INTERVAL) {
            sendProgressReport();
            last_reported_percentage = current_percentage;
        }
    }
    
    output_file.close();
    
    std::cout << "[OTA] ✓ Download completed: " << download_file << "\n";
    return true;
}

bool OTAManager::downloadChunk(const std::string& url, size_t start, size_t end, std::ofstream& output_file) {
    for (uint32_t attempt = 0; attempt < max_retries_; attempt++) {
        // Prepare Range header
        std::map<std::string, std::string> headers;
        headers["Range"] = "bytes=" + std::to_string(start) + "-" + std::to_string(end);
        
        // Set headers
        http_client_->setHeaders(headers);
        
        // Download chunk
        HttpResponse response = http_client_->get(url);
        
        if (response.success && (response.status_code == 206 || response.status_code == 200)) {
            // Write to file
            output_file.write(response.body.c_str(), response.body.size());
            if (!output_file.good()) {
                std::cerr << "[OTA] ✗ Failed to write chunk to file\n";
                return false;
            }
            return true;
        }
        
        std::cerr << "[OTA] ⚠️  Chunk download failed (attempt " << (attempt + 1) << "/" << max_retries_ << ")\n";
        usleep(1000000);  // Wait 1 second before retry
    }
    
    return false;
}

// ==================== Verification ====================

bool OTAManager::verifyPackage() {
    std::cout << "[OTA] Verifying package integrity...\n";
    
    std::string download_file = download_path_ + "/" + package_info_.campaign_id + ".bin";
    
    // Calculate SHA256
    uint8_t calculated_hash[32];
    if (!calculateSHA256(download_file, calculated_hash)) {
        std::cerr << "[OTA] ✗ Failed to calculate SHA256\n";
        return false;
    }
    
    // Convert expected hash from hex string to binary
    uint8_t expected_hash[32];
    if (!hexToBinary(package_info_.sha256_hash, expected_hash)) {
        std::cerr << "[OTA] ✗ Invalid SHA256 format\n";
        return false;
    }
    
    // Compare hashes
    if (std::memcmp(calculated_hash, expected_hash, 32) != 0) {
        std::cerr << "[OTA] ✗ SHA256 mismatch! Package corrupted\n";
        
        // Print hashes for debugging
        std::cout << "[OTA] Expected: ";
        for (int i = 0; i < 32; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)expected_hash[i];
        }
        std::cout << "\n[OTA] Calculated: ";
        for (int i = 0; i < 32; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)calculated_hash[i];
        }
        std::cout << std::dec << "\n";
        
        return false;
    }
    
    std::cout << "[OTA] ✓ Package integrity verified\n";
    return true;
}

bool OTAManager::calculateSHA256(const std::string& file_path, uint8_t* hash) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[OTA] ✗ Failed to open file for hashing: " << file_path << "\n";
        return false;
    }
    
    // Use EVP API (recommended in OpenSSL 3.0)
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        file.close();
        return false;
    }
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        file.close();
        return false;
    }
    
    const size_t buffer_size = 8192;
    char buffer[buffer_size];
    
    while (file.good()) {
        file.read(buffer, buffer_size);
        size_t bytes_read = file.gcount();
        
        if (bytes_read > 0) {
            if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
                EVP_MD_CTX_free(mdctx);
                file.close();
                return false;
            }
        }
    }
    
    file.close();
    
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return false;
    }
    
    EVP_MD_CTX_free(mdctx);
    return true;
}

bool OTAManager::hexToBinary(const std::string& hex_string, uint8_t* binary) {
    if (hex_string.length() != 64) {  // SHA256 = 32 bytes = 64 hex chars
        return false;
    }
    
    for (size_t i = 0; i < 32; i++) {
        std::string byte_str = hex_string.substr(i * 2, 2);
        binary[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return true;
}

// ==================== Installation ====================

bool OTAManager::installPackage() {
    std::cout << "[OTA] Installing package to standby partition...\n";
    
    // Get standby partition
    PartitionId standby = partition_mgr_->getStandbyPartition();
    std::string standby_path = partition_mgr_->getPartitionPath(standby);
    
    std::cout << "[OTA] Target partition: " << (standby == PartitionId::PARTITION_A ? "A" : "B") << "\n";
    std::cout << "[OTA] Target path: " << standby_path << "\n";
    
    // Set partition state to UPDATING
    partition_mgr_->setPartitionState(standby, PartitionState::STATE_UPDATING);
    
    // Prepare metadata
    PartitionMetadata metadata;
    std::memset(&metadata, 0, sizeof(PartitionMetadata));
    metadata.magic_number = PARTITION_MAGIC_NUMBER;
    metadata.firmware_version = package_info_.firmware_version;
    metadata.build_timestamp = static_cast<uint32_t>(time(nullptr));
    metadata.total_size = package_info_.package_size;
    metadata.state = PartitionState::STATE_READY;
    
    // Convert hash from hex to binary
    hexToBinary(package_info_.sha256_hash, metadata.sha256_hash);
    
    // Write metadata to partition (first 1KB)
    std::ofstream partition_file(standby_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!partition_file.is_open()) {
        std::cerr << "[OTA] ✗ Failed to open partition for writing\n";
        partition_mgr_->setPartitionState(standby, PartitionState::STATE_ERROR);
        return false;
    }
    
    partition_file.write(reinterpret_cast<const char*>(&metadata), sizeof(PartitionMetadata));
    
    // Copy package data (skip metadata in source, write after metadata in partition)
    std::string download_file = download_path_ + "/" + package_info_.campaign_id + ".bin";
    std::ifstream source_file(download_file, std::ios::binary);
    if (!source_file.is_open()) {
        std::cerr << "[OTA] ✗ Failed to open downloaded package\n";
        partition_file.close();
        partition_mgr_->setPartitionState(standby, PartitionState::STATE_ERROR);
        return false;
    }
    
    // Copy data
    const size_t buffer_size = 8192;
    char buffer[buffer_size];
    size_t total_copied = 0;
    
    while (source_file.good() && total_copied < package_info_.package_size) {
        source_file.read(buffer, buffer_size);
        size_t bytes_read = source_file.gcount();
        
        if (bytes_read > 0) {
            partition_file.write(buffer, bytes_read);
            total_copied += bytes_read;
        }
    }
    
    source_file.close();
    partition_file.close();
    
    std::cout << "[OTA] ✓ Package installed (" << total_copied << " bytes)\n";
    
    // Verify partition
    std::cout << "[OTA] Verifying installed partition...\n";
    if (!partition_mgr_->verifyPartition(standby)) {
        std::cerr << "[OTA] ✗ Partition verification failed\n";
        partition_mgr_->setPartitionState(standby, PartitionState::STATE_ERROR);
        return false;
    }
    
    // Set partition state to READY
    partition_mgr_->setPartitionState(standby, PartitionState::STATE_READY);
    
    // Switch boot target
    std::cout << "[OTA] Switching boot target...\n";
    if (!partition_mgr_->switchBootTarget(standby)) {
        std::cerr << "[OTA] ✗ Failed to switch boot target\n";
        return false;
    }
    
    std::cout << "[OTA] ✓ Installation completed successfully\n";
    return true;
}

// ==================== Progress Reporting ====================

void OTAManager::updateState(OTAState state, const std::string& step_description) {
    current_state_ = state;
    progress_.state = state;
    progress_.current_step = step_description;
    
    std::cout << "[OTA] " << step_description << "...\n";
    
    sendProgressReport();
}

void OTAManager::updateProgress(uint32_t downloaded, uint32_t total) {
    progress_.downloaded_bytes = downloaded;
    progress_.total_bytes = total;
    progress_.percentage = (downloaded * 100) / total;
}

void OTAManager::reportError(const std::string& error_message) {
    current_state_ = OTAState::OTA_ERROR;
    progress_.state = OTAState::OTA_ERROR;
    progress_.error_message = error_message;
    
    std::cerr << "[OTA] ✗ ERROR: " << error_message << "\n";
    
    sendProgressReport();
}

void OTAManager::sendProgressReport() {
    if (!mqtt_client_) {
        return;
    }
    
    // Create progress JSON
    nlohmann::json progress_json;
    progress_json["state"] = static_cast<int>(progress_.state);
    progress_json["percentage"] = progress_.percentage;
    progress_json["downloaded_bytes"] = progress_.downloaded_bytes;
    progress_json["total_bytes"] = progress_.total_bytes;
    progress_json["current_step"] = progress_.current_step;
    
    if (!progress_.error_message.empty()) {
        progress_json["error"] = progress_.error_message;
    }
    
    // Send via MQTT (publish to ota/progress topic)
    std::string topic = "oem/" + config_.getVin() + "/ota/progress";
    mqtt_client_->publish(topic, progress_json.dump());
}

// ==================== Cancel ====================

bool OTAManager::cancelOTA() {
    if (!isOTAInProgress()) {
        return false;
    }
    
    std::cout << "[OTA] ⚠️  Cancelling OTA update...\n";
    
    reportError("OTA cancelled by user");
    current_state_ = OTAState::OTA_IDLE;
    
    return true;
}

