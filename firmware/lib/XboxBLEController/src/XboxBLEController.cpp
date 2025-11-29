#include "XboxBLEController.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"

// Initialize static map
std::map<BLERemoteCharacteristic*, XboxBLEController*> XboxBLEController::instanceMap;

XboxBLEController::XboxBLEController() 
    : pClient(nullptr), 
      pInputReportCharacteristic(nullptr),
      pServerAddress(nullptr),
      initialized(false) {
    resetState();
}

XboxBLEController::~XboxBLEController() {
    if (isConnected()) {
        disconnect();
    }
    if (pServerAddress) {
        delete pServerAddress;
    }
    // Clean up instance map entry
    if (pInputReportCharacteristic) {
        instanceMap.erase(pInputReportCharacteristic);
    }
}

bool XboxBLEController::begin() {
    BLEDevice::init("ESP32_Controller_Client");
    
    // Enable bonding/pairing
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    
    initialized = true;
    resetState();
    return true;
}

bool XboxBLEController::scanAndConnect(uint32_t scanTimeMs) {
    if (!initialized) {
        return false;
    }

    // Create scanner
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // Start scanning
    BLEScanResults foundDevices = pBLEScan->start(scanTimeMs / 1000, false);
    
    // Look for Xbox controller in results
    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        
        if (isXboxController(&device)) {
            // Found Xbox controller - attempt to connect
            pBLEScan->stop();
            
            if (connectToController(device.getAddress())) {
                return true;
            }
        }
    }
    
    pBLEScan->clearResults();
    return false;
}

bool XboxBLEController::connectToController(BLEAddress address) {
    // Store address
    if (pServerAddress) {
        delete pServerAddress;
    }
    pServerAddress = new BLEAddress(address);
    
    // Create client
    if (pClient) {
        delete pClient;
    }
    pClient = BLEDevice::createClient();
    
    // Set security callbacks
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new XboxSecurityCallbacks());
    
    // Connect to the server
    if (!pClient->connect(*pServerAddress)) {
        return false;
    }
    
    Serial.println("Connected! Waiting for bonding...");
    
    // Wait a moment for bonding to complete
    delay(3000);
    
    // Check if we're actually bonded
    Serial.printf("Connection secured: %s\n", pClient->isConnected() ? "Yes" : "No");
    
    // Set MTU size (important for HID)
    pClient->setMTU(517);
    
    Serial.println("Discovering services...");
    
    // Find and subscribe to input report characteristic
    if (!findInputReportCharacteristic()) {
        Serial.println("Failed to find input report characteristic");
        pClient->disconnect();
        return false;
    }
    
    Serial.println("Enabling notifications...");
    
    // Register for notifications
    if (pInputReportCharacteristic->canNotify()) {
        // Register this instance in the map
        instanceMap[pInputReportCharacteristic] = this;
        
        // Register the static callback
        pInputReportCharacteristic->registerForNotify(notificationCallback);
        
        // Alternative: Try reading the value first to test connection
        try {
            std::string value = pInputReportCharacteristic->readValue();
            Serial.printf("Initial read successful, %d bytes\n", value.length());
        } catch (...) {
            Serial.println("Initial read failed (this may be normal)");
        }
        
        Serial.println("Subscribed to notifications!");
        Serial.println("Try pressing buttons on the controller...");
    } else {
        Serial.println("ERROR: Characteristic cannot notify!");
        pClient->disconnect();
        return false;
    }
    
    state.connected = true;
    state.lastUpdateTime = millis();
    return true;
}

bool XboxBLEController::findInputReportCharacteristic() {
    Serial.println("Looking for HID service...");
    
    // Get HID service
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(XBOX_SERVICE_UUID));
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find HID service!");
        return false;
    }
    
    Serial.println("HID service found! Looking for characteristics...");
    
    // Get all characteristics
    std::map<std::string, BLERemoteCharacteristic*>* pCharacteristics = 
        pRemoteService->getCharacteristics();
    
    Serial.printf("Found %d characteristics in HID service\n", pCharacteristics->size());
    
    // Store references to important characteristics
    BLERemoteCharacteristic* pHIDControlPoint = nullptr;
    BLERemoteCharacteristic* pProtocolMode = nullptr;
    BLERemoteCharacteristic* pHIDInfo = nullptr;
    BLERemoteCharacteristic* pReportMap = nullptr;
    
    // Look for all HID characteristics
    int reportCount = 0;
    
    for (auto& pair : *pCharacteristics) {
        BLERemoteCharacteristic* pChar = pair.second;
        
        Serial.printf("  Char UUID: %s, Properties: ", pChar->getUUID().toString().c_str());
        if (pChar->canRead()) Serial.print("READ ");
        if (pChar->canWrite()) Serial.print("WRITE ");
        if (pChar->canWriteNoResponse()) Serial.print("WRITE_NR ");
        if (pChar->canNotify()) Serial.print("NOTIFY ");
        if (pChar->canIndicate()) Serial.print("INDICATE ");
        Serial.println();
        
        std::string uuid = pChar->getUUID().toString();
        
        // 0x2A4E is Protocol Mode
        if (uuid == "00002a4e-0000-1000-8000-00805f9b34fb") {
            pProtocolMode = pChar;
            Serial.println("    -> Found Protocol Mode");
        }
        // 0x2A4A is HID Information
        else if (uuid == "00002a4a-0000-1000-8000-00805f9b34fb") {
            pHIDInfo = pChar;
            Serial.println("    -> Found HID Information");
        }
        // 0x2A4B is Report Map
        else if (uuid == "00002a4b-0000-1000-8000-00805f9b34fb") {
            pReportMap = pChar;
            Serial.println("    -> Found Report Map");
        }
        // 0x2A4C is HID Control Point
        else if (uuid == "00002a4c-0000-1000-8000-00805f9b34fb") {
            pHIDControlPoint = pChar;
            Serial.println("    -> Found HID Control Point");
        }
        // 0x2A4D is HID Report
        else if (pChar->getUUID().equals(BLEUUID(XBOX_REPORT_UUID))) {
            reportCount++;
            Serial.printf("    -> This is HID Report #%d\n", reportCount);
            
            if (pChar->canNotify()) {
                Serial.println("    -> Has NOTIFY - this is the input report!");
                pInputReportCharacteristic = pChar;
            }
        }
    }
    
    if (pInputReportCharacteristic == nullptr) {
        Serial.println("Failed to find notifiable HID Report characteristic!");
        return false;
    }
    
    // CRITICAL: Get the Client Characteristic Configuration Descriptor (CCCD)
    // and manually enable notifications - sometimes registerForNotify isn't enough
    Serial.println("Looking for CCCD descriptor on input report...");
    BLERemoteDescriptor* pCCCD = pInputReportCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (pCCCD != nullptr) {
        Serial.println("  Found CCCD! Enabling notifications manually...");
        uint8_t notificationOn[] = {0x01, 0x00}; // Enable notifications
        try {
            pCCCD->writeValue(notificationOn, 2, true);
            Serial.println("  CCCD written successfully");
        } catch (...) {
            Serial.println("  Failed to write CCCD");
        }
    } else {
        Serial.println("  WARNING: CCCD not found!");
    }
    
    // Read HID Information
    if (pHIDInfo != nullptr && pHIDInfo->canRead()) {
        Serial.println("Reading HID Information...");
        try {
            std::string hidInfo = pHIDInfo->readValue();
            Serial.printf("  HID Info: %d bytes - ", hidInfo.length());
            for (size_t i = 0; i < hidInfo.length(); i++) {
                Serial.printf("%02X ", (uint8_t)hidInfo[i]);
            }
            Serial.println();
        } catch (...) {
            Serial.println("  Failed to read HID Info");
        }
    }
    
    // Read Report Map to understand the report structure
    if (pReportMap != nullptr && pReportMap->canRead()) {
        Serial.println("Reading Report Map...");
        try {
            std::string reportMap = pReportMap->readValue();
            Serial.printf("  Report Map: %d bytes\n", reportMap.length());
            // Don't print the whole map, it's usually very long
        } catch (...) {
            Serial.println("  Failed to read Report Map");
        }
    }
    
    // Set Protocol Mode to Report Protocol (0x01)
    // This is crucial - Report Protocol enables input reports
    if (pProtocolMode != nullptr && pProtocolMode->canWrite()) {
        Serial.println("Setting Protocol Mode to Report Protocol...");
        uint8_t reportProtocol = 0x01; // 0=Boot Protocol, 1=Report Protocol
        try {
            pProtocolMode->writeValue(&reportProtocol, 1, true); // with response
            delay(200);
            
            // Verify it was set
            if (pProtocolMode->canRead()) {
                std::string mode = pProtocolMode->readValue();
                if (mode.length() > 0) {
                    Serial.printf("  Protocol Mode is now: 0x%02X ", (uint8_t)mode[0]);
                    Serial.println((uint8_t)mode[0] == 0x01 ? "(Report Protocol - Good!)" : "(Boot Protocol - Bad!)");
                }
            }
            Serial.println("  Protocol Mode set successfully");
        } catch (...) {
            Serial.println("  Failed to set Protocol Mode");
        }
    } else {
        Serial.println("WARNING: Protocol Mode characteristic not found or not writable!");
    }
    
    // Exit suspend mode
    if (pHIDControlPoint != nullptr && pHIDControlPoint->canWriteNoResponse()) {
        Serial.println("Sending exit suspend command...");
        uint8_t exitSuspend = 0x00;
        try {
            pHIDControlPoint->writeValue(&exitSuspend, 1, false); // without response
            delay(100);
            Serial.println("  Exit suspend sent");
        } catch (...) {
            Serial.println("  Failed to send exit suspend");
        }
    }
    
    return true;
}

bool XboxBLEController::update() {
    if (!state.connected || !pClient || !pClient->isConnected()) {
        state.connected = false;
        return false;
    }
    
    // Notifications are handled asynchronously via callback
    // This method just checks connection status
    return true;
}

void XboxBLEController::disconnect() {
    if (pClient && pClient->isConnected()) {
        pClient->disconnect();
    }
    // Clean up instance map entry
    if (pInputReportCharacteristic) {
        instanceMap.erase(pInputReportCharacteristic);
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

bool XboxBLEController::isXboxController(BLEAdvertisedDevice* device) {
    // Check for Xbox controller by name
    if (device->haveName()) {
        std::string name = device->getName();
        // Convert to lowercase for comparison
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        if (name.find("xbox") != std::string::npos || 
            name.find("controller") != std::string::npos) {
            return true;
        }
    }
    
    // Check for HID service UUID
    if (device->haveServiceUUID()) {
        if (device->isAdvertisingService(BLEUUID(XBOX_SERVICE_UUID))) {
            return true;
        }
    }
    
    return false;
}

void XboxBLEController::notificationCallback(
    BLERemoteCharacteristic* pCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {
    
    Serial.printf("Notification received! Length: %d bytes\n", length);
    Serial.print("Data: ");
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
    
    // Look up the controller instance from the map
    auto it = instanceMap.find(pCharacteristic);
    if (it != instanceMap.end()) {
        XboxBLEController* controller = it->second;
        controller->parseReport(pData, length);
        controller->state.lastUpdateTime = millis();
    } else {
        Serial.println("WARNING: No controller instance found for this characteristic!");
    }
}

void XboxBLEController::parseReport(const uint8_t* data, uint16_t length) {
    // Xbox One controller HID report format (simplified)
    // Actual format may vary by controller model
    // Byte 0-1: Left stick X (little endian)
    // Byte 2-3: Left stick Y (little endian)
    // Byte 4-5: Right stick X (little endian)
    // Byte 6-7: Right stick Y (little endian)
    // Byte 8: Left trigger
    // Byte 9: Right trigger
    
    Serial.printf("Parsing report: length=%d\n", length);
    
    if (length >= 10) {
        // Parse left stick X (bytes 0-1, little endian)
        state.leftStickX = (int16_t)(data[0] | (data[1] << 8));
        
        // Parse left stick Y (bytes 2-3, little endian)
        state.leftStickY = (int16_t)(data[2] | (data[3] << 8));
        
        // Parse triggers
        state.leftTrigger = data[8];
        state.rightTrigger = data[9];
        
        Serial.printf("  Left Stick: X=%d Y=%d, Triggers: L=%d R=%d\n",
                     state.leftStickX, state.leftStickY,
                     state.leftTrigger, state.rightTrigger);
    } else {
        Serial.printf("  Report too short (expected >= 10 bytes)\n");
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
