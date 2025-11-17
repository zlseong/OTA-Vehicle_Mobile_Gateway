/**
 * @file vehicle_state.cpp
 * @brief Vehicle State Management Implementation
 */

#include "vehicle_state.hpp"
#include <iostream>

VehicleStateManager::VehicleStateManager()
    : current_state_(VehicleState::PARKED_IGNITION_OFF),
      previous_state_(VehicleState::UNKNOWN),
      state_changed_(false) {
}

void VehicleStateManager::updateState(VehicleState new_state) {
    if (current_state_ != new_state) {
        previous_state_ = current_state_;
        current_state_ = new_state;
        state_changed_ = true;
        
        std::cout << "[VEHICLE] State changed: " 
                  << getStateString() << std::endl;
    }
}

std::string VehicleStateManager::getStateString() const {
    switch (current_state_) {
        case VehicleState::DRIVING:
            return "DRIVING";
        case VehicleState::PARKED_IGNITION_ON:
            return "PARKED_IGNITION_ON";
        case VehicleState::PARKED_IGNITION_OFF:
            return "PARKED_IGNITION_OFF";
        case VehicleState::CHARGING:
            return "CHARGING";
        case VehicleState::OTA_ACTIVE:
            return "OTA_ACTIVE";
        default:
            return "UNKNOWN";
    }
}

bool VehicleStateManager::hasStateChanged() {
    if (state_changed_) {
        state_changed_ = false;
        return true;
    }
    return false;
}

void VehicleStateManager::detectState() {
    // TODO: Implement actual state detection using DoIP/UDS
    // For now, assume PARKED_IGNITION_OFF
    // In production:
    // 1. Query ZGW for ignition status (UDS DID)
    // 2. Query vehicle speed (CAN signal)
    // 3. Query charging status (for EV/PHEV)
    // 4. Determine state based on these parameters
    
    // Mock logic for demonstration
    VehicleState detected = VehicleState::PARKED_IGNITION_OFF;
    updateState(detected);
}

