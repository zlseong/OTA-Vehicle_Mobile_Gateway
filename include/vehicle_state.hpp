/**
 * @file vehicle_state.hpp
 * @brief Vehicle State Management Module
 * 
 * Manages vehicle operational state for adaptive behavior
 */

#ifndef VEHICLE_STATE_HPP
#define VEHICLE_STATE_HPP

#include <string>

/**
 * @brief Vehicle operational states
 */
enum class VehicleState {
    DRIVING,              // 주행 중
    PARKED_IGNITION_ON,   // 시동 ON 주차
    PARKED_IGNITION_OFF,  // 시동 OFF 주차
    CHARGING,             // 충전 중 (EV/PHEV)
    OTA_ACTIVE,           // OTA 진행 중
    UNKNOWN               // 알 수 없음
};

/**
 * @brief Vehicle State Manager
 */
class VehicleStateManager {
public:
    VehicleStateManager();
    
    /**
     * @brief Get current vehicle state
     */
    VehicleState getCurrentState() const { return current_state_; }
    
    /**
     * @brief Update vehicle state
     * @param new_state New state to set
     */
    void updateState(VehicleState new_state);
    
    /**
     * @brief Get state as string
     */
    std::string getStateString() const;
    
    /**
     * @brief Check if state changed since last call
     */
    bool hasStateChanged();
    
    /**
     * @brief Detect vehicle state from ZGW data
     * TODO: Implement actual detection logic using DoIP/UDS
     */
    void detectState();

private:
    VehicleState current_state_;
    VehicleState previous_state_;
    bool state_changed_;
};

#endif // VEHICLE_STATE_HPP

