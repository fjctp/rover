// XboxBLEController.h
#ifndef XBOX_BLE_CONTROLLER_H
#define XBOX_BLE_CONTROLLER_H

#include <CurieBLE.h>

// Xbox Controller BLE Service UUIDs (these are standard for Xbox One S/X controllers)
#define XBOX_SERVICE_UUID "00001812-0000-1000-8000-00805f9b34fb" // HID Service
#define XBOX_REPORT_UUID "00002a4d-0000-1000-8000-00805f9b34fb"  // HID Report (Handle 29)

// Note: Multiple HID Report characteristics may exist with the same UUID.
// We identify the input report by looking for the first one with NOTIFY property.
// This works because input reports always support notifications in the HID profile.

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

    // Initialize BLE and start scanning
    bool begin();

    // Scan for Xbox controllers and connect to the strongest signal
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
    BLEDevice peripheral;
    BLECharacteristic reportCharacteristic;
    ControllerState state;
    bool initialized;

    // Helper functions
    bool isXboxController(BLEDevice& device);
    bool findInputReportCharacteristic();
    void parseReport(const uint8_t* data, uint16_t length);
    void resetState();
};

#endif // XBOX_BLE_CONTROLLER_H
