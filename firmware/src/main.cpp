#include <Arduino.h>

bool state_led = HIGH;

void setup() {
  // put your setup code here, to run once:
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(PIN_LED, state_led);
  state_led = !state_led;
  delay(1000);
}
