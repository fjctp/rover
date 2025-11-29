#ifndef XBOX_BLE_CONTROLLER_H
#define XBOX_BLE_CONTROLLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <algorithm>
#include <map>

// Xbox Controller BLE Service UUIDs (standard for Xbox One S/X/Series controllers)
#define XBOX_SERVICE_UUID "00001812-0000-1000-8000-00805f9b34fb" // HID Service
#define XBOX_REPORT_UUID "00002a4d-0000-1000-8000-00805f9b34fb"  // HID Report
#define XBOX_REPORT_MAP_UUID "00002a4b-0000-1000-8000-00805f9b34fb" // HID Report Map

// Simple security callbacks implementation
class XboxSecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() {
        Serial.println("PassKeyRequest");
        return 123456;
    }
    
    void onPassKeyNotify(uint32_t pass_key) {
        Serial.printf("PassKeyNotify: %d\n", pass_key);
    }
    
    bool onConfirmPIN(uint32_t pass_key) {
        Serial.printf("ConfirmPIN: %d\n", pass_key);
        return true;
    }
    
    bool onSecurityRequest() {
        Serial.println("SecurityRequest");
        return true;
    }
    
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
        if (cmpl.success) {
            Serial.println("Pairing success!");
        } else {
            Serial.println("Pairing failed!");
        }
    }
};

class XboxBLEController {
public:
    struct ControllerState {
        int16_t leftStickX;      // -32768 to 32767 (left to right)
        int16_t leftStickY;      // -32768 to 32767 (up to down)
        uint8_t leftTrigger;     // 0 to 255
        uint8_t rightTrigger;    // 0 to 255
        bool connected;
        uint32_t lastUpdateTime; // millis() timestamp
    };

    XboxBLEController();
    ~XboxBLEController();

    // Initialize BLE
    bool begin();

    // Scan for Xbox controllers and connect to the first one found
    bool scanAndConnect(uint32_t scanTimeMs = 5000);

    // Update controller state (call in loop)
    bool update();

    // Disconnect from controller
    void disconnect();

    // Get current controller state
    ControllerState getState() const { return state; }

    // Check if connected
    bool isConnected() const { return state.connected; }

    // Get normalized values for robot control (-1.0 to 1.0)
    float getLeftStickXNormalized() const;
    float getLeftStickYNormalized() const;
    float getLeftTriggerNormalized() const;  // 0.0 to 1.0
    float getRightTriggerNormalized() const; // 0.0 to 1.0

    // For testing purposes
    void setStateForTesting(const ControllerState& testState) { state = testState; }

private:
    BLEClient* pClient;
    BLERemoteCharacteristic* pInputReportCharacteristic;
    BLEAddress* pServerAddress;
    ControllerState state;
    bool initialized;

    // Static map to track characteristic -> controller instance mappings
    static std::map<BLERemoteCharacteristic*, XboxBLEController*> instanceMap;

    // Helper functions
    bool isXboxController(BLEAdvertisedDevice* device);
    bool connectToController(BLEAddress address);
    bool findInputReportCharacteristic();
    void parseReport(const uint8_t* data, uint16_t length);
    void resetState();
    
    // Static callback for notifications
    static void notificationCallback(
        BLERemoteCharacteristic* pCharacteristic,
        uint8_t* pData,
        size_t length,
        bool isNotify);
};

#endif // XBOX_BLE_CONTROLLER_H
