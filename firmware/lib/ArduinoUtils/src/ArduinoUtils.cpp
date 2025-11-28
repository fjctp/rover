#include "ArduinoUtils.h"

#include <Arduino.h>

void log(const char* msg)
{
  Serial.print(msg);
}

void sleep_forever(void)
{
  const uint32_t SLEEP_MS = 60 * 1e3;
  while (true)
    delay(SLEEP_MS);
}
