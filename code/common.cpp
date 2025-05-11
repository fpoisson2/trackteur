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

unsigned long lastGpsPoll = 0;
unsigned long lastReconnectAttempt = 0;

uint32_t lastSectorUsed = 1;
uint32_t sectorIndex = 0;
uint8_t consecutiveNetFails = 0;

const uint8_t powerPin = 2;
const uint8_t swRxPin = 3;
const uint8_t swTxPin = 4;

SoftwareSerial moduleSerial(swRxPin, swTxPin);

const unsigned long moduleBaudRate = 9600UL;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000UL;  // 10s entre envois

char responseBuffer[RESPONSE_BUFFER_SIZE];
uint8_t responseBufferPos = 0;

NetState netState = NetState::BOOTING;
float currentLat = 0.0f;
float currentLon = 0.0f;
char gpsTimestampTraccar[GPS_TIMESTAMP_TRACCAR_BUF_SIZE] = {0};

bool setupSuccess = false;


bool sdAvailable = false;

void initializeWatchdog() {
  wdt_enable(WDTO_8S);
}

void initializeSerial() {
  Serial.begin(115200);
  while (!Serial) { ; }
  INFOLN(F("Arduino initialisé"));
}

// === Machine d’état réseau ===
void serviceNetwork() {
  switch (netState) {
    case NetState::BOOTING:
     INFOLN(F("Initialisation"));
      if (initialCommunication() &&
          step1NetworkSettings() &&
          waitForSimReady() &&
          step2NetworkRegistration() &&
          step3PDPContext()) {
        INFOLN(F("Connecté au réseau."));
        netState = NetState::ONLINE;
        consecutiveNetFails = 0;
      } else {
        INFOLN(F("Échec d'initialisation, passage OFFLINE."));
        netState = NetState::OFFLINE;
        lastReconnectAttempt = millis();
      }
      break;

    case NetState::OFFLINE:
      if (millis() - lastReconnectAttempt >= RECONNECT_PERIOD) {
        INFOLN(F("Tentative de reconnexion..."));
        if (initialCommunication() &&
            step1NetworkSettings() &&
            waitForSimReady() &&
            step2NetworkRegistration() &&
            step3PDPContext()) {
          INFOLN(F("Reconnexion réussie."));
          netState = NetState::ONLINE;
          consecutiveNetFails = 0;
        } else {
          INFOLN(F("Reconnexion échouée."));
          lastReconnectAttempt = millis();
        }
      }
      break;

    case NetState::ONLINE:
      // Pas de traitement ici, le statut sera mis à jour en cas d’erreurs ailleurs
      break;
  }
}
