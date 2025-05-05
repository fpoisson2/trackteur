/* =======================================================================
 * Projet      : Traceur GPS LTE avec SD et Traccar
 * Fichier     : gps.cpp
 * Description : Contient la fonction `getGpsData` qui interroge le module GNSS
 *               via AT+CGNSINF, valide l’état du fix GPS, extrait la latitude,
 *               la longitude et le timestamp, puis formate le tout pour Traccar.
 *
 * Auteur      : Francis Poisson-Gagnon
 * Dépôt Git   : https://github.com/fpoisson2/trackteur
 * Matériel    : SIM7000G (GNSS intégré) + Arduino Nano
 * ======================================================================= */


#include "common.h"
#include "gps.h"
#include "gsm.h"  // pour readSerialResponse()


// Récupère et formate les données GPS
bool getGpsData(float &lat, float &lon, char* timestampOutput) {
  Serial.println(F("Requesting GNSS info (AT+CGNSINF)..."));
  moduleSerial.println("AT+CGNSINF");
  readSerialResponse(3000UL);

  char* startOfResponse = strstr(responseBuffer, "+CGNSINF:");
  if (!startOfResponse) {
    Serial.println(F("ERROR: No '+CGNSINF:' in response."));
    return false;
  }
  char* dataStart = startOfResponse + 10; // passe le "+CGNSINF: "

  uint8_t gnssRunStatus = 0, fixStatus = 0;
  if (sscanf(dataStart, "%hhu,%hhu", &gnssRunStatus, &fixStatus) < 2) {
    Serial.println(F("ERROR: Failed parsing GNSS run/fix status."));
    Serial.print(F(" Data starts at: ")); Serial.println(dataStart);
    return false;
  }
  Serial.print(F("GNSS Status: run="));
  Serial.print(gnssRunStatus);
  Serial.print(F(", fix="));
  Serial.println(fixStatus);

  if (gnssRunStatus != 1 || fixStatus != 1) {
    Serial.println(F("No valid fix."));
    return false;
  }

  char tempTimestamp[20] = {0};
  float tempLat = 0.0f, tempLon = 0.0f;
  char tempFloatBuf[16] = {0};

  // Récupère le timestamp
  char* fieldStart = strchr(dataStart, ',');
  if (fieldStart) fieldStart = strchr(fieldStart + 1, ',');
  else { Serial.println(F("ERR: Parse fail (comma 1)")); return false; }
  if (!fieldStart) { Serial.println(F("ERR: Parse fail (comma 2)")); return false; }
  fieldStart++;
  char* fieldEnd = strchr(fieldStart, ',');
  if (!fieldEnd) { Serial.println(F("ERR: Parse fail (comma 3)")); return false; }
  uint8_t fieldLen = fieldEnd - fieldStart;
  if (fieldLen >= sizeof(tempTimestamp)) fieldLen = sizeof(tempTimestamp) - 1;
  strncpy(tempTimestamp, fieldStart, fieldLen);
  tempTimestamp[fieldLen] = '\0';

  // Parse la latitude
  fieldStart = fieldEnd + 1;
  fieldEnd = strchr(fieldStart, ',');
  if (!fieldEnd) { Serial.println(F("ERR: Parse fail (comma 4)")); return false; }
  fieldLen = fieldEnd - fieldStart;
  if (fieldLen >= sizeof(tempFloatBuf)) fieldLen = sizeof(tempFloatBuf) - 1;
  strncpy(tempFloatBuf, fieldStart, fieldLen);
  tempFloatBuf[fieldLen] = '\0';
  tempLat = atof(tempFloatBuf);

  // Parse la longitude
  fieldStart = fieldEnd + 1;
  fieldEnd = strchr(fieldStart, ',');
  if (!fieldEnd) { Serial.println(F("ERR: Parse fail (comma 5)")); return false; }
  fieldLen = fieldEnd - fieldStart;
  if (fieldLen >= sizeof(tempFloatBuf)) fieldLen = sizeof(tempFloatBuf) - 1;
  strncpy(tempFloatBuf, fieldStart, fieldLen);
  tempFloatBuf[fieldLen] = '\0';
  tempLon = atof(tempFloatBuf);

  lat = tempLat;
  lon = tempLon;
  Serial.print(F("Parsed Lat: ")); Serial.print(lat, 6);
  Serial.print(F(" Parsed Lon: ")); Serial.println(lon, 6);

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (sscanf(tempTimestamp, "%4d%2d%2d%2d%2d%2d",
             &year, &month, &day, &hour, &minute, &second) < 6) {
    Serial.println(F("ERROR: Failed parsing time."));
    Serial.print(F(" Timestamp str: ")); Serial.println(tempTimestamp);
    return false;
  }
  if (year < 2024 || year > 2038) {
    Serial.print(F("WARNING: Unlikely year parsed: "));
    Serial.println(year);
  }
  snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
           "%04d-%02d-%02d%%20%02d:%02d:%02d",
           year, month, day, hour, minute, second);
  return true;
}
