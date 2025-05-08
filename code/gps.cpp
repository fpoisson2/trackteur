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
    DBGLN(F("Requesting GNSS info (AT+CGPSINFO)..."));
    moduleSerial.println("AT+CGPSINFO");
    readSerialResponse(3000UL);

    char* p = strstr(responseBuffer, "+CGPSINFO:");
    if (!p) { DBGLN(F("ERROR: No +CGPSINFO in response")); return false; }

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

    DBG(F("Parsed Lat: ")); DBG2(lat, 6);
    DBG(F(" Lon: ")); DBGLN2(lon, 6);

    snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
             "20%.2s-%.2s-%.2s%%20%.2s:%.2s:%.2s",
             date + 4, date + 2, date,
             utc, utc + 2, utc + 4);
    return true;
  }

DBGLN(F("Requesting GNSS info (AT+CGNSINF)..."));
moduleSerial.println("AT+CGNSINF");
readSerialResponse(3000UL);

char* resp = strstr(responseBuffer, "+CGNSINF:");
if (!resp) {
  DBGLN(F("ERROR: No '+CGNSINF:' in response."));
  return false;
}

char* data = resp + 10;  // saute "+CGNSINF: "
char* token;
uint8_t run = 0;
uint8_t fix = 0;

// Get Run Status (field 0)
token = strtok(data, ",");
if (token != nullptr && strlen(token) > 0) {
  run = atoi(token);
} else {
  DBGLN(F("ERROR: Failed parsing run status."));
  return false;
}

// Get Fix Status (field 1)
token = strtok(nullptr, ",");
if (token != nullptr && strlen(token) > 0) {
  fix = atoi(token);
} else {
   DBGLN(F("ERROR: Failed parsing fix status."));
   return false;
}

// Check for valid fix (run=1 and fix=1)
if (run != 1 || fix != 1) {
  DBGLN(F("No valid fix (run: ")); DBG(run); DBG(F(", fix: ")); DBG(fix); DBGLN(F(")."));
  return false;
}

// Get Timestamp (field 2)
token = strtok(nullptr, ",");
if (token == nullptr) {
  DBGLN(F("ERROR: Failed getting timestamp token."));
  return false;
}
char rawTime[20]; // Increased size slightly for safety
strncpy(rawTime, token, sizeof(rawTime) - 1);
rawTime[sizeof(rawTime) - 1] = '\0';

// Suppression des millisecondes
char* dot = strchr(rawTime, '.');
if (dot) *dot = '\0';

// Parsing du timestamp
int year, month, day, hour, minute, second;
if (sscanf(rawTime, "%4d%2d%2d%2d%2d%2d",
           &year, &month, &day, &hour, &minute, &second) != 6) {
  DBGLN(F("ERROR: Failed parsing timestamp format."));
  return false;
}

// Get Latitude (field 3)
token = strtok(nullptr, ",");
if (token == nullptr) {
   DBGLN(F("ERROR: Failed getting latitude token."));
   return false;
}
float latitude = atof(token);

// Get Longitude (field 4)
token = strtok(nullptr, ",");
if (token == nullptr) {
   DBGLN(F("ERROR: Failed getting longitude token."));
   return false;
}
float longitude = atof(token);

// Assign parsed values to output parameters
lat = latitude;
lon = longitude;
DBG(F("Parsed Lat: ")); DBG2(lat, 6);
DBG(F(" Lon: ")); DBGLN2(lon, 6);

// Formatage Traccar
snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
         "%04d-%02d-%02d%%20%02d:%02d:%02d",
         year, month, day, hour, minute, second);

return true; // Success!
}
