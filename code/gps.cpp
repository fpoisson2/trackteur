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
  // --- A7670E : section inchangée ------------------------------------------
  if (gsmModel == GSM_A7670) {
    Serial.println(F("Requesting GNSS info (AT+CGPSINFO)..."));
    moduleSerial.println("AT+CGPSINFO");
    readSerialResponse(3000UL);

    char* p = strstr(responseBuffer, "+CGPSINFO:");
    if (!p) { Serial.println(F("ERROR: No +CGPSINFO in response")); return false; }

    // Découpe aux virgules
    char bufA[128];
    strncpy(bufA, p + 11, sizeof(bufA) - 1);
    bufA[sizeof(bufA) - 1] = '\0';
    char* eol = strpbrk(bufA, "\r\n");
    if (eol) *eol = '\0';

    char* tok = strtok(bufA, ","); if (!tok) return false;
    float latRaw = atof(tok);
    tok = strtok(nullptr, ","); if (!tok) return false;
    char ns = *tok;
    tok = strtok(nullptr, ","); if (!tok) return false;
    float lonRaw = atof(tok);
    tok = strtok(nullptr, ","); if (!tok) return false;
    char ew = *tok;
    tok = strtok(nullptr, ","); if (!tok) return false;
    char date[7]; strncpy(date, tok, 6); date[6] = '\0';
    tok = strtok(nullptr, ","); if (!tok) return false;
    char utc[7]; strncpy(utc, tok, 6); utc[6] = '\0';

    int degLat = latRaw / 100;
    float minLat = latRaw - degLat * 100;
    lat = degLat + minLat / 60.0f;
    int degLon = lonRaw / 100;
    float minLon = lonRaw - degLon * 100;
    lon = degLon + minLon / 60.0f;
    if (ns == 'S' || ns == 's') lat = -lat;
    if (ew == 'W' || ew == 'w') lon = -lon;

    Serial.print(F("Parsed Lat: ")); Serial.print(lat, 6);
    Serial.print(F(" Lon: ")); Serial.println(lon, 6);

    snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
             "20%.2s-%.2s-%.2s%%20%.2s:%.2s:%.2s",
             date + 4, date + 2, date,
             utc, utc + 2, utc + 4);
    return true;
  }

  // --- SIM7000G : section corrigée -----------------------------------------
  Serial.println(F("Requesting GNSS info (AT+CGNSINF)..."));
  moduleSerial.println("AT+CGNSINF");
  readSerialResponse(3000UL);

  char* resp = strstr(responseBuffer, "+CGNSINF:");
  if (!resp) {
    Serial.println(F("ERROR: No '+CGNSINF:' in response."));
    return false;
  }

  char* data = resp + 10;  // saute "+CGNSINF: "
  uint8_t run = 0, fix = 0;
  if (sscanf(data, "%hhu,%hhu", &run, &fix) != 2) {
    Serial.println(F("ERROR: Failed parsing GNSS run/fix status."));
    return false;
  }
  if (run != 1 || fix != 1) {
    Serial.println(F("No valid fix."));
    return false;
  }

  // Saute deux champs (run, fix) pour atteindre le timestamp
  char* p2 = data;
  for (int i = 0; i < 2; ++i) {
    p2 = strchr(p2, ',');
    if (!p2) return false;
    ++p2;
  }

  // Extraction de la timestamp brute
  char rawTime[20] = {0};
  char* comma = strchr(p2, ',');
  if (!comma) return false;
  size_t len = comma - p2;
  if (len >= sizeof(rawTime)) len = sizeof(rawTime) - 1;
  strncpy(rawTime, p2, len);
  rawTime[len] = '\0';

  // Suppression des millisecondes
  char* dot = strchr(rawTime, '.');
  if (dot) *dot = '\0';

  // Parsing du timestamp
  int year, month, day, hour, minute, second;
  if (sscanf(rawTime, "%4d%2d%2d%2d%2d%2d",
             &year, &month, &day, &hour, &minute, &second) != 6) {
    Serial.println(F("ERROR: Failed parsing timestamp."));
    return false;
  }

  // Extraction latitude
  char* q = comma + 1;
  char bufG[16] = {0};
  comma = strchr(q, ',');
  if (!comma) return false;
  len = comma - q;
  if (len >= sizeof(bufG)) len = sizeof(bufG) - 1;
  strncpy(bufG, q, len);
  bufG[len] = '\0';
  float latitude = atof(bufG);

  // Extraction longitude
  char* r = comma + 1;
  comma = strchr(r, ',');
  if (!comma) return false;
  len = comma - r;
  if (len >= sizeof(bufG)) len = sizeof(bufG) - 1;
  strncpy(bufG, r, len);
  bufG[len] = '\0';
  float longitude = atof(bufG);

  lat = latitude;
  lon = longitude;
  Serial.print(F("Parsed Lat: ")); Serial.print(lat, 6);
  Serial.print(F(" Lon: ")); Serial.println(lon, 6);

  // Formatage Traccar
  snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
           "%04d-%02d-%02d%%20%02d:%02d:%02d",
           year, month, day, hour, minute, second);
  return true;
}
