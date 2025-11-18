/**
 * @file doip_client.hpp
 * @brief DoIP Client for VMG (Diagnostics over IP)
 * 
 * VMG → ZGW communication using DoIP/UDS
 * ISO 13400 (DoIP) + ISO 14229 (UDS)
 * 
 * Parallel design with ZGW: Libraries/DoIP/doip_client.h
 */

#ifndef DOIP_CLIENT_HPP
#define DOIP_CLIENT_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

/*******************************************************************************
 * DoIP Protocol Constants (ISO 13400-2)
 ******************************************************************************/

// Protocol Version
constexpr uint8_t DOIP_PROTOCOL_VERSION = 0x02;
constexpr uint8_t DOIP_INVERSE_VERSION = 0xFD;

// DoIP Header Size
constexpr size_t DOIP_HEADER_SIZE = 8;

// Logical Addresses (must match ZGW)
constexpr uint16_t DOIP_VMG_ADDRESS = 0x0200;  // VMG logical address
constexpr uint16_t DOIP_ZGW_ADDRESS = 0x0100;  // ZGW logical address

// Timeouts (milliseconds)
constexpr int DOIP_TIMEOUT_CONNECTION = 3000;    // TCP connection: 3s
constexpr int DOIP_TIMEOUT_ROUTING = 2000;       // Routing activation: 2s
constexpr int DOIP_TIMEOUT_DIAGNOSTIC = 5000;    // UDS response: 5s

/*******************************************************************************
 * DoIP Payload Types (ISO 13400-2)
 ******************************************************************************/

enum class DoIPPayloadType : uint16_t {
    GENERIC_NACK = 0x0000,
    ROUTING_ACTIVATION_REQUEST = 0x0005,
    ROUTING_ACTIVATION_RESPONSE = 0x0006,
    ALIVE_CHECK_REQUEST = 0x0007,
    ALIVE_CHECK_RESPONSE = 0x0008,
    DIAGNOSTIC_MESSAGE = 0x8001,
    DIAGNOSTIC_MESSAGE_ACK = 0x8002,
    DIAGNOSTIC_MESSAGE_NACK = 0x8003,
    
    // Custom payload types (parallel with ZGW)
    VCI_REPORT = 0x9000,           // VCI Report from ZGW
    READINESS_REPORT = 0x9001      // Readiness Report from ZGW (was HEALTH_STATUS in ZGW)
};

/*******************************************************************************
 * UDS Service IDs (ISO 14229)
 ******************************************************************************/

enum class UDSService : uint8_t {
    READ_DATA_BY_ID = 0x22,
    WRITE_DATA_BY_ID = 0x2E,
    ROUTINE_CONTROL = 0x31,
    REQUEST_DOWNLOAD = 0x34,
    TRANSFER_DATA = 0x36,
    REQUEST_TRANSFER_EXIT = 0x37,
    
    // Positive response offset
    POSITIVE_RESPONSE = 0x40
};

/*******************************************************************************
 * UDS Routine Control IDs (parallel with ZGW vmg_server.py)
 ******************************************************************************/

constexpr uint16_t RID_VCI_COLLECTION_START = 0xF001;   // Start VCI collection
constexpr uint16_t RID_VCI_SEND_REPORT = 0xF002;        // Request VCI report
constexpr uint16_t RID_READINESS_CHECK = 0xF003;        // Start readiness check
constexpr uint16_t RID_READINESS_SEND_REPORT = 0xF004;  // Request readiness report

/*******************************************************************************
 * DoIP Message Structures
 ******************************************************************************/

// DoIP Header (8 bytes, big-endian)
struct DoIPHeader {
    uint8_t protocol_version;           // 0x02
    uint8_t inverse_protocol_version;   // 0xFD
    uint16_t payload_type;              // DoIPPayloadType
    uint32_t payload_length;            // Payload length in bytes
} __attribute__((packed));

/*******************************************************************************
 * VCI Structure (parallel with ZGW DoIP_VCI_Info)
 ******************************************************************************/

struct VCIInfo {
    char ecu_id[16];        // ECU ID (e.g., "ECU_091")
    char sw_version[8];     // Software version (e.g., "0.0.0")
    char hw_version[8];     // Hardware version (e.g., "0.0.0")
    char serial_num[16];    // Serial number (e.g., "091000001")
} __attribute__((packed));  // Total: 48 bytes per ECU

/*******************************************************************************
 * Readiness Structure (parallel with ZGW OTA_ReadinessInfo)
 ******************************************************************************/

struct ReadinessInfo {
    char ecu_id[16];            // ECU ID
    uint8_t vehicle_parked;     // 1 = parked
    uint8_t engine_off;         // 1 = engine off
    uint16_t battery_voltage_mv;// Battery voltage (mV)
    uint32_t available_memory_kb;// Available memory (KB)
    uint8_t all_doors_closed;   // 1 = all closed
    uint8_t compatible;         // 1 = SW compatible
    uint8_t ready_for_update;   // 1 = ready for OTA
} __attribute__((packed));      // Total: 27 bytes per ECU

/*******************************************************************************
 * DoIP Client States
 ******************************************************************************/

enum class DoIPClientState {
    IDLE,           // Not connected
    CONNECTING,     // TCP connection in progress
    CONNECTED,      // TCP connected, waiting for routing activation
    ACTIVE,         // Routing activated, ready for communication
    ERROR           // Error state
};

/*******************************************************************************
 * @class DoIPClient
 * @brief DoIP/UDS Client for VMG (parallel with ZGW DoIP_Client)
 * 
 * Responsibilities:
 * 1. TCP connection to ZGW
 * 2. DoIP routing activation
 * 3. UDS command transmission (Routine Control 0x31)
 * 4. VCI/Readiness Report reception (0x9000/0x9001)
 ******************************************************************************/

class DoIPClient {
public:
    /**
     * @brief Constructor
     * @param zgw_ip ZGW IP address
     * @param zgw_port ZGW DoIP port (default: 13400)
     */
    DoIPClient(const std::string& zgw_ip, uint16_t zgw_port = 13400);
    
    ~DoIPClient();
    
    /***************************************************************************
     * Connection Management (parallel with ZGW DoIP_Client_Init/Close)
     **************************************************************************/
    
    /**
     * @brief Connect to ZGW and activate routing
     * @return true if connected and routing activated
     */
    bool connect();
    
    /**
     * @brief Disconnect from ZGW
     */
    void disconnect();
    
    /**
     * @brief Check if DoIP is active (routing activated)
     * @return true if active
     */
    bool isActive() const;
    
    /**
     * @brief Get current DoIP state
     */
    DoIPClientState getState() const;
    
    /***************************************************************************
     * UDS Routine Control Commands (parallel with vmg_server.py)
     **************************************************************************/
    
    /**
     * @brief Request VCI collection from ZGW
     * @details Sends UDS 0x31 01 F001 (Start VCI Collection)
     * @return true if command accepted
     */
    bool requestVCICollection();
    
    /**
     * @brief Request VCI report from ZGW
     * @details Sends UDS 0x31 01 F002 (Send VCI Report)
     *          ZGW will respond with DoIP 0x9000 (VCI_REPORT)
     * @param vci_list Output: VCI data for all ECUs
     * @return true if successful
     */
    bool requestVCIReport(std::vector<VCIInfo>& vci_list);
    
    /**
     * @brief Request readiness check from ZGW
     * @details Sends UDS 0x31 01 F003 (Start Readiness Check)
     * @return true if command accepted
     */
    bool requestReadinessCheck();
    
    /**
     * @brief Request readiness report from ZGW
     * @details Sends UDS 0x31 01 F004 (Send Readiness Report)
     *          ZGW will respond with DoIP 0x9001 (READINESS_REPORT)
     * @param readiness_list Output: Readiness data for all ECUs
     * @return true if successful
     */
    bool requestReadinessReport(std::vector<ReadinessInfo>& readiness_list);
    
    /***************************************************************************
     * OTA Firmware Transfer (UDS 0x34/36/37)
     **************************************************************************/
    
    /**
     * @brief Send firmware to ZGW for a specific ECU
     * @param ecu_id Target ECU ID (e.g., "ECU_011")
     * @param firmware_data Binary firmware
     * @return true if transfer successful
     */
    bool sendFirmware(const std::string& ecu_id, 
                      const std::vector<uint8_t>& firmware_data);

private:
    /***************************************************************************
     * Private Members
     **************************************************************************/
    
    std::string zgw_ip_;
    uint16_t zgw_port_;
    int socket_fd_;
    DoIPClientState state_;
    
    /***************************************************************************
     * DoIP Low-Level Functions
     **************************************************************************/
    
    /**
     * @brief Send Routing Activation Request (0x0005)
     * @return true if routing activated (response 0x0006)
     */
    bool activateRouting();
    
    /**
     * @brief Build DoIP message (header + payload)
     * @param payload_type DoIP payload type
     * @param payload Payload data
     * @return Complete DoIP message
     */
    std::vector<uint8_t> buildDoIPMessage(DoIPPayloadType payload_type,
                                           const std::vector<uint8_t>& payload);
    
    /**
     * @brief Parse DoIP header and extract payload
     * @param response Raw DoIP message
     * @param payload_type Output: payload type
     * @param payload Output: payload data
     * @return true if valid DoIP message
     */
    bool parseDoIPMessage(const std::vector<uint8_t>& response,
                          DoIPPayloadType& payload_type,
                          std::vector<uint8_t>& payload);
    
    /***************************************************************************
     * UDS Low-Level Functions
     **************************************************************************/
    
public:
    /**
     * @brief Send UDS diagnostic message (0x8001)
     * @param service_id UDS service ID (e.g., 0x31)
     * @param data UDS payload
     * @return UDS response data
     */
    std::vector<uint8_t> sendDiagnosticMessage(uint8_t service_id,
                                                const std::vector<uint8_t>& data);
    
private:
    
    /**
     * @brief Send UDS Routine Control (0x31)
     * @param routine_id Routine ID (e.g., 0xF001)
     * @param subfunction Sub-function (0x01 = Start)
     * @return UDS response
     */
    std::vector<uint8_t> sendRoutineControl(uint16_t routine_id, 
                                             uint8_t subfunction = 0x01);
    
    /***************************************************************************
     * Socket Low-Level Functions
     **************************************************************************/
    
    /**
     * @brief Send raw bytes to socket
     */
    bool sendRaw(const std::vector<uint8_t>& data);
    
    /**
     * @brief Receive raw bytes from socket
     * @param timeout_ms Timeout in milliseconds
     * @return Received data
     */
    std::vector<uint8_t> receiveRaw(int timeout_ms = DOIP_TIMEOUT_DIAGNOSTIC);
    
    /**
     * @brief Receive exactly N bytes (blocking)
     */
    bool receiveExact(std::vector<uint8_t>& buffer, size_t size, int timeout_ms);
};

#endif // DOIP_CLIENT_HPP

