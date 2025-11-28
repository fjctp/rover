#include <Arduino.h>

#include "ArduinoUtils.h"
#include "XboxBLEController.h"

const uint8_t MAIN_LOOP_HZ = 50;
const float MAIN_LOOP_MS = 1.0f / MAIN_LOOP_HZ * 1.0e3;
const uint32_t BAND_RATE = 115200;
const uint32_t BLE_SCAN_MS = 3 * 1e3;

XboxBLEController xbox;

void setup()
{
  Serial.begin(BAND_RATE);
  while (!Serial);

  Serial.println("Xbox BLE Controller - Robot Rover Control");
  Serial.println("=========================================");

  // Initialize BLE
  if (!xbox.begin())
  {
    Serial.println("Failed to initialize BLE!");
    sleep_forever();
  }

  // Scan and connect to first controller found
  if (xbox.scanAndConnect(BLE_SCAN_MS))
  {
    Serial.println("Connected to Xbox controller!");
  }
  else
  {
    Serial.println("No Xbox controller found. Make sure it's in pairing mode.");
    Serial.println("Press and hold the pairing button on the controller.");
    sleep_forever();
  }
}

void loop()
{
  if (xbox.isConnected())
  {
    // Update controller state
    if (xbox.update())
    {
      // Get normalized values for robot control
      float leftX = xbox.getLeftStickXNormalized();          // -1.0 to 1.0 (steering)
      float leftY = xbox.getLeftStickYNormalized();          // -1.0 to 1.0 (forward/back)
      float leftTrigger = xbox.getLeftTriggerNormalized();   // 0.0 to 1.0 (brake)
      float rightTrigger = xbox.getRightTriggerNormalized(); // 0.0 to 1.0 (throttle)

      // Example: Tank drive control
      float forward = -leftY; // Invert Y (up is positive)
      float turn = leftX;

      float leftMotor = forward + turn;
      float rightMotor = forward - turn;

      // Clamp to -1.0 to 1.0
      leftMotor = constrain(leftMotor, -1.0, 1.0);
      rightMotor = constrain(rightMotor, -1.0, 1.0);

      // Apply trigger modulation
      float throttle = rightTrigger - leftTrigger;
      leftMotor *= abs(throttle);
      rightMotor *= abs(throttle);

      Serial.print("Left: ");
      Serial.print(leftMotor, 2);
      Serial.print(" | Right: ");
      Serial.print(rightMotor, 2);
      Serial.print(" | LT: ");
      Serial.print(leftTrigger, 2);
      Serial.print(" | RT: ");
      Serial.println(rightTrigger, 2);
    }
  }
  else
  {
    Serial.println("Controller disconnected!");
    delay(1000);

    // Try to reconnect
    Serial.println("Attempting to reconnect...");
    xbox.scanAndConnect(5000);
  }

  delay(MAIN_LOOP_MS);
}
