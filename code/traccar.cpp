/* =======================================================================
 * Projet      : Traceur GPS LTE avec SD et Traccar
 * Fichier     : traccar.cpp
 * Description : Implémente la fonction `sendGpsToTraccar`, qui établit une
 *               connexion TCP avec un serveur Traccar, formate une requête
 *               HTTP GET avec les données GPS, envoie la requête, attend la
 *               réponse HTTP puis ferme proprement la socket.
 *
 * Auteur      : Francis Poisson-Gagnon
 * Dépôt Git   : https://github.com/fpoisson2/trackteur
 * Matériel    : Arduino Nano (ATmega328P) + SIM7000G
 * ======================================================================= */

#include "config.h"
#include "logging.h"
#include "common.h"
#include "gsm.h"       // executeSimpleCommand, readSerialResponse, moduleSerial
#include "traccar.h"

// ---------------------------------------------------------------------------
// Petite fonction utilitaire : attend l'apparition de `pattern` sur le port
// série du module LTE ou jusqu'au timeout (ms). Retourne true si trouvé.
// ---------------------------------------------------------------------------
static bool waitForPattern(const char *pattern, uint32_t timeoutMs)
{
  const size_t plen = strlen(pattern);
  size_t matched   = 0;
  uint32_t start   = millis();

  while (millis() - start < timeoutMs) {
    if (moduleSerial.available()) {
      char c = moduleSerial.read();
      if (c == pattern[matched]) {
        if (++matched == plen) {
          return true;            // motif complet reçu
        }
      } else {
        matched = (c == pattern[0]) ? 1 : 0; // recommence la détection
      }
    }
  }
  return false;  // timeout
}

// ---------------------------------------------------------------------------
// Envoi d'une position GPS au serveur Traccar (protocole OsmAnd)
// ---------------------------------------------------------------------------
bool sendGpsToTraccar(const char *host, uint16_t port, const char *deviceId,
                      float lat, float lon, const char *timestampStr)
{
  // 1) Ouverture de la socket TCP ------------------------------------------------
  DBGLN(F("Connecting to Traccar…"));
  if (!tcpOpen(host, port)) {
    DBGLN(F("❌ tcpOpen failed"));
    return false;
  }
  DBGLN(F("✔ Socket ouverte"));

  // 2) Construction de la requête HTTP ------------------------------------------
  char latStr[12], lonStr[12];
  dtostrf(lat, 0, 6, latStr);
  dtostrf(lon, 0, 6, lonStr);

  char httpReq[HTTP_REQUEST_BUFFER_SIZE];
  int reqLen = snprintf(httpReq, sizeof(httpReq),
    "GET /?id=%s&lat=%s&lon=%s&timestamp=%s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Connection: close\r\n"
    "\r\n",
    deviceId, latStr, lonStr, timestampStr, host);

  if (reqLen <= 0 || reqLen >= (int)sizeof(httpReq)) {
    DBGLN(F("❌ HTTP request too long"));
    tcpClose();
    return false;
  }

  INFOLN(httpReq);  // pour debug

  // 3) Envoi de la requête -------------------------------------------------------
  DBG(F("Sending ")); DBG(reqLen); DBGLN(F(" bytes…"));
  if (!tcpSend(httpReq, reqLen)) {
    DBGLN(F("❌ tcpSend failed"));
    tcpClose();
    return false;
  }

  // 4) Attendre la réponse HTTP (200 OK) ----------------------------------------
  if (!waitForPattern("HTTP/1.1", 5000)) {   // 5 s max
    DBGLN(F("⚠ Pas de réponse HTTP (timeout)"));
  } else {
    DBGLN(F("✔ Réponse HTTP reçue"));
  }

  // 5) Fermeture propre de la socket --------------------------------------------
  tcpClose();
  DBGLN(F("Socket fermée"));
  return true;
}
