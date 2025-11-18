/**
 * @file system_manager.cpp
 * @brief System Manager Implementation
 */

#include "system_manager.hpp"
#include <iostream>
#include <csignal>
#include <ctime>
#include <algorithm>

SystemManager::SystemManager(ConfigManager& config)
    : config_(config),
      running_(false),
      trigger_vci_collection_(false),
      trigger_readiness_check_(false),
      trigger_ota_start_(false),
      heartbeat_timer_(0),
      last_heartbeat_time_(0) {
}

bool SystemManager::initialize() {
    std::cout << "\n[INIT] Initializing VMG System...\n";
    
    // 1. Initialize HTTP Client
    std::cout << "[INIT] Setting up HTTP client...\n";
    std::string protocol = config_.useHttps() ? "https" : "http";
    std::string base_url = protocol + "://" + config_.getServerHost() + ":" + 
                          std::to_string(config_.getHttpPort()) + config_.getApiBase();
    
    http_client_ = std::make_unique<HttpClient>(base_url, config_.verifyPeer());
    std::cout << "[INIT] ✓ HTTP client initialized\n";
    
    // 2. Initialize MQTT Client
    std::cout << "[INIT] Setting up MQTT client...\n";
    std::string client_id = config_.getDeviceId() + "_mqtt";
    std::string vin = config_.getVin();  // Get VIN for topic generation
    
    mqtt_client_ = std::make_unique<MqttClient>(
        config_.getServerHost(),
        config_.getMqttPort(),
        client_id,
        vin,  // Pass VIN for oem/{vin}/* topics
        config_.useMqttTls(),
        config_.verifyPeer()
    );
    std::cout << "[INIT] ✓ MQTT client initialized\n";
    
    // 3. Test HTTP connection
    std::cout << "\n[CONN] Testing HTTP connection...\n";
    auto health_response = http_client_->get(config_.getHealthEndpoint());
    if (!health_response.success) {
        std::cerr << "[ERROR] HTTP connection failed: " << health_response.error << std::endl;
        return false;
    }
    std::cout << "[CONN] ✓ HTTP connected\n";
    
    // 4. Connect MQTT
    std::cout << "[CONN] Connecting to MQTT broker...\n";
    if (!mqtt_client_->connect()) {
        std::cerr << "[ERROR] MQTT connection failed\n";
        return false;
    }
    std::cout << "[CONN] ✓ MQTT connected\n";
    
    // 5. Setup MQTT callback and subscribe
    setupMqttCallback();
    
    // Subscribe to server command topics (oem/{vin}/*)
    std::string command_topic = "oem/" + vin + "/command";
    std::string ota_campaign_topic = "oem/" + vin + "/ota/campaign";
    std::string ota_metadata_topic = "oem/" + vin + "/ota/metadata";
    
    if (!mqtt_client_->subscribe(command_topic)) {
        std::cerr << "[ERROR] Failed to subscribe to command topic\n";
        return false;
    }
    std::cout << "[CONN] ✓ Subscribed to " << command_topic << "\n";
    
    if (!mqtt_client_->subscribe(ota_campaign_topic)) {
        std::cerr << "[ERROR] Failed to subscribe to OTA campaign topic\n";
        return false;
    }
    std::cout << "[CONN] ✓ Subscribed to " << ota_campaign_topic << "\n";
    
    if (!mqtt_client_->subscribe(ota_metadata_topic)) {
        std::cerr << "[ERROR] Failed to subscribe to OTA metadata topic\n";
        return false;
    }
    std::cout << "[CONN] ✓ Subscribed to " << ota_metadata_topic << "\n";
    
    // 6. Initialize DoIP Client (shared by VCI and Readiness)
    std::cout << "[INIT] Setting up DoIP client...\n";
    doip_client_ = std::make_shared<DoIPClient>(
        config_.getZgwIp(),
        config_.getZgwDoipPort()
    );
    std::cout << "[INIT] ✓ DoIP client initialized\n";
    
    // 7. Initialize subsystems
    vehicle_state_ = std::make_unique<VehicleStateManager>();
    vci_collector_ = std::make_unique<VCICollector>(config_, *http_client_, doip_client_);
    readiness_manager_ = std::make_unique<ReadinessManager>(config_, *mqtt_client_, doip_client_);
    
    // 8. Initialize OTA components (parallel with ZGW FlashBankManager)
    std::cout << "[INIT] Setting up OTA components...\n";
    
    // Partition Manager (simulation mode for development)
    partition_mgr_ = std::make_shared<PartitionManager>(
        config_.getPartitionAPath(),
        config_.getPartitionBPath(),
        "/dev/mmcblk0p4",  // data_partition_path
        "/data",           // data_mount_point
        config_.getBootStatusPath(),
        true  // simulation_mode = true
    );
    
    if (!partition_mgr_->initialize()) {
        std::cerr << "[ERROR] Failed to initialize Partition Manager\n";
        return false;
    }
    std::cout << "[INIT] ✓ Partition Manager initialized\n";
    
    // OTA Manager
    ota_manager_ = std::make_unique<OTAManager>(
        config_,
        http_client_.get(),
        mqtt_client_.get(),
        partition_mgr_,
        std::vector<std::shared_ptr<DoIPClient>>{}  // No DoIP clients initially
    );
    
    if (!ota_manager_->initialize()) {
        std::cerr << "[ERROR] Failed to initialize OTA Manager\n";
        return false;
    }
    std::cout << "[INIT] ✓ OTA Manager initialized\n";
    
    std::cout << "[INIT] ✓ All subsystems initialized\n";
    
    running_ = true;
    return true;
}

bool SystemManager::performPowerOnVci() {
    std::cout << "\n[BOOT] Performing power-on sequence...\n";
    
    // 1. Send vehicle wake-up message to server
    std::string vmg_sw_version = config_.getSoftwareVersion();
    std::string vehicle_state = vehicle_state_->getStateString();
    
    if (mqtt_client_->sendWakeUp(vmg_sw_version, vehicle_state)) {
        std::cout << "[BOOT] ✓ Vehicle wake-up sent\n";
    } else {
        std::cerr << "[BOOT] ✗ Failed to send wake-up\n";
    }
    
    // 2. Collect and upload VCI
    std::cout << "[BOOT] Collecting VCI...\n";
    return vci_collector_->collectAndUpload("power_on");
}

void SystemManager::setupMqttCallback() {
    mqtt_client_->setMessageCallback([this](const std::string& topic, const std::string& payload) {
        handleMqttCommand(topic, payload);
    });
}

void SystemManager::handleMqttCommand(const std::string& topic, const std::string& payload) {
    try {
        nlohmann::json cmd = nlohmann::json::parse(payload);
        std::string command = cmd["command"];
        
        std::cout << "\n[MQTT] Command received: " << command << "\n";
        
        if (command == "collect_vci") {
            std::cout << "       Reason: " << cmd.value("reason", "unknown") << "\n";
            trigger_vci_collection_ = true;
            
        } else if (command == "collect_readiness") {
            std::cout << "       Reason: " << cmd.value("reason", "unknown") << "\n";
            trigger_readiness_check_ = true;
            
        } else if (command == "start_ota") {
            std::string campaign_id = cmd.value("campaign_id", "unknown");
            std::cout << "       Campaign ID: " << campaign_id << "\n";
            
            // Set OTA trigger flag
            trigger_ota_start_ = true;
            
            // TODO: Store OTA package info for processEvents() to pick up
            
        } else if (command == "shutdown") {
            std::cout << "       Initiating graceful shutdown...\n";
            stop();
            
        } else {
            std::cout << "       Unknown command\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[MQTT] Error parsing command: " << e.what() << std::endl;
    }
}

void SystemManager::processEvents() {
    // Process MQTT messages
    mqtt_client_->loop(100);
    
    // Handle VCI collection trigger
    if (trigger_vci_collection_.exchange(false)) {
        std::cout << "\n[VCI] External VCI collection requested\n";
        
        if (vci_collector_->collectAndUpload("external_request")) {
            // Send ACK via MQTT
            std::string status_topic = config_.getStatusTopic(config_.getDeviceId());
            nlohmann::json ack = {
                {"device_id", config_.getDeviceId()},
                {"event", "vci_collected"},
                {"timestamp", std::time(nullptr)}
            };
            mqtt_client_->publish(status_topic, ack.dump());
        }
    }
    
    // Handle Readiness check trigger
    if (trigger_readiness_check_.exchange(false)) {
        std::cout << "\n[READY] External readiness check requested\n";
        readiness_manager_->checkAndPublish("external_request");
    }
    
    // Handle OTA start trigger
    if (trigger_ota_start_.exchange(false)) {
        std::cout << "\n[OTA] OTA update requested\n";
        
        // TODO: Parse OTA package info from MQTT message
        // For now, use mock data for testing
        OTAPackageInfo package_info;
        package_info.campaign_id = "campaign_test_001";
        package_info.package_url = "http://localhost:5000/packages/campaign_test_001/full_package.bin";
        package_info.package_size = 10485760;  // 10MB for testing
        package_info.firmware_version = 0x01020003;  // v1.2.3
        package_info.sha256_hash = "0000000000000000000000000000000000000000000000000000000000000000";  // Mock
        
        // Start OTA update (non-blocking, runs in background)
        if (ota_manager_->startOTA(package_info)) {
            std::cout << "[OTA] ✓ OTA update started\n";
        } else {
            std::cerr << "[OTA] ✗ Failed to start OTA update\n";
        }
    }
}

void SystemManager::processHeartbeat() {
    // Get adaptive interval based on vehicle state
    int interval = getAdaptiveHeartbeatInterval();
    
    uint32_t current_time = std::time(nullptr);
    
    if (current_time - last_heartbeat_time_ >= (uint32_t)interval) {
        last_heartbeat_time_ = current_time;
        publishHeartbeat();
    }
}

void SystemManager::publishHeartbeat() {
    if (!config_.isHeartbeatEnabled()) {
        return;
    }
    
    std::string vehicle_state = vehicle_state_->getStateString();
    
    if (mqtt_client_->sendHeartbeat(vehicle_state, heartbeat_timer_)) {
        std::cout << "[HB] ♥ Heartbeat published (state: " 
                  << vehicle_state << ")\n";
    } else {
        std::cerr << "[HB] ✗ Failed to publish heartbeat\n";
    }
    
    heartbeat_timer_++;
}

int SystemManager::getAdaptiveHeartbeatInterval() {
    if (!config_.isAdaptiveHeartbeat()) {
        return config_.getHeartbeatInterval();
    }
    
    // Get interval based on vehicle state
    std::string state_str = vehicle_state_->getStateString();
    
    // Convert to lowercase for matching config keys
    std::string state_key = state_str;
    std::transform(state_key.begin(), state_key.end(), state_key.begin(), ::tolower);
    
    return config_.getHeartbeatInterval(state_key);
}

void SystemManager::shutdown() {
    std::cout << "\n[SHUTDOWN] Cleaning up VMG System...\n";
    
    // Disconnect MQTT (will send LWT if configured)
    mqtt_client_->disconnect();
    std::cout << "[SHUTDOWN] ✓ MQTT disconnected\n";
    
    std::cout << "[SHUTDOWN] ✓ VMG gracefully shut down\n";
}

void SystemManager::runInteractive() {
    std::cout << "\n[INTERACTIVE] Mode enabled - Manual command execution\n";
    
    while (running_) {
        std::cout << "\n╔════════════════════════════════════════════╗\n";
        std::cout << "║       VMG Interactive Command Menu        ║\n";
        std::cout << "╠════════════════════════════════════════════╣\n";
        std::cout << "║  1. Collect VCI from ZGW                  ║\n";
        std::cout << "║  2. Check Readiness from ZGW              ║\n";
        std::cout << "║  3. Process Events                        ║\n";
        std::cout << "║  4. Send Heartbeat                        ║\n";
        std::cout << "║  0. Exit                                  ║\n";
        std::cout << "╚════════════════════════════════════════════╝\n";
        std::cout << "Enter choice: ";
        
        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }
        
        switch (choice) {
            case 1: performPowerOnVci(); break;
            case 2: std::cout << "[INFO] Use MQTT command\n"; break;
            case 3: processEvents(); break;
            case 4: processHeartbeat(); break;
            case 0: stop(); return;
            default: break;
        }
        
        std::cin.ignore(10000, '\n');
    }
}

void SystemManager::runDaemon() {
    std::cout << "\n[DAEMON] Mode enabled - Automatic operation\n";
    std::cout << "[MAIN] Entering main loop (Press Ctrl+C to exit)...\n\n";
    
    while (running_) {
        processEvents();
        processHeartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

