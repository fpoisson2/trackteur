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
 * Dépendances :
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
  // 1. Surveillance du watchdog
  initializeWatchdog();

  // 2. Initialisation de la liaison série
  initializeSerial();

  // 3. Montage de la carte SD et lecture des métadonnées
  initializeSD();
  sectorIndex = loadLogMetadata();
  Serial.print(F("Resuming at sector ")); Serial.println(sectorIndex);

  // 4. Mise sous tension du module
  initializeModulePower();

  // 5. Séquences de configuration enchaînées et conditionnelles
  bool setupOk = initialCommunication();
  if (setupOk) setupOk = step1NetworkSettings();
  if (setupOk) setupOk = waitForSimReady();
  if (setupOk) setupOk = step2NetworkRegistration();
  if (setupOk) setupOk = step3PDPContext();
  if (setupOk) setupOk = step4EnableGNSS();
  setupSuccess = setupOk;

  // En cas d'échec : arrêt sécurisé
  if (!setupSuccess) {
    Serial.println(F("=== SETUP FAILED, HALTING ==="));
    while (1) {
      wdt_reset();
      delay(1000);
    }
  }

  // 6. Vidage des logs pendants au démarrage
  flushStartupLogs();

  Serial.println(F("=== SETUP COMPLETE ==="));
  Serial.println(F("Entering main loop..."));
  lastSendTime = millis();
}

void loop() {
  
  if (setupSuccess && (millis() - lastSendTime >= sendInterval)) {
    Serial.print(F("\n--- Interval Timer ("));
    Serial.print(millis() / 1000);
    Serial.println(F("s) ---"));

    /* -------- Code original : GNSS / Traccar -------------- */
    if (getGpsData(currentLat, currentLon, gpsTimestampTraccar)) {
      Serial.print(F("GPS OK: Lat=")); Serial.print(currentLat, 6);
      Serial.print(F(" Lon="));        Serial.print(currentLon, 6);
      Serial.print(F(" Time="));       Serial.println(gpsTimestampTraccar);

      memset(responseBuffer, 0, sizeof(responseBuffer));
      responseBufferPos = 0;

      bool ok = sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, currentLat, currentLon, gpsTimestampTraccar);
     if (ok) {
        consecutiveNetFails = 0;
        Serial.println(F(">>> Send OK."));
        resendLastLog();
      }
      else {
        consecutiveNetFails++;
        Serial.print(F("Network fail #")); Serial.println(consecutiveNetFails);
        Serial.println(F(">>> Send FAIL."));
        logRealPositionToSd(currentLat, currentLon, gpsTimestampTraccar);
        if (consecutiveNetFails >= NET_FAIL_THRESHOLD) {
          resetGsmModule();
          consecutiveNetFails = 0;
        }
      }

    } else {
      Serial.println(F(">>> No GPS fix, skipping send."));
    }
    lastSendTime = millis();
  }
  delay(100);
  wdt_reset();
}
