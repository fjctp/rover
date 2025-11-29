#include "ArduinoUtils.h"

#include <Arduino.h>

void log(const char* level, const char* msg)
{
  char combined[255];
  sprintf(combined, "[%s] %s", level, msg);
  Serial.print(combined);
}

void sleep_forever(void)
{
  const uint32_t SLEEP_MS = 60 * 1e3;
  while (true)
    delay(SLEEP_MS);
}
