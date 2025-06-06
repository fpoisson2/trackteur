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
#define HTTP_REQUEST_BUFFER_SIZE 256
#define RESPONSE_BUFFER_SIZE 128
#define GPS_TIMESTAMP_TRACCAR_BUF_SIZE 25
#define SD_CS_PIN 8

#define RECONNECT_PERIOD    60000UL   // 60 secondes entre tentatives réseau

extern char responseBuffer[RESPONSE_BUFFER_SIZE];

bool initialCommunication();
bool step1NetworkSettings();
bool waitForSimReady();
bool step2NetworkRegistration();
bool step3PDPContext();
bool step4EnableGNSS();


// --- Surveillance perte de fix GNSS -----------------------------
static uint8_t consecutiveGpsFails = 0;          // compteur d’échecs
const uint8_t GPS_FAIL_THRESHOLD  = 10;          // déclenche à 10 échecs

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

extern unsigned long lastSendTime;


extern uint8_t responseBufferPos;

extern float currentLat;
extern float currentLon;
extern char gpsTimestampTraccar[GPS_TIMESTAMP_TRACCAR_BUF_SIZE];

extern bool setupSuccess;

void initializeWatchdog();
void initializeSerial();

void serviceNetwork();

const char* toStr(NetState state);


#endif
