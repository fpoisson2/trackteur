/* =======================================================================
 * Projet      : Traceur GPS LTE avec enregistrement SD et envoi Traccar
 * Fichier     : main.ino
 * Description : Initialise le module GSM/GNSS SIM7000G, lit les coordonnées GPS,
 *               envoie les données à un serveur Traccar via TCP et journalise
 *               localement sur carte SD en cas d'échec réseau. Implémente une
 *               logique de reprise et de nettoyage des logs non transmis.
 * Auteur      : Francis Poisson-Gagnon
 * Date        : 2025-05-05
 * Cible       : Arduino Nano (ATmega328P)
 * Dépôt Git   : https://github.com/fpoisson2/trackteur
 * 
 * Dépendances :    if (getGpsData(currentLat, currentLon, gpsTimestampTraccar)) {
      Serial.print(F("GPS OK: Lat=")); Serial.print(currentLat, 6);
      Serial.print(F(" Lon="));        Serial.print(currentLon, 6);
      Serial.print(F(" Time="));       Serial.println(gpsTimestampTraccar);

      logRealPositionToSd(currentLat, currentLon, gpsTimestampTraccar);

      if (netState == NetState::ONLINE) {
        memset(responseBuffer, 0, sizeof(responseBuffer));
        responseBufferPos = 0;

        bool ok = sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, currentLat, currentLon, gpsTimestampTraccar);
        if (ok) {
          Serial.println(F(">>> Send OK."));
          consecutiveNetFails = 0;
          resendLastLog();
        } else {
          Serial.print(F("Network fail #")); Serial.println(++consecutiveNetFails);
          Serial.println(F(">>> Send FAIL."));
          if (consecutiveNetFails >= NET_FAIL_THRESHOLD) {
            Serial.println(F(">>> Trop d'échecs réseau, on redémarre le module GSM."));
            resetGsmModule();
            netState = NetState::OFFLINE;
            lastReconnectAttempt = millis();
            consecutiveNetFails = 0;
          }
        }
      } else {
        Serial.println(F("Pas de réseau, stockage uniquement."));
      }

    }
 *   - common.h         : Définitions globales, watchdog, constantes
 *   - sdlog.h/.cpp     : Gestion de la carte SD avec Petit FatFs
 *   - gsm.h/.cpp       : Communication série AT avec le module LTE
 *   - gps.h/.cpp       : Extraction et formatage des données GNSS
 *   - traccar.h/.cpp   : Format et envoi vers Traccar via TCP
 * ======================================================================= */


#include "common.h"
#include "sdlog.h"
#include "gsm.h"
#include "gps.h"
#include "traccar.h"

void setup() {
  initializeWatchdog();
  initializeSerial();

  initializeSD();
  sectorIndex = loadLogMetadata();
  Serial.print(F("Resuming at sector ")); Serial.println(sectorIndex);

  initializeModulePower(); // met sous tension le module LTE

  netState = NetState::BOOTING;
  lastSendTime = millis();
  lastGpsPoll = millis();

  Serial.println(F("=== SETUP TERMINÉ ==="));
}

void loop() {
  wdt_reset();

  // Mise à jour de l'état du réseau (connexion, reconnexion si besoin)
  serviceNetwork();

  // Lecture GPS périodique
  if (millis() - lastGpsPoll >= GPS_POLL_INTERVAL) {
if (getGpsData(currentLat, currentLon, gpsTimestampTraccar)) {
  Serial.print(F("GPS OK: Lat=")); Serial.print(currentLat, 6);
  Serial.print(F(" Lon="));        Serial.print(currentLon, 6);
  Serial.print(F(" Time="));       Serial.println(gpsTimestampTraccar);

  if (netState == NetState::ONLINE) {
    memset(responseBuffer, 0, sizeof(responseBuffer));
    responseBufferPos = 0;

    bool ok = sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, currentLat, currentLon, gpsTimestampTraccar);
    if (ok) {
      Serial.println(F(">>> Send OK."));
      consecutiveNetFails = 0;

      // vidange des anciennes données seulement
      resendLastLog();
    } else {
      Serial.print(F("Network fail #")); Serial.println(++consecutiveNetFails);
      Serial.println(F(">>> Send FAIL."));
      
      // Log uniquement si l'envoi a échoué
      logRealPositionToSd(currentLat, currentLon, gpsTimestampTraccar);

      if (consecutiveNetFails >= NET_FAIL_THRESHOLD) {
        Serial.println(F(">>> Trop d'échecs réseau, on redémarre le module GSM."));
        resetGsmModule();
        netState = NetState::OFFLINE;
        lastReconnectAttempt = millis();
        consecutiveNetFails = 0;
      }
    }

  } else {
    Serial.println(F("Pas de réseau, stockage uniquement."));
    logRealPositionToSd(currentLat, currentLon, gpsTimestampTraccar);
  }
}
 else {
      Serial.println(F(">>> Aucun fix GPS."));
    }

    lastGpsPoll = millis();
  }

  delay(100);
}
