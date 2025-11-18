/**
 * @file doip_client.cpp
 * @brief DoIP Client Implementation for VMG
 * 
 * Parallel design with ZGW: Libraries/DoIP/doip_client.c
 */

#include "doip_client.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

/*******************************************************************************
 * Helper Functions - Endianness Conversion
 ******************************************************************************/

static uint16_t hton16(uint16_t value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

static uint32_t hton32(uint32_t value) {
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
           ((value & 0xFF0000) >> 8) | ((value >> 24) & 0xFF);
}

static uint16_t ntoh16(uint16_t value) {
    return hton16(value);  // Same operation
}

static uint32_t ntoh32(uint32_t value) {
    return hton32(value);  // Same operation
}

/*******************************************************************************
 * DoIPClient Constructor/Destructor
 ******************************************************************************/

DoIPClient::DoIPClient(const std::string& zgw_ip, uint16_t zgw_port)
    : zgw_ip_(zgw_ip)
    , zgw_port_(zgw_port)
    , socket_fd_(-1)
    , state_(DoIPClientState::IDLE)
{
    std::cout << "[DoIP] Client initialized for ZGW: " << zgw_ip_ 
              << ":" << zgw_port_ << std::endl;
}

DoIPClient::~DoIPClient()
{
    disconnect();
}

/*******************************************************************************
 * Connection Management (parallel with ZGW DoIP_Client_Init/Close)
 ******************************************************************************/

bool DoIPClient::connect()
{
    if (state_ == DoIPClientState::ACTIVE) {
        std::cout << "[DoIP] Already connected and active" << std::endl;
        return true;
    }
    
    // Clean up existing connection
    disconnect();
    
    // Create TCP socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[DoIP] Failed to create socket" << std::endl;
        state_ = DoIPClientState::ERROR;
        return false;
    }
    
    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = DOIP_TIMEOUT_CONNECTION / 1000;
    tv.tv_usec = (DOIP_TIMEOUT_CONNECTION % 1000) * 1000;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    // Connect to ZGW
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(zgw_port_);
    
    if (inet_pton(AF_INET, zgw_ip_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[DoIP] Invalid ZGW IP address: " << zgw_ip_ << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        state_ = DoIPClientState::ERROR;
        return false;
    }
    
    std::cout << "[DoIP] Connecting to ZGW..." << std::endl;
    state_ = DoIPClientState::CONNECTING;
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[DoIP] Connection failed: " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        state_ = DoIPClientState::ERROR;
        return false;
    }
    
    std::cout << "[DoIP] TCP connected" << std::endl;
    state_ = DoIPClientState::CONNECTED;
    
    // Send Routing Activation Request
    if (!activateRouting()) {
        std::cerr << "[DoIP] Routing activation failed" << std::endl;
        disconnect();
        return false;
    }
    
    std::cout << "[DoIP] Routing activated - ACTIVE" << std::endl;
    state_ = DoIPClientState::ACTIVE;
    return true;
}

void DoIPClient::disconnect()
{
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        std::cout << "[DoIP] Disconnected" << std::endl;
    }
    state_ = DoIPClientState::IDLE;
}

bool DoIPClient::isActive() const
{
    return state_ == DoIPClientState::ACTIVE;
}

DoIPClientState DoIPClient::getState() const
{
    return state_;
}

/*******************************************************************************
 * DoIP Low-Level Functions (parallel with ZGW doip_message.c)
 ******************************************************************************/

bool DoIPClient::activateRouting()
{
    // Build Routing Activation Request (0x0005)
    // Payload: SA(2) + ActivationType(1) + Reserved(4)
    std::vector<uint8_t> payload(7);
    
    // Source Address (VMG = 0x0200)
    payload[0] = (DOIP_VMG_ADDRESS >> 8) & 0xFF;
    payload[1] = DOIP_VMG_ADDRESS & 0xFF;
    
    // Activation Type (0x00 = default)
    payload[2] = 0x00;
    
    // Reserved (4 bytes, all 0x00)
    payload[3] = 0x00;
    payload[4] = 0x00;
    payload[5] = 0x00;
    payload[6] = 0x00;
    
    std::vector<uint8_t> request = buildDoIPMessage(
        DoIPPayloadType::ROUTING_ACTIVATION_REQUEST, 
        payload
    );
    
    std::cout << "[DoIP] TX: Routing Activation Request" << std::endl;
    
    if (!sendRaw(request)) {
        return false;
    }
    
    // Receive Routing Activation Response (0x0006)
    std::vector<uint8_t> response = receiveRaw(DOIP_TIMEOUT_ROUTING);
    if (response.empty()) {
        std::cerr << "[DoIP] No routing activation response" << std::endl;
        return false;
    }
    
    DoIPPayloadType payload_type;
    std::vector<uint8_t> response_payload;
    
    if (!parseDoIPMessage(response, payload_type, response_payload)) {
        std::cerr << "[DoIP] Invalid routing activation response" << std::endl;
        return false;
    }
    
    if (payload_type != DoIPPayloadType::ROUTING_ACTIVATION_RESPONSE) {
        std::cerr << "[DoIP] Unexpected response type: 0x" 
                  << std::hex << static_cast<int>(payload_type) << std::endl;
        return false;
    }
    
    // Parse response: SA(2) + TA(2) + ResponseCode(1) + Reserved(4)
    if (response_payload.size() < 9) {
        std::cerr << "[DoIP] Invalid routing activation response size" << std::endl;
        return false;
    }
    
    uint8_t response_code = response_payload[4];
    
    if (response_code == 0x10) {  // Success
        std::cout << "[DoIP] RX: Routing Activation Response - SUCCESS (0x10)" << std::endl;
        return true;
    } else {
        std::cerr << "[DoIP] RX: Routing Activation Response - FAILED (0x" 
                  << std::hex << static_cast<int>(response_code) << ")" << std::endl;
        return false;
    }
}

std::vector<uint8_t> DoIPClient::buildDoIPMessage(DoIPPayloadType payload_type,
                                                    const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> message(DOIP_HEADER_SIZE + payload.size());
    
    // DoIP Header (8 bytes)
    message[0] = DOIP_PROTOCOL_VERSION;        // 0x02
    message[1] = DOIP_INVERSE_VERSION;         // 0xFD
    
    uint16_t type = static_cast<uint16_t>(payload_type);
    message[2] = (type >> 8) & 0xFF;           // Payload type (big-endian)
    message[3] = type & 0xFF;
    
    uint32_t length = static_cast<uint32_t>(payload.size());
    message[4] = (length >> 24) & 0xFF;        // Payload length (big-endian)
    message[5] = (length >> 16) & 0xFF;
    message[6] = (length >> 8) & 0xFF;
    message[7] = length & 0xFF;
    
    // Copy payload
    if (!payload.empty()) {
        memcpy(&message[DOIP_HEADER_SIZE], payload.data(), payload.size());
    }
    
    return message;
}

bool DoIPClient::parseDoIPMessage(const std::vector<uint8_t>& response,
                                   DoIPPayloadType& payload_type,
                                   std::vector<uint8_t>& payload)
{
    if (response.size() < DOIP_HEADER_SIZE) {
        return false;
    }
    
    // Validate DoIP header
    if (response[0] != DOIP_PROTOCOL_VERSION || 
        response[1] != DOIP_INVERSE_VERSION) {
        return false;
    }
    
    // Extract payload type (big-endian)
    uint16_t type = (static_cast<uint16_t>(response[2]) << 8) | response[3];
    payload_type = static_cast<DoIPPayloadType>(type);
    
    // Extract payload length (big-endian)
    uint32_t length = (static_cast<uint32_t>(response[4]) << 24) |
                      (static_cast<uint32_t>(response[5]) << 16) |
                      (static_cast<uint32_t>(response[6]) << 8) |
                      static_cast<uint32_t>(response[7]);
    
    // Validate total length
    if (response.size() < DOIP_HEADER_SIZE + length) {
        return false;
    }
    
    // Extract payload
    payload.clear();
    if (length > 0) {
        payload.assign(response.begin() + DOIP_HEADER_SIZE,
                      response.begin() + DOIP_HEADER_SIZE + length);
    }
    
    return true;
}

/*******************************************************************************
 * UDS Low-Level Functions (parallel with ZGW uds_handler.c)
 ******************************************************************************/

std::vector<uint8_t> DoIPClient::sendDiagnosticMessage(uint8_t service_id,
                                                         const std::vector<uint8_t>& data)
{
    if (!isActive()) {
        std::cerr << "[DoIP] Not active - cannot send diagnostic message" << std::endl;
        return {};
    }
    
    // Build DoIP Diagnostic Message (0x8001)
    // Payload: SA(2) + TA(2) + UDS_Data
    std::vector<uint8_t> payload(4 + 1 + data.size());
    
    // Source Address (VMG = 0x0200)
    payload[0] = (DOIP_VMG_ADDRESS >> 8) & 0xFF;
    payload[1] = DOIP_VMG_ADDRESS & 0xFF;
    
    // Target Address (ZGW = 0x0100)
    payload[2] = (DOIP_ZGW_ADDRESS >> 8) & 0xFF;
    payload[3] = DOIP_ZGW_ADDRESS & 0xFF;
    
    // UDS Service ID
    payload[4] = service_id;
    
    // UDS Data
    if (!data.empty()) {
        memcpy(&payload[5], data.data(), data.size());
    }
    
    std::vector<uint8_t> request = buildDoIPMessage(
        DoIPPayloadType::DIAGNOSTIC_MESSAGE,
        payload
    );
    
    std::cout << "[DoIP] TX: Diagnostic Message (SID=0x" 
              << std::hex << static_cast<int>(service_id) << ")" << std::endl;
    
    if (!sendRaw(request)) {
        return {};
    }
    
    // Receive Diagnostic Response (0x8001)
    std::vector<uint8_t> response = receiveRaw(DOIP_TIMEOUT_DIAGNOSTIC);
    if (response.empty()) {
        std::cerr << "[DoIP] No diagnostic response" << std::endl;
        return {};
    }
    
    DoIPPayloadType payload_type;
    std::vector<uint8_t> response_payload;
    
    if (!parseDoIPMessage(response, payload_type, response_payload)) {
        std::cerr << "[DoIP] Invalid diagnostic response" << std::endl;
        return {};
    }
    
    if (payload_type != DoIPPayloadType::DIAGNOSTIC_MESSAGE) {
        std::cerr << "[DoIP] Unexpected response type: 0x" 
                  << std::hex << static_cast<int>(payload_type) << std::endl;
        return {};
    }
    
    // Extract UDS data (skip SA(2) + TA(2))
    if (response_payload.size() < 5) {
        std::cerr << "[DoIP] Invalid diagnostic response size" << std::endl;
        return {};
    }
    
    std::vector<uint8_t> uds_response(response_payload.begin() + 4, 
                                      response_payload.end());
    
    std::cout << "[DoIP] RX: Diagnostic Response (" << uds_response.size() 
              << " bytes)" << std::endl;
    
    return uds_response;
}

std::vector<uint8_t> DoIPClient::sendRoutineControl(uint16_t routine_id, 
                                                      uint8_t subfunction)
{
    // UDS Routine Control (0x31)
    // Format: SID(1) + SubFunction(1) + RID(2)
    std::vector<uint8_t> data(3);
    data[0] = subfunction;                      // 0x01 = Start Routine
    data[1] = (routine_id >> 8) & 0xFF;         // RID high byte
    data[2] = routine_id & 0xFF;                // RID low byte
    
    return sendDiagnosticMessage(
        static_cast<uint8_t>(UDSService::ROUTINE_CONTROL),
        data
    );
}

/*******************************************************************************
 * UDS Routine Control Commands (parallel with vmg_server.py)
 ******************************************************************************/

bool DoIPClient::requestVCICollection()
{
    std::cout << "[DoIP] Requesting VCI Collection (RID=0xF001)..." << std::endl;
    
    std::vector<uint8_t> response = sendRoutineControl(RID_VCI_COLLECTION_START, 0x01);
    
    if (response.empty()) {
        std::cerr << "[DoIP] VCI Collection request failed" << std::endl;
        return false;
    }
    
    // Check positive response (0x71 = 0x31 + 0x40)
    if (response[0] == 0x71 && response.size() >= 4) {
        uint8_t status = response[4];  // Status byte after SID + SubFunc + RID
        if (status == 0x00) {
            std::cout << "[DoIP] VCI Collection started (Status=0x00)" << std::endl;
            return true;
        }
    }
    
    std::cerr << "[DoIP] VCI Collection negative response" << std::endl;
    return false;
}

bool DoIPClient::requestVCIReport(std::vector<VCIInfo>& vci_list)
{
    std::cout << "[DoIP] Requesting VCI Report (RID=0xF002)..." << std::endl;
    
    std::vector<uint8_t> response = sendRoutineControl(RID_VCI_SEND_REPORT, 0x01);
    
    if (response.empty()) {
        std::cerr << "[DoIP] VCI Report request failed" << std::endl;
        return false;
    }
    
    // Check positive response (0x71)
    if (response[0] != 0x71 || response.size() < 5) {
        std::cerr << "[DoIP] VCI Report negative response" << std::endl;
        return false;
    }
    
    uint8_t ecu_count = response[5];  // ECU count after SID + SubFunc + RID + Status
    std::cout << "[DoIP] VCI Report: " << static_cast<int>(ecu_count) 
              << " ECUs" << std::endl;
    
    // Now wait for VCI_REPORT (0x9000) payload
    std::vector<uint8_t> vci_report_msg = receiveRaw(DOIP_TIMEOUT_DIAGNOSTIC);
    
    if (vci_report_msg.empty()) {
        std::cerr << "[DoIP] VCI Report (0x9000) not received" << std::endl;
        return false;
    }
    
    DoIPPayloadType payload_type;
    std::vector<uint8_t> vci_payload;
    
    if (!parseDoIPMessage(vci_report_msg, payload_type, vci_payload)) {
        std::cerr << "[DoIP] Invalid VCI Report message" << std::endl;
        return false;
    }
    
    if (payload_type != DoIPPayloadType::VCI_REPORT) {
        std::cerr << "[DoIP] Expected VCI_REPORT (0x9000), got 0x" 
                  << std::hex << static_cast<int>(payload_type) << std::endl;
        return false;
    }
    
    // Parse VCI data: ECU_Count(1) + VCIInfo[N] (48 bytes each)
    if (vci_payload.empty()) {
        std::cerr << "[DoIP] Empty VCI payload" << std::endl;
        return false;
    }
    
    uint8_t count = vci_payload[0];
    size_t expected_size = 1 + count * sizeof(VCIInfo);
    
    if (vci_payload.size() < expected_size) {
        std::cerr << "[DoIP] Incomplete VCI data" << std::endl;
        return false;
    }
    
    vci_list.clear();
    size_t offset = 1;
    
    for (uint8_t i = 0; i < count; i++) {
        VCIInfo vci;
        memcpy(&vci, &vci_payload[offset], sizeof(VCIInfo));
        vci_list.push_back(vci);
        offset += sizeof(VCIInfo);
        
        std::cout << "  [" << static_cast<int>(i+1) << "] ECU: " 
                  << std::string(vci.ecu_id, 16) << ", SW: " 
                  << std::string(vci.sw_version, 8) << std::endl;
    }
    
    std::cout << "[DoIP] VCI Report received successfully" << std::endl;
    return true;
}

bool DoIPClient::requestReadinessCheck()
{
    std::cout << "[DoIP] Requesting Readiness Check (RID=0xF003)..." << std::endl;
    
    std::vector<uint8_t> response = sendRoutineControl(RID_READINESS_CHECK, 0x01);
    
    if (response.empty()) {
        std::cerr << "[DoIP] Readiness Check request failed" << std::endl;
        return false;
    }
    
    // Check positive response (0x71)
    if (response[0] == 0x71 && response.size() >= 4) {
        uint8_t status = response[4];
        if (status == 0x00) {
            std::cout << "[DoIP] Readiness Check started (Status=0x00)" << std::endl;
            return true;
        }
    }
    
    std::cerr << "[DoIP] Readiness Check negative response" << std::endl;
    return false;
}

bool DoIPClient::requestReadinessReport(std::vector<ReadinessInfo>& readiness_list)
{
    std::cout << "[DoIP] Requesting Readiness Report (RID=0xF004)..." << std::endl;
    
    std::vector<uint8_t> response = sendRoutineControl(RID_READINESS_SEND_REPORT, 0x01);
    
    if (response.empty()) {
        std::cerr << "[DoIP] Readiness Report request failed" << std::endl;
        return false;
    }
    
    // Check positive response (0x71)
    if (response[0] != 0x71 || response.size() < 5) {
        std::cerr << "[DoIP] Readiness Report negative response" << std::endl;
        return false;
    }
    
    uint8_t ecu_count = response[5];
    std::cout << "[DoIP] Readiness Report: " << static_cast<int>(ecu_count) 
              << " ECUs" << std::endl;
    
    // Wait for READINESS_REPORT (0x9001) payload
    std::vector<uint8_t> readiness_msg = receiveRaw(DOIP_TIMEOUT_DIAGNOSTIC);
    
    if (readiness_msg.empty()) {
        std::cerr << "[DoIP] Readiness Report (0x9001) not received" << std::endl;
        return false;
    }
    
    DoIPPayloadType payload_type;
    std::vector<uint8_t> readiness_payload;
    
    if (!parseDoIPMessage(readiness_msg, payload_type, readiness_payload)) {
        std::cerr << "[DoIP] Invalid Readiness Report message" << std::endl;
        return false;
    }
    
    if (payload_type != DoIPPayloadType::READINESS_REPORT) {
        std::cerr << "[DoIP] Expected READINESS_REPORT (0x9001), got 0x" 
                  << std::hex << static_cast<int>(payload_type) << std::endl;
        return false;
    }
    
    // Parse Readiness data: ECU_Count(1) + ReadinessInfo[N] (27 bytes each)
    if (readiness_payload.empty()) {
        std::cerr << "[DoIP] Empty Readiness payload" << std::endl;
        return false;
    }
    
    uint8_t count = readiness_payload[0];
    size_t expected_size = 1 + count * sizeof(ReadinessInfo);
    
    if (readiness_payload.size() < expected_size) {
        std::cerr << "[DoIP] Incomplete Readiness data" << std::endl;
        return false;
    }
    
    readiness_list.clear();
    size_t offset = 1;
    
    for (uint8_t i = 0; i < count; i++) {
        ReadinessInfo info;
        memcpy(&info, &readiness_payload[offset], sizeof(ReadinessInfo));
        readiness_list.push_back(info);
        offset += sizeof(ReadinessInfo);
        
        std::cout << "  [" << static_cast<int>(i+1) << "] ECU: " 
                  << std::string(info.ecu_id, 16) 
                  << ", Ready: " << (info.ready_for_update ? "YES" : "NO") 
                  << std::endl;
    }
    
    std::cout << "[DoIP] Readiness Report received successfully" << std::endl;
    return true;
}

/*******************************************************************************
 * OTA Firmware Transfer (UDS 0x34/36/37)
 ******************************************************************************/

bool DoIPClient::sendFirmware(const std::string& ecu_id, 
                               const std::vector<uint8_t>& firmware_data)
{
    std::cout << "[DoIP] Sending firmware to ECU: " << ecu_id 
              << " (" << firmware_data.size() << " bytes)" << std::endl;
    
    // TODO: Implement UDS 0x34 (Request Download) + 0x36 (Transfer Data) + 0x37 (Transfer Exit)
    // This requires proper memory address, block sequence counter, etc.
    
    std::cerr << "[DoIP] Firmware transfer not implemented yet" << std::endl;
    return false;
}

/*******************************************************************************
 * Socket Low-Level Functions
 ******************************************************************************/

bool DoIPClient::sendRaw(const std::vector<uint8_t>& data)
{
    if (socket_fd_ < 0 || data.empty()) {
        return false;
    }
    
    ssize_t sent = send(socket_fd_, data.data(), data.size(), 0);
    
    if (sent < 0) {
        std::cerr << "[DoIP] Send failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (static_cast<size_t>(sent) != data.size()) {
        std::cerr << "[DoIP] Incomplete send: " << sent << "/" << data.size() 
                  << " bytes" << std::endl;
        return false;
    }
    
    return true;
}

std::vector<uint8_t> DoIPClient::receiveRaw(int timeout_ms)
{
    if (socket_fd_ < 0) {
        return {};
    }
    
    // Use poll() for timeout
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;
    
    int ret = poll(&pfd, 1, timeout_ms);
    
    if (ret < 0) {
        std::cerr << "[DoIP] Poll error: " << strerror(errno) << std::endl;
        return {};
    }
    
    if (ret == 0) {
        std::cerr << "[DoIP] Receive timeout (" << timeout_ms << "ms)" << std::endl;
        return {};
    }
    
    // First, receive DoIP header (8 bytes)
    std::vector<uint8_t> header(DOIP_HEADER_SIZE);
    if (!receiveExact(header, DOIP_HEADER_SIZE, timeout_ms)) {
        return {};
    }
    
    // Parse payload length
    uint32_t payload_length = (static_cast<uint32_t>(header[4]) << 24) |
                              (static_cast<uint32_t>(header[5]) << 16) |
                              (static_cast<uint32_t>(header[6]) << 8) |
                              static_cast<uint32_t>(header[7]);
    
    // Allocate full message buffer
    std::vector<uint8_t> message(DOIP_HEADER_SIZE + payload_length);
    memcpy(message.data(), header.data(), DOIP_HEADER_SIZE);
    
    // Receive payload
    if (payload_length > 0) {
        std::vector<uint8_t> payload(payload_length);
        if (!receiveExact(payload, payload_length, timeout_ms)) {
            return {};
        }
        memcpy(&message[DOIP_HEADER_SIZE], payload.data(), payload_length);
    }
    
    return message;
}

bool DoIPClient::receiveExact(std::vector<uint8_t>& buffer, size_t size, int timeout_ms)
{
    if (socket_fd_ < 0 || size == 0) {
        return false;
    }
    
    buffer.resize(size);
    size_t received = 0;
    
    while (received < size) {
        // Use poll() for timeout
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, timeout_ms);
        
        if (ret <= 0) {
            return false;
        }
        
        ssize_t n = recv(socket_fd_, &buffer[received], size - received, 0);
        
        if (n < 0) {
            std::cerr << "[DoIP] Receive error: " << strerror(errno) << std::endl;
            return false;
        }
        
        if (n == 0) {
            std::cerr << "[DoIP] Connection closed by peer" << std::endl;
            return false;
        }
        
        received += n;
    }
    
    return true;
}

