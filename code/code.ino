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


// --- Niveau de journalisation (logging) ---
// 0 = Aucun journal (désactive les logs pour économiser de la mémoire et du temps d'exécution)
// 1 = Informations générales (messages importants comme les connexions, erreurs majeures)
// 2 = Débogage détaillé (affiche tous les messages utiles au développement et à la mise au point)
#define LOG_LEVEL 2

// --- Configuration de l'APN (Access Point Name) ---
// L'APN est nécessaire pour que le module LTE se connecte à Internet via le réseau mobile.
// Choisissez l’APN selon la carte SIM utilisée.
const char* APN = "onomondo";  // Choix de l'APN selon le fournisseur SIM : "onomondo", "em" (Emnify), ou "hologram" (Hologram.io). Modifier la valeur ici selon l'opérateur utilisé.

// --- Paramètres de connexion au serveur Traccar ---
// Traccar est un serveur de suivi GPS auquel les coordonnées sont envoyées via une requête HTTP.
const char* TRACCAR_HOST = "trackteur.ve2fpd.com";  // Nom de domaine ou adresse IP du serveur Traccar
const uint16_t TRACCAR_PORT = 5055;                 // Port utilisé pour envoyer les données (par défaut 5055 pour HTTP)
const char* DEVICE_ID = "212910";                   // Identifiant unique de l'appareil dans Traccar (utilisé pour le reconnaître dans l’interface)


void setup() {
  initializeWatchdog();
  initializeSerial();

  initializeSD();
  sectorIndex = loadLogMetadata();
  DBG(F("Resuming at sector ")); DBGLN(sectorIndex);

  initializeModulePower(); // met sous tension le module LTE

  netState = NetState::BOOTING;
  lastSendTime = millis();
  lastGpsPoll = millis();
  initialAT();
  serviceNetwork();
  step4EnableGNSS();
  INFOLN(F("Setup terminé"));
}

void loop() {
  wdt_reset();

  serviceNetwork();

  // Lecture GPS périodique
  if (millis() - lastGpsPoll >= GPS_POLL_INTERVAL) {
if (getGpsData(currentLat, currentLon, gpsTimestampTraccar)) {
  DBG(F("GPS OK: Lat=")); DBG2(currentLat, 6);
  DBG(F(" Lon="));        DBG2(currentLon, 6);
  DBG(F(" Time="));       DBGLN(gpsTimestampTraccar);
  if (gsmModel == GSM_SIM7070) {
    disableGNSS();
  }

  // Mise à jour de l'état du réseau (connexion, reconnexion si besoin)
  if (netState == NetState::ONLINE) {
    memset(responseBuffer, 0, sizeof(responseBuffer));
    responseBufferPos = 0;

    bool ok = sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, currentLat, currentLon, gpsTimestampTraccar);
    if (ok) {
      DBGLN(F(">>> Send OK."));
      consecutiveNetFails = 0;

      // vidange des anciennes données seulement
      resendLastLog();
    } else {
      DBG(F("Network fail #")); DBGLN(++consecutiveNetFails);
      DBGLN(F(">>> Send FAIL."));
      
      // Log uniquement si l'envoi a échoué
      logRealPositionToSd(currentLat, currentLon, gpsTimestampTraccar);

      if (consecutiveNetFails >= NET_FAIL_THRESHOLD) {
        DBGLN(F(">>> Trop d'échecs réseau, on redémarre le module GSM."));
        resetGsmModule();
        netState = NetState::OFFLINE;
        lastReconnectAttempt = millis();
        consecutiveNetFails = 0;
      }
    }

  } else {
    DBGLN(F("Pas de réseau, stockage uniquement."));
    logRealPositionToSd(currentLat, currentLon, gpsTimestampTraccar);
  }
  step4EnableGNSS();
}
 else {
      DBGLN(F(">>> Aucun fix GPS."));
    }

    lastGpsPoll = millis();
  }

  delay(100);
}
