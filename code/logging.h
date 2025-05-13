#ifndef LOGGING_H
#define LOGGING_H

#include "config.h"

#if LOG_LEVEL == 2
  #define DBG(x) Serial.print(x)
  #define DBGLN(x) Serial.println(x)
  #define DBG2(x, y) Serial.print(x, y)
  #define DBGLN2(x, y) Serial.println(x, y)
#else
  #define DBG(x)
  #define DBGLN(x)
  #define DBG2(x, y)
  #define DBGLN2(x, y)
#endif

#if LOG_LEVEL >= 1
  #define INFO(x) Serial.print(x)
  #define INFOLN(x) Serial.println(x)
  #define INFO2(x, y) Serial.print(x, y)
  #define INFOLN2(x, y) Serial.println(x, y)
#else
  #define INFO(x)
  #define INFOLN(x)
  #define INFO2(x, y)
  #define INFOLN2(x, y)
#endif

#endif
