/**
 * @file vci_collector.hpp
 * @brief VCI Collection Module
 * 
 * Collects Vehicle Configuration Information from ZGW via DoIP/UDS
 * Parallel design with ZGW: Libraries/DataCollection/vci_manager.c
 */

#ifndef VCI_COLLECTOR_HPP
#define VCI_COLLECTOR_HPP

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "config_manager.hpp"
#include "http_client.hpp"
#include "doip_client.hpp"

/**
 * @brief VCI Collector Class
 */
class VCICollector {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager
     * @param http_client HTTP client for uploading VCI
     * @param doip_client DoIP client for ZGW communication
     */
    VCICollector(const ConfigManager& config, HttpClient& http_client, 
                 std::shared_ptr<DoIPClient> doip_client);
    
    /**
     * @brief Collect VCI from ZGW via DoIP/UDS
     * @return true if successful
     */
    bool collect();
    
    /**
     * @brief Upload collected VCI to server
     * @return true if successful
     */
    bool upload();
    
    /**
     * @brief Collect and upload VCI (convenience method)
     * @param trigger Trigger reason (e.g., "power_on", "external_request")
     * @return true if successful
     */
    bool collectAndUpload(const std::string& trigger = "manual");
    
    /**
     * @brief Get last collected VCI data
     */
    const nlohmann::json& getVciData() const { return vci_data_; }

private:
    const ConfigManager& config_;
    HttpClient& http_client_;
    std::shared_ptr<DoIPClient> doip_client_;
    nlohmann::json vci_data_;
    
    /**
     * @brief Query ZGW for VCI via DoIP/UDS
     * @details Sends UDS Routine Control (0x31 01 F001/F002) to ZGW
     *          and receives VCI Report (0x9000)
     */
    bool queryZgwVci();
    
    /**
     * @brief Convert binary VCI data to JSON format
     * @param vci_list Binary VCI data from ZGW
     * @return JSON formatted VCI data
     */
    nlohmann::json convertVciToJson(const std::vector<VCIInfo>& vci_list);
    
    /**
     * @brief Generate mock VCI data for testing (fallback)
     */
    nlohmann::json generateMockVci(const std::string& trigger);
};

#endif // VCI_COLLECTOR_HPP

