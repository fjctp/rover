// XboxBLEController.cpp
#include "XboxBLEController.h"

XboxBLEController::XboxBLEController() : initialized(false) {
    resetState();
}

XboxBLEController::~XboxBLEController() {
    if (isConnected()) {
        disconnect();
    }
}

bool XboxBLEController::begin() {
    if (!BLE.begin()) {
        return false;
    }
    initialized = true;
    resetState();
    return true;
}

bool XboxBLEController::scanAndConnect(uint32_t scanTimeMs) {
    if (!initialized) {
        return false;
    }

    // Start scanning
    BLE.scan();
    
    uint32_t startTime = millis();
    
    // Scan for first Xbox controller found
    while (millis() - startTime < scanTimeMs) {
        BLEDevice device = BLE.available();
        
        if (device) {
            // Check if this is an Xbox controller
            if (isXboxController(device)) {
                // Found an Xbox controller - stop scanning and connect immediately
                BLE.stopScan();
                
                // Attempt to connect
                if (device.connect()) {
                    peripheral = device;
                    
                    // Discover attributes
                    if (peripheral.discoverAttributes()) {
                        // Find the correct HID report characteristic for input
                        if (findInputReportCharacteristic()) {
                            // Subscribe to notifications using the handle
                            if (reportCharacteristic.canNotify()) {
                                reportCharacteristic.subscribe();
                            }
                            
                            state.connected = true;
                            state.lastUpdateTime = millis();
                            return true;
                        }
                    }
                    
                    // If we got here, connection succeeded but service discovery failed
                    peripheral.disconnect();
                }
                
                // Connection failed, continue scanning for another controller
                BLE.scan();
            }
        }
        
        delay(10);
    }
    
    BLE.stopScan();
    return false;
}

bool XboxBLEController::findInputReportCharacteristic() {
    // Find the HID service
    BLEService hidService = peripheral.service(XBOX_SERVICE_UUID);
    
    if (!hidService) {
        return false;
    }
    
    // Get all characteristics in the HID service
    int charCount = hidService.characteristicCount();
    
    // Look for HID Report characteristics with NOTIFY property
    // The input report is the one with NOTIFY capability
    // Note: CurieBLE doesn't expose handles, so we iterate and take the first
    // notifiable HID Report characteristic (which is typically the input report)
    
    for (int i = 0; i < charCount; i++) {
        BLECharacteristic characteristic = hidService.characteristic(i);
        
        // Check if this is a HID Report characteristic (UUID 0x2A4D)
        if (characteristic.uuid() == XBOX_REPORT_UUID) {
            // Input reports have NOTIFY property
            if (characteristic.canNotify()) {
                // Found the input report characteristic
                reportCharacteristic = characteristic;
                return true;
            }
        }
    }
    
    return false;
}

bool XboxBLEController::update() {
    if (!state.connected || !peripheral.connected()) {
        state.connected = false;
        return false;
    }

    // Check if there's new data available
    if (reportCharacteristic.valueUpdated()) {
        const uint8_t* data = reportCharacteristic.value();
        uint16_t length = reportCharacteristic.valueLength();
        
        parseReport(data, length);
        state.lastUpdateTime = millis();
        return true;
    }
    
    return false;
}

void XboxBLEController::disconnect() {
    if (peripheral) {
        peripheral.disconnect();
    }
    resetState();
}

float XboxBLEController::getLeftStickXNormalized() const {
    return state.leftStickX / 32768.0f;
}

float XboxBLEController::getLeftStickYNormalized() const {
    return state.leftStickY / 32768.0f;
}

float XboxBLEController::getLeftTriggerNormalized() const {
    return state.leftTrigger / 255.0f;
}

float XboxBLEController::getRightTriggerNormalized() const {
    return state.rightTrigger / 255.0f;
}

bool XboxBLEController::isXboxController(BLEDevice& device) {
    // Check for Xbox controller by local name
    if (device.hasLocalName()) {
        String name = device.localName();
        name.toLowerCase();
        
        if (name.indexOf("xbox") >= 0 || 
            name.indexOf("controller") >= 0) {
            return true;
        }
    }
    
    // Check for HID service UUID (common for game controllers)
    if (device.hasAdvertisedServiceUuid()) {
        String serviceUuid = device.advertisedServiceUuid();
        serviceUuid.toLowerCase();
        
        if (serviceUuid.indexOf("1812") >= 0) { // HID Service
            return true;
        }
    }
    
    return false;
}

void XboxBLEController::parseReport(const uint8_t* data, uint16_t length) {
    // Xbox One controller HID report format (simplified)
    // Actual format may vary, this is a common structure:
    // Byte 0-1: Left stick X (little endian)
    // Byte 2-3: Left stick Y (little endian)
    // Byte 4-5: Right stick X (little endian)
    // Byte 6-7: Right stick Y (little endian)
    // Byte 8: Left trigger
    // Byte 9: Right trigger
    // Additional bytes for buttons, etc.
    
    if (length >= 10) {
        // Parse left stick X (bytes 0-1, little endian)
        state.leftStickX = (int16_t)(data[0] | (data[1] << 8));
        
        // Parse left stick Y (bytes 2-3, little endian)
        state.leftStickY = (int16_t)(data[2] | (data[3] << 8));
        
        // Parse triggers
        state.leftTrigger = data[8];
        state.rightTrigger = data[9];
    }
}

void XboxBLEController::resetState() {
    state.leftStickX = 0;
    state.leftStickY = 0;
    state.leftTrigger = 0;
    state.rightTrigger = 0;
    state.connected = false;
    state.lastUpdateTime = 0;
}
