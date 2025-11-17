/**
 * @file system_manager.hpp
 * @brief System Manager - Orchestrates all VMG components
 * 
 * Main system controller that integrates all modules
 * Parallel design with ZGW: Cpu0_Main.c + SystemMain.c
 */

#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP

#include <memory>
#include <atomic>
#include "config_manager.hpp"
#include "vehicle_state.hpp"
#include "vci_collector.hpp"
#include "readiness_manager.hpp"
#include "http_client.hpp"
#include "mqtt_client.hpp"
#include "doip_client.hpp"
#include "partition_manager.hpp"
#include "ota_manager.hpp"

/**
 * @brief System Manager Class
 * 
 * Orchestrates initialization, event handling, and lifecycle management
 */
class SystemManager {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager
     */
    explicit SystemManager(ConfigManager& config);
    
    /**
     * @brief Initialize all subsystems
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Check if system is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Stop the system
     */
    void stop() { running_ = false; }
    
    /**
     * @brief Process incoming events (MQTT commands, etc.)
     */
    void processEvents();
    
    /**
     * @brief Process heartbeat logic (adaptive based on vehicle state)
     */
    void processHeartbeat();
    
    /**
     * @brief Graceful shutdown
     */
    void shutdown();
    
    /**
     * @brief Perform power-on VCI collection (1-time)
     */
    bool performPowerOnVci();
    
    /**
     * @brief Run in interactive mode (manual command execution)
     */
    void runInteractive();
    
    /**
     * @brief Run in daemon mode (automatic operation)
     */
    void runDaemon();

private:
    ConfigManager& config_;
    std::atomic<bool> running_;
    
    // Subsystem components
    std::unique_ptr<HttpClient> http_client_;
    std::unique_ptr<MqttClient> mqtt_client_;
    std::shared_ptr<DoIPClient> doip_client_;  // Shared: used by VCI and Readiness
    std::unique_ptr<VehicleStateManager> vehicle_state_;
    std::unique_ptr<VCICollector> vci_collector_;
    std::unique_ptr<ReadinessManager> readiness_manager_;
    
    // OTA components (parallel with ZGW FlashBankManager)
    std::shared_ptr<PartitionManager> partition_mgr_;
    std::unique_ptr<OTAManager> ota_manager_;
    
    // Event triggers (set by MQTT callback)
    std::atomic<bool> trigger_vci_collection_;
    std::atomic<bool> trigger_readiness_check_;
    std::atomic<bool> trigger_ota_start_;  // New: OTA trigger
    
    // Timers
    uint32_t heartbeat_timer_;
    uint32_t last_heartbeat_time_;
    
    /**
     * @brief Setup MQTT message callback
     */
    void setupMqttCallback();
    
    /**
     * @brief Handle MQTT command
     */
    void handleMqttCommand(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Publish heartbeat based on current vehicle state
     */
    void publishHeartbeat();
    
    /**
     * @brief Get adaptive heartbeat interval based on vehicle state
     */
    int getAdaptiveHeartbeatInterval();
};

#endif // SYSTEM_MANAGER_HPP

