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


bool getGpsData(float &lat, float &lon, char* timestampOutput) {
if (gsmModel == GSM_A7670) {
  Serial.println(F("Requesting GNSS info (AT+CGPSINFO)..."));
  moduleSerial.println("AT+CGPSINFO");
  readSerialResponse(3000UL);

  char* p = strstr(responseBuffer, "+CGPSINFO:");
  if (!p) { Serial.println(F("ERROR: No +CGPSINFO in response")); return false; }

  // --- découpe aux virgules ------------------------------------------------
  char buf[128];
  strncpy(buf, p + 11, sizeof(buf) - 1);   // saute "+CGPSINFO:"
  buf[sizeof(buf) - 1] = '\0';
  // enlève éventuel \r ou \n
  char* eol = strpbrk(buf, "\r\n");
  if (eol) *eol = '\0';

  //          0      1  2       3  4      5          6   7   8
  // ex:  " 4649.43,N,07114.47,W,060525,012551.00,-9.3,0.0,86.8"
  char* tok = strtok(buf, ",");
  if (!tok) return false;
  float latRaw = atof(tok);

  tok = strtok(nullptr, ",");  if (!tok) return false;
  char ns = *tok;

  tok = strtok(nullptr, ",");  if (!tok) return false;
  float lonRaw = atof(tok);

  tok = strtok(nullptr, ",");  if (!tok) return false;
  char ew = *tok;

  tok = strtok(nullptr, ",");  if (!tok) return false;
  char date[7]; strncpy(date, tok, 6); date[6] = '\0';

  tok = strtok(nullptr, ",");  if (!tok) return false;
  char utc[7];  strncpy(utc, tok, 6);  utc[6]  = '\0';   // on ignore les décimales

  // --- conversion DMM → degrés décimaux -----------------------------------
  int degLat = (int)(latRaw / 100);
  float minLat = latRaw - degLat * 100;
  lat = degLat + minLat / 60.0f;

  int degLon = (int)(lonRaw / 100);
  float minLon = lonRaw - degLon * 100;
  lon = degLon + minLon / 60.0f;

  if (ns == 'S' || ns == 's') lat = -lat;
  if (ew == 'W' || ew == 'w') lon = -lon;

  Serial.print(F("Parsed Lat: ")); Serial.print(lat, 6);
  Serial.print(F(" Lon: "));       Serial.println(lon, 6);

  // --- horodatage Traccar --------------------------------------------------
  snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
           "20%.2s-%.2s-%.2s%%20%.2s:%.2s:%.2s",
           date + 4, date + 2, date,
           utc, utc + 2, utc + 4);

  return true;
}


  // --- SIM7000G : AT+CGNSINF
  Serial.println(F("Requesting GNSS info (AT+CGNSINF)..."));
  moduleSerial.println("AT+CGNSINF");
  readSerialResponse(3000UL);

  char* startOfResponse = strstr(responseBuffer, "+CGNSINF:");
  if (!startOfResponse) {
    Serial.println(F("ERROR: No '+CGNSINF:' in response."));
    return false;
  }

  char* dataStart = startOfResponse + 10;
  uint8_t gnssRunStatus = 0, fixStatus = 0;
  if (sscanf(dataStart, "%hhu,%hhu", &gnssRunStatus, &fixStatus) < 2) {
    Serial.println(F("ERROR: Failed parsing GNSS run/fix status."));
    return false;
  }

  if (gnssRunStatus != 1 || fixStatus != 1) {
    Serial.println(F("No valid fix."));
    return false;
  }

  char tempTimestamp[20] = {0};
  float tempLat = 0.0f, tempLon = 0.0f;
  char tempFloatBuf[16] = {0};

  // Timestamp
  char* fieldStart = strchr(dataStart, ',');
  for (int i = 0; i < 2; i++) {
    if (!fieldStart) return false;
    fieldStart = strchr(fieldStart + 1, ',');
  }
  if (!fieldStart) return false;
  fieldStart++;
  char* fieldEnd = strchr(fieldStart, ',');
  if (!fieldEnd) return false;
  strncpy(tempTimestamp, fieldStart, min((int)(fieldEnd - fieldStart), 19));
  tempTimestamp[19] = '\0';

  // Latitude
  fieldStart = fieldEnd + 1;
  fieldEnd = strchr(fieldStart, ',');
  strncpy(tempFloatBuf, fieldStart, min((int)(fieldEnd - fieldStart), 15));
  tempLat = atof(tempFloatBuf);

  // Longitude
  fieldStart = fieldEnd + 1;
  fieldEnd = strchr(fieldStart, ',');
  strncpy(tempFloatBuf, fieldStart, min((int)(fieldEnd - fieldStart), 15));
  tempLon = atof(tempFloatBuf);

  lat = tempLat;
  lon = tempLon;

  Serial.print(F("Parsed Lat: ")); Serial.print(lat, 6);
  Serial.print(F(" Parsed Lon: ")); Serial.println(lon, 6);

  int year, month, day, hour, minute, second;
  if (sscanf(tempTimestamp, "%4d%2d%2d%2d%2d%2d",
             &year, &month, &day, &hour, &minute, &second) < 6) {
    Serial.println(F("ERROR: Failed parsing timestamp."));
    return false;
  }

  snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
           "%04d-%02d-%02d%%20%02d:%02d:%02d",
           year, month, day, hour, minute, second);
  return true;
}
