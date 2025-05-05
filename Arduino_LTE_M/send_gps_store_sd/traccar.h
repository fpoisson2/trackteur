#ifndef TRACCAR_H
#define TRACCAR_H

#include <Arduino.h>

bool sendGpsToTraccar(const char* host, uint16_t port, const char* deviceId,
                      float lat, float lon, const char* timestampStr);

#endif
