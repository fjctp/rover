#include "ArduinoUtils.h"
#include <Arduino.h>

bool log_ready = false;

const char *getLogLevelName(const LogLevel level)
{
  switch (level)
  {
  case LogLevel::ERROR:
    return "ERROR";
  case LogLevel::WARN:
    return "WARN";
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::VERBOSE:
    return "VERBOSE";
  default:
    return "INFO";
  }
}

void log(const LogLevel level, const char *msg)
{
#if DEBUG_LEVEL > -1
  if (!log_ready) {
    Serial.begin(BAND_RATE);
    while (!Serial);
    log_ready = true;
  }

  if (level > DEBUG_LEVEL)
    return;
  char combined[255];
  sprintf(combined, "[%s] %s\n",
          getLogLevelName(level), msg);
  Serial.print(combined);
#endif
}

void sleep_forever(void)
{
  const uint32_t SLEEP_MS = 60 * 1e3;
  while (true)
    delay(SLEEP_MS);
}
