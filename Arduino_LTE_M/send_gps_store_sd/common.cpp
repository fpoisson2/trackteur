/* =======================================================================
 * Projet      : Traceur GPS LTE avec SD et Traccar
 * Fichier     : common.cpp
 * Description : Déclare et initialise les variables globales, les objets
 *               de communication série, les constantes de configuration
 *               réseau, GPS, Traccar ainsi que les buffers partagés.
 *               Contient aussi les fonctions d'initialisation du watchdog
 *               et de la liaison série principale (USB).
 *
 * Auteur      : Francis Poisson-Gagnon
 * Dépôt Git   : https://github.com/fpoisson2/trackteur
 * Date        : 2025-05-05
 * Matériel    : Arduino Nano (ATmega328P) + SIM7000G
 * ======================================================================= */

#include "common.h"

FATFS fs;
const char* LOG_FILE = "GPS_LOG.CSV";

uint32_t lastSectorUsed = 1;
uint32_t sectorIndex = 0;
uint8_t consecutiveNetFails = 0;

const uint8_t powerPin = 2;
const uint8_t swRxPin = 3;
const uint8_t swTxPin = 4;

SoftwareSerial moduleSerial(swRxPin, swTxPin);

const unsigned long moduleBaudRate = 9600UL;
const char* APN = "em";

const char* TRACCAR_HOST = "trackteur.ve2fpd.com";
const uint16_t TRACCAR_PORT = 5055;
const char* DEVICE_ID = "212910";

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000UL;  // 10s entre envois

char responseBuffer[RESPONSE_BUFFER_SIZE];
uint8_t responseBufferPos = 0;

float currentLat = 0.0f;
float currentLon = 0.0f;
char gpsTimestampTraccar[GPS_TIMESTAMP_TRACCAR_BUF_SIZE] = {0};

bool setupSuccess = false;


void initializeWatchdog() {
  wdt_enable(WDTO_8S);
}

void initializeSerial() {
  Serial.begin(115200);
  while (!Serial) { ; }
  Serial.println(F("--- Arduino Initialized ---"));
}
