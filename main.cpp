/**
 * @file main.cpp
 * @brief Vehicle Mobile Gateway - Main Entry Point
 * 
 * Simplified main function with modular design (ZGW Style)
 * 
 * @version 2.0
 * @date 2024-11-15
 */

#include <iostream>
#include <csignal>
#include "config_manager.hpp"
#include "system_manager.hpp"

// Global Variables
volatile sig_atomic_t g_vmg_running = 1;
static SystemManager* g_system_manager = nullptr;

// Signal Handler
void SignalHandler(int signal) {
    g_vmg_running = 0;
    if (g_system_manager) {
        g_system_manager->stop();
    }
}

// Main Entry Point
int main(int argc, char* argv[]) {
    // Parse arguments
    bool interactive_mode = false;
    const char* config_file = "config.json";
    
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--interactive" || std::string(argv[i]) == "-i") {
            interactive_mode = true;
        } else {
            config_file = argv[i];
        }
    }
    
    // Load config
    ConfigManager config(config_file);
    if (!config.load()) {
        std::cerr << "[ERROR] Failed to load config\n";
        return 1;
    }
    
    // Setup signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    // Initialize system
    SystemManager system(config);
    g_system_manager = &system;
    
    if (!system.initialize()) {
        std::cerr << "[ERROR] Initialization failed\n";
        return 1;
    }
    
    // Power-on VCI (Daemon mode only)
    if (!interactive_mode) {
        system.performPowerOnVci();
    }
    
    // Main loop
    if (interactive_mode) {
        system.runInteractive();
    } else {
        system.runDaemon();
    }
    
    // Shutdown
    system.shutdown();
    return 0;
}
