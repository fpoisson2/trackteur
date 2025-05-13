#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Logging level ---
// 0 = No logs, 1 = Info, 2 = Debug
#define LOG_LEVEL 2

// --- Configuration système ---
extern const unsigned long GPS_POLL_INTERVAL;

// --- Paramètres réseau et Traccar ---
extern const char* APN;
extern const char* TRACCAR_HOST;
extern const uint16_t TRACCAR_PORT;
extern const char* DEVICE_ID;

#endif
