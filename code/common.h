#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <SPI.h>
#include <PF.h>             // Petit FatFs
#include <diskio.h>
#include <SoftwareSerial.h>
#include <stdio.h>          // pour snprintf
#include <string.h>         // pour strstr, memset, strncpy, etc.
#include <stdlib.h>         // pour atof, dtostrf
#include <avr/wdt.h>        // pour wdt_reset et wdt_enable

// --- Constantes globales ---
#define NET_FAIL_THRESHOLD 5
#define HISTORY 3
#define MAX_SECTORS 1000UL
#define HTTP_REQUEST_BUFFER_SIZE 200
#define RESPONSE_BUFFER_SIZE 128
#define GPS_TIMESTAMP_TRACCAR_BUF_SIZE 25
#define SD_CS_PIN 8

#define GPS_POLL_INTERVAL   10000UL   // 10 secondes entre lectures GPS
#define RECONNECT_PERIOD    60000UL   // 60 secondes entre tentatives réseau

#define DEBUG  // ← commente cette ligne pour désactiver les logs

#ifdef DEBUG
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

extern char responseBuffer[RESPONSE_BUFFER_SIZE];

bool initialCommunication();
bool step1NetworkSettings();
bool waitForSimReady();
bool step2NetworkRegistration();
bool step3PDPContext();
bool step4EnableGNSS();

// === État réseau ===
enum class NetState { BOOTING, OFFLINE, ONLINE };
extern NetState netState;

// --- Variables globales partagées ---
extern FATFS fs;
extern const char* LOG_FILE;

extern unsigned long lastGpsPoll;
extern unsigned long lastReconnectAttempt;

extern uint32_t lastSectorUsed;
extern uint32_t sectorIndex;
extern uint8_t consecutiveNetFails;

extern const uint8_t powerPin;
extern const uint8_t swRxPin;
extern const uint8_t swTxPin;

extern bool sdAvailable;

extern SoftwareSerial moduleSerial;

extern const unsigned long moduleBaudRate;
extern const char* APN;

extern const char* TRACCAR_HOST;
extern const uint16_t TRACCAR_PORT;
extern const char* DEVICE_ID;

extern unsigned long lastSendTime;
extern const unsigned long sendInterval;


extern uint8_t responseBufferPos;

extern float currentLat;
extern float currentLon;
extern char gpsTimestampTraccar[GPS_TIMESTAMP_TRACCAR_BUF_SIZE];

extern bool setupSuccess;

void initializeWatchdog();
void initializeSerial();

void serviceNetwork();

#endif
