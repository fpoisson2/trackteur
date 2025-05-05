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

// Envoie des données GPS au serveur Traccar
bool sendGpsToTraccar(const char* host, uint16_t port, const char* deviceId,
                      float lat, float lon, const char* timestampStr) {
  bool connectSuccess = false, sendSuccess = false;
  char httpRequestBuffer[HTTP_REQUEST_BUFFER_SIZE];

  Serial.println(F("Connecting to Traccar..."));
  moduleSerial.print(F("AT+CIPSTART=\"TCP\",\""));
  moduleSerial.print(host);
  moduleSerial.print(F("\","));
  moduleSerial.println(port);
  if (executeSimpleCommand(responseBuffer, "OK", 20000UL, 1)) {
    if (strstr(responseBuffer, "CONNECT OK") || strstr(responseBuffer, "ALREADY CONNECT")) {
      connectSuccess = true;
      Serial.println(F("Connection établie."));
    } else {
      readSerialResponse(500UL);
      if (strstr(responseBuffer, "CONNECT OK") || strstr(responseBuffer, "ALREADY CONNECT")) {
        connectSuccess = true;
        Serial.println(F("Connection établie (async)."));
      } else {
        Serial.println(F("Échec de connexion."));
      }
    }
  } else {
    Serial.println(F("CIPSTART command failed."));
  }

  if (connectSuccess) {
    char latStr[15], lonStr[15];
    dtostrf(lat, 4, 6, latStr);
    dtostrf(lon, 4, 6, lonStr);
    snprintf(httpRequestBuffer, HTTP_REQUEST_BUFFER_SIZE,
             "GET /?id=%s&lat=%s&lon=%s&timestamp=%s HTTP/1.1\r\nHost: %s\r\n\r\n",
             deviceId, latStr, lonStr, timestampStr, host);
    int dataLength = strlen(httpRequestBuffer);
    Serial.print(F("Sending CIPSEND (len "));
    Serial.print(dataLength);
    Serial.println(F(")..."));
    if (dataLength >= HTTP_REQUEST_BUFFER_SIZE || dataLength >= 1460) {
      Serial.println(F("ERROR: Request too long!"));
      sendSuccess = false;
    } else {
      snprintf(responseBuffer, RESPONSE_BUFFER_SIZE, "AT+CIPSEND=%d", dataLength);
      moduleSerial.println(responseBuffer);
      readSerialResponse(500UL);  // Attend le ">"
      if (strchr(responseBuffer, '>')) {
        Serial.println(F("Got '>'. Sending HTTP..."));
        moduleSerial.print(httpRequestBuffer);
        readSerialResponse(10000UL);
        if (strstr(responseBuffer, "SEND OK")) {
          sendSuccess = true;
          Serial.println(F("SEND OK."));
          readSerialResponse(500UL);
        } else {
          sendSuccess = false;
          Serial.println(F("SEND FAIL/TIMEOUT?"));
        }
      } else {
        sendSuccess = false;
        Serial.println(F("No '>' prompt."));
      }
    }
  } else {
    Serial.println(F("Skipping send (no connection)."));
  }

  if (connectSuccess) {
    Serial.println(F("Closing connection..."));
    if (!executeSimpleCommand("AT+CIPCLOSE=0", "CLOSE OK", 500UL, 1))
      Serial.println(F("CIPCLOSE failed?"));
  } else {
    executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000UL, 1);
  }
  delay(1000);
  return connectSuccess && sendSuccess;
}
