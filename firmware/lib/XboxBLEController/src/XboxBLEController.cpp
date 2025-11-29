#include "XboxBLEController.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"

#include "ArduinoUtils.h"

// Initialize static map
std::map<BLERemoteCharacteristic *, XboxBLEController *> XboxBLEController::instanceMap;

XboxBLEController::XboxBLEController()
    : pClient(nullptr),
      pInputReportCharacteristic(nullptr),
      pServerAddress(nullptr),
      initialized(false)
{
  resetState();
}

XboxBLEController::~XboxBLEController()
{
  if (isConnected())
  {
    disconnect();
  }
  if (pServerAddress)
  {
    delete pServerAddress;
  }
  // Clean up instance map entry
  if (pInputReportCharacteristic)
  {
    instanceMap.erase(pInputReportCharacteristic);
  }
}

bool XboxBLEController::begin()
{
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

bool XboxBLEController::scanAndConnect(uint32_t scanTimeMs)
{
  if (!initialized)
  {
    return false;
  }

  // Create scanner
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // Start scanning
  BLEScanResults foundDevices = pBLEScan->start(scanTimeMs / 1000, false);

  // Look for Xbox controller in results
  for (int i = 0; i < foundDevices.getCount(); i++)
  {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    if (isXboxController(&device))
    {
      // Found Xbox controller - attempt to connect
      pBLEScan->stop();

      if (connectToController(device.getAddress()))
      {
        return true;
      }
    }
  }

  pBLEScan->clearResults();
  return false;
}

bool XboxBLEController::connectToController(BLEAddress address)
{
  // Store address
  if (pServerAddress)
  {
    delete pServerAddress;
  }
  pServerAddress = new BLEAddress(address);

  // Create client
  if (pClient)
  {
    delete pClient;
  }
  pClient = BLEDevice::createClient();

  // Set security callbacks
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new XboxSecurityCallbacks());

  // Connect to the server
  if (!pClient->connect(*pServerAddress))
  {
    return false;
  }

  // Wait a moment for bonding to complete
  delay(3000);

  // Check if we're actually bonded
  if (pClient->isConnected())
    log(LogLevel::INFO, "Connected securely!");
  else
    log(LogLevel::INFO, "Connected!");

  // Set MTU size (important for HID)
  pClient->setMTU(517);

  // Find and subscribe to input report characteristic
  if (!findInputReportCharacteristic())
  {
    log(LogLevel::ERROR, "Failed to find input report characteristic");
    pClient->disconnect();
    return false;
  }

  // Register for notifications
  if (pInputReportCharacteristic->canNotify())
  {
    // Register this instance in the map
    instanceMap[pInputReportCharacteristic] = this;

    // Register the static callback
    pInputReportCharacteristic->registerForNotify(notificationCallback);

    // Alternative: Try reading the value first to test connection
    try
    {
      std::string value = pInputReportCharacteristic->readValue();
    }
    catch (...)
    {
      log(LogLevel::ERROR, "Initial read failed (this may be normal)");
    }

    log(LogLevel::INFO, "Subscribed to notifications!");
  }
  else
  {
    log(LogLevel::ERROR, "Characteristic cannot notify!");
    pClient->disconnect();
    return false;
  }

  state.connected = true;
  state.lastUpdateTime = millis();
  return true;
}

bool XboxBLEController::findInputReportCharacteristic()
{
  log(LogLevel::INFO, "Looking for HID service...");

  // Get HID service
  BLERemoteService *pRemoteService = pClient->getService(BLEUUID(XBOX_SERVICE_UUID));
  if (pRemoteService == nullptr)
  {
    log(LogLevel::ERROR, "Failed to find HID service!");
    return false;
  }

  // Get all characteristics
  std::map<std::string, BLERemoteCharacteristic *> *pCharacteristics =
      pRemoteService->getCharacteristics();

  // Store references to important characteristics
  BLERemoteCharacteristic *pHIDControlPoint = nullptr;
  BLERemoteCharacteristic *pProtocolMode = nullptr;
  BLERemoteCharacteristic *pHIDInfo = nullptr;
  BLERemoteCharacteristic *pReportMap = nullptr;

  // Look for all HID characteristics
  int reportCount = 0;

  for (auto &pair : *pCharacteristics)
  {
    BLERemoteCharacteristic *pChar = pair.second;
    std::string uuid = pChar->getUUID().toString();

    // 0x2A4E is Protocol Mode
    if (uuid == "00002a4e-0000-1000-8000-00805f9b34fb")
    {
      pProtocolMode = pChar;
    }
    // 0x2A4A is HID Information
    else if (uuid == "00002a4a-0000-1000-8000-00805f9b34fb")
    {
      pHIDInfo = pChar;
    }
    // 0x2A4B is Report Map
    else if (uuid == "00002a4b-0000-1000-8000-00805f9b34fb")
    {
      pReportMap = pChar;
    }
    // 0x2A4C is HID Control Point
    else if (uuid == "00002a4c-0000-1000-8000-00805f9b34fb")
    {
      pHIDControlPoint = pChar;
    }
    // 0x2A4D is HID Report
    else if (pChar->getUUID().equals(BLEUUID(XBOX_REPORT_UUID)))
    {
      reportCount++;

      if (pChar->canNotify())
      {

        pInputReportCharacteristic = pChar;
      }
    }
  }

  if (pInputReportCharacteristic == nullptr)
  {
    log(LogLevel::ERROR, "Failed to find notifiable HID Report characteristic!");
    return false;
  }

  // CRITICAL: Get the Client Characteristic Configuration Descriptor (CCCD)
  // and manually enable notifications - sometimes registerForNotify isn't enough
  BLERemoteDescriptor *pCCCD = pInputReportCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902));
  if (pCCCD != nullptr)
  {
    uint8_t notificationOn[] = {0x01, 0x00}; // Enable notifications
    try
    {
      pCCCD->writeValue(notificationOn, 2, true);
    }
    catch (...)
    {
      log(LogLevel::ERROR, "Failed to write CCCD");
    }
  }
  else
  {
    log(LogLevel::WARN, "CCCD not found!");
  }

  // Set Protocol Mode to Report Protocol (0x01)
  // This is crucial - Report Protocol enables input reports
  if (pProtocolMode != nullptr && pProtocolMode->canWrite())
  {
    uint8_t reportProtocol = 0x01; // 0=Boot Protocol, 1=Report Protocol
    try
    {
      pProtocolMode->writeValue(&reportProtocol, 1, true); // with response
      delay(200);

      // Verify it was set
      if (pProtocolMode->canRead())
      {
        std::string mode = pProtocolMode->readValue();
      }
      log(LogLevel::DEBUG, "Protocol Mode set successfully");
    }
    catch (...)
    {
      log(LogLevel::ERROR, "Failed to set Protocol Mode");
    }
  }
  else
  {
    log(LogLevel::WARN, "Protocol Mode characteristic not found or not writable!");
  }

  // Exit suspend mode
  if (pHIDControlPoint != nullptr && pHIDControlPoint->canWriteNoResponse())
  {
    log(LogLevel::DEBUG, "Sending exit suspend command...");
    uint8_t exitSuspend = 0x00;
    try
    {
      pHIDControlPoint->writeValue(&exitSuspend, 1, false); // without response
      delay(100);
      log(LogLevel::DEBUG, "Exit suspend sent");
    }
    catch (...)
    {
      log(LogLevel::ERROR, "Failed to send exit suspend");
    }
  }

  return true;
}

bool XboxBLEController::update()
{
  if (!state.connected || !pClient || !pClient->isConnected())
  {
    state.connected = false;
    return false;
  }

  // Notifications are handled asynchronously via callback
  // This method just checks connection status
  return true;
}

void XboxBLEController::disconnect()
{
  if (pClient && pClient->isConnected())
  {
    pClient->disconnect();
  }
  // Clean up instance map entry
  if (pInputReportCharacteristic)
  {
    instanceMap.erase(pInputReportCharacteristic);
  }
  resetState();
}

float XboxBLEController::getLeftStickXNormalized() const
{
  return state.leftStickX / 32768.0f;
}

float XboxBLEController::getLeftStickYNormalized() const
{
  return state.leftStickY / 32768.0f;
}

float XboxBLEController::getLeftTriggerNormalized() const
{
  return state.leftTrigger / 255.0f;
}

float XboxBLEController::getRightTriggerNormalized() const
{
  return state.rightTrigger / 255.0f;
}

bool XboxBLEController::isXboxController(BLEAdvertisedDevice *device)
{
  // Check for Xbox controller by name
  if (device->haveName())
  {
    std::string name = device->getName();
    // Convert to lowercase for comparison
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    if (name.find("xbox") != std::string::npos ||
        name.find("controller") != std::string::npos)
    {
      return true;
    }
  }

  // Check for HID service UUID
  if (device->haveServiceUUID())
  {
    if (device->isAdvertisingService(BLEUUID(XBOX_SERVICE_UUID)))
    {
      return true;
    }
  }

  return false;
}

void XboxBLEController::notificationCallback(
    BLERemoteCharacteristic *pCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
  // Look up the controller instance from the map
  auto it = instanceMap.find(pCharacteristic);
  if (it != instanceMap.end())
  {
    XboxBLEController *controller = it->second;
    controller->parseReport(pData, length);
    controller->state.lastUpdateTime = millis();
  }
}

void XboxBLEController::parseReport(const uint8_t *data, uint16_t length)
{
  // Xbox One controller HID report format (simplified)
  log(LogLevel::DEBUG, "Parsing report");

  if (length >= 10)
  {
    // Parse stick (little endian)
    state.leftStickX = (uint16_t)(data[0] | (data[1] << 8));
    state.leftStickY = (uint16_t)(data[2] | (data[3] << 8));
    // state.rightStickX = (uint16_t)(data[4] | (data[5] << 8));
    // state.rightStickY = (uint16_t)(data[6] | (data[7] << 8));

    // Parse triggers
    state.leftTrigger = (uint16_t)(data[8] | (data[9] << 8));
    state.rightTrigger = (uint16_t)(data[10] | (data[11] << 8));
    // state.dpad = data[12];
    // state.button1 = data[13];
    // state.button2 = data[14];

    char buffer[80];
    sprintf(buffer, "  Left Stick: X=%d Y=%d, Triggers: L=%d R=%d",
            state.leftStickX, state.leftStickY,
            state.leftTrigger, state.rightTrigger);
    log(LogLevel::INFO, buffer);
  }
}

void XboxBLEController::resetState()
{
  state.leftStickX = 0;
  state.leftStickY = 0;
  state.leftTrigger = 0;
  state.rightTrigger = 0;
  state.connected = false;
  state.lastUpdateTime = 0;
}
