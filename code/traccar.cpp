/* =======================================================================
 * Projet      : Traceur GPS LTE avec SD et Traccar
 * Fichier     : traccar.cpp
 * Description : Implémente la fonction `sendGpsToTraccar`, qui établit une
 *               connexion TCP avec un serveur Traccar, formate une requête
 *               HTTP GET avec les données GPS, envoie la requête, et
 *               gère la fermeture de la session réseau.
 *
 * Auteur      : Francis Poisson-Gagnon
 * Dépôt Git   : https://github.com/fpoisson2/trackteur
 * Matériel    : Arduino Nano (ATmega328P) + SIM7000G
 * ======================================================================= */

#include "common.h"
#include "gsm.h"       // pour executeSimpleCommand, readSerialResponse, moduleSerial
#include "traccar.h"

bool sendGpsToTraccar(const char* host, uint16_t port, const char* deviceId,
                      float lat, float lon, const char* timestampStr)
{
  /* ---------- 1. Ouvrir la socket TCP ----------- */
  DBGLN(F("Connecting to Traccar…"));
  if (!tcpOpen(host, port)) {
    DBGLN(F("❌ tcpOpen failed"));
    return false;
  }
  DBGLN(F("✔ Socket ouverte"));

  /* ---------- 2. Construire la requête ---------- */
  char latStr[16], lonStr[16];
  dtostrf(lat, 0, 6, latStr);
  dtostrf(lon, 0, 6, lonStr);

  char httpReq[HTTP_REQUEST_BUFFER_SIZE];
  snprintf(httpReq, sizeof(httpReq),
           "GET /?id=%s&lat=%s&lon=%s&timestamp=%s HTTP/1.1\r\nHost: %s\r\n\r\n",
           deviceId, latStr, lonStr, timestampStr, host);

  uint16_t reqLen = strlen(httpReq);
  if (reqLen >= sizeof(httpReq)) {
    DBGLN(F("❌ HTTP request too long"));
    tcpClose();
    return false;
  }

  /* ---------- 3. Envoyer la requête ------------- */
  DBG(F("Sending ")); DBG(reqLen); DBGLN(F(" bytes…"));
  if (!tcpSend(httpReq, reqLen)) {
    DBGLN(F("❌ tcpSend failed"));
    tcpClose();
    return false;
  }
  DBGLN(F("✔ Send OK"));

  /* ---------- 4. Fermer proprement -------------- */
  tcpClose();
  DBGLN(F("Socket fermée"));
  return true;
}
