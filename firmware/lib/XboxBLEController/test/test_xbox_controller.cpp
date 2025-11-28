#include <unity.h>
#include "XboxBLEController.h"

XboxBLEController* controller;

void setUp(void) {
    controller = new XboxBLEController();
}

void tearDown(void) {
    delete controller;
}

// Test initialization
void test_controller_initialization(void) {
    XboxBLEController::ControllerState state = controller->getState();
    
    TEST_ASSERT_EQUAL_INT16(0, state.leftStickX);
    TEST_ASSERT_EQUAL_INT16(0, state.leftStickY);
    TEST_ASSERT_EQUAL_UINT8(0, state.leftTrigger);
    TEST_ASSERT_EQUAL_UINT8(0, state.rightTrigger);
    TEST_ASSERT_FALSE(state.connected);
}

// Test normalized values at center
void test_normalized_values_center(void) {
    XboxBLEController::ControllerState testState = {0, 0, 0, 0, false, 0};
    controller->setStateForTesting(testState);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0f, controller->getLeftStickXNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0f, controller->getLeftStickYNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0f, controller->getLeftTriggerNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0f, controller->getRightTriggerNormalized());
}

// Test normalized values at maximum positive
void test_normalized_values_max_positive(void) {
    XboxBLEController::ControllerState testState = {32767, 32767, 255, 255, false, 0};
    controller->setStateForTesting(testState);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.9999f, controller->getLeftStickXNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.9999f, controller->getLeftStickYNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0f, controller->getLeftTriggerNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0f, controller->getRightTriggerNormalized());
}

// Test normalized values at maximum negative
void test_normalized_values_max_negative(void) {
    XboxBLEController::ControllerState testState = {-32768, -32768, 0, 0, false, 0};
    controller->setStateForTesting(testState);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001, -1.0f, controller->getLeftStickXNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, -1.0f, controller->getLeftStickYNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0f, controller->getLeftTriggerNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0f, controller->getRightTriggerNormalized());
}

// Test half stick values
void test_normalized_values_half(void) {
    XboxBLEController::ControllerState testState = {16384, 16384, 128, 128, false, 0};
    controller->setStateForTesting(testState);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.5f, controller->getLeftStickXNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.5f, controller->getLeftStickYNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.502f, controller->getLeftTriggerNormalized());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.502f, controller->getRightTriggerNormalized());
}

// Test connection state
void test_connection_state(void) {
    TEST_ASSERT_FALSE(controller->isConnected());
    
    XboxBLEController::ControllerState testState = {0, 0, 0, 0, true, 1000};
    controller->setStateForTesting(testState);
    
    TEST_ASSERT_TRUE(controller->isConnected());
}

// Test state retrieval
void test_get_state(void) {
    XboxBLEController::ControllerState testState = {1234, -5678, 100, 200, true, 5000};
    controller->setStateForTesting(testState);
    
    XboxBLEController::ControllerState retrieved = controller->getState();
    
    TEST_ASSERT_EQUAL_INT16(1234, retrieved.leftStickX);
    TEST_ASSERT_EQUAL_INT16(-5678, retrieved.leftStickY);
    TEST_ASSERT_EQUAL_UINT8(100, retrieved.leftTrigger);
    TEST_ASSERT_EQUAL_UINT8(200, retrieved.rightTrigger);
    TEST_ASSERT_TRUE(retrieved.connected);
    TEST_ASSERT_EQUAL_UINT32(5000, retrieved.lastUpdateTime);
}

// Test edge case: trigger overflow protection
void test_trigger_range_limits(void) {
    XboxBLEController::ControllerState testState = {0, 0, 255, 255, false, 0};
    controller->setStateForTesting(testState);
    
    float leftTrig = controller->getLeftTriggerNormalized();
    float rightTrig = controller->getRightTriggerNormalized();
    
    TEST_ASSERT_TRUE(leftTrig >= 0.0f && leftTrig <= 1.0f);
    TEST_ASSERT_TRUE(rightTrig >= 0.0f && rightTrig <= 1.0f);
}

// Test edge case: stick overflow protection
void test_stick_range_limits(void) {
    XboxBLEController::ControllerState testState = {32767, -32768, 0, 0, false, 0};
    controller->setStateForTesting(testState);
    
    float stickX = controller->getLeftStickXNormalized();
    float stickY = controller->getLeftStickYNormalized();
    
    TEST_ASSERT_TRUE(stickX >= -1.0f && stickX <= 1.0f);
    TEST_ASSERT_TRUE(stickY >= -1.0f && stickY <= 1.0f);
}

// Main test runner
void setup() {
    delay(2000); // Wait for serial connection
    
    UNITY_BEGIN();
    
    RUN_TEST(test_controller_initialization);
    RUN_TEST(test_normalized_values_center);
    RUN_TEST(test_normalized_values_max_positive);
    RUN_TEST(test_normalized_values_max_negative);
    RUN_TEST(test_normalized_values_half);
    RUN_TEST(test_connection_state);
    RUN_TEST(test_get_state);
    RUN_TEST(test_trigger_range_limits);
    RUN_TEST(test_stick_range_limits);
    
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}

// platformio.ini
/*
[env:genuino101]
platform = intel_arc32
board = genuino101
framework = arduino

; Library dependencies
lib_deps = 
    CurieBLE

; Test configuration
test_build_project_src = true

; Serial monitor configuration
monitor_speed = 9600

; Build flags
build_flags = 
    -DUNIT_TEST

; Test framework
test_framework = unity

[env:native]
platform = native
test_framework = unity
build_flags = 
    -DUNIT_TEST
    -std=c++11
*/

// Example usage in main.cpp
/*
#include <Arduino.h>
#include "XboxBLEController.h"

XboxBLEController xbox;

void setup() {
    Serial.begin(9600);
    while (!Serial);
    
    Serial.println("Xbox BLE Controller - Robot Rover Control");
    Serial.println("=========================================");
    
    // Initialize BLE
    if (!xbox.begin()) {
        Serial.println("Failed to initialize BLE!");
        while (1);
    }
    
    Serial.println("Scanning for Xbox controllers...");
    
    // Scan and connect to first controller found
    if (xbox.scanAndConnect(10000)) { // 10 second scan
        Serial.println("Connected to Xbox controller!");
    } else {
        Serial.println("No Xbox controller found. Make sure it's in pairing mode.");
        while (1);
    }
}

void loop() {
    if (xbox.isConnected()) {
        // Update controller state
        if (xbox.update()) {
            // Get normalized values for robot control
            float leftX = xbox.getLeftStickXNormalized();     // -1.0 to 1.0 (steering)
            float leftY = xbox.getLeftStickYNormalized();     // -1.0 to 1.0 (forward/back)
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
            
            Serial.print("Left: "); Serial.print(leftMotor, 2);
            Serial.print(" | Right: "); Serial.print(rightMotor, 2);
            Serial.print(" | LT: "); Serial.print(leftTrigger, 2);
            Serial.print(" | RT: "); Serial.println(rightTrigger, 2);
        }
    } else {
        Serial.println("Controller disconnected!");
        delay(1000);
        
        // Try to reconnect
        Serial.println("Attempting to reconnect...");
        xbox.scanAndConnect(5000);
    }
    
    delay(50); // Update at ~20Hz
}
*/
