#ifndef ARDUINO_UTILS_H
#define ARDUINO_UTILS_H

typedef enum {
  ERROR   = 0,
  WARN    = 1,
  INFO    = 2,
  DEBUG   = 3,
  VERBOSE = 4
} LogLevel;

const char* getLogLevelName(const LogLevel level);
void log(const LogLevel level, const char* msg);
void sleep_forever(void);

#endif // ARDUINO_UTILS_H