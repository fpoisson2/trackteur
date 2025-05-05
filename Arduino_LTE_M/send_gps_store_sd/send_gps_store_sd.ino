#include <SPI.h>
#include <PF.h>          // Petit FatFs
#include <diskio.h>
#include <SoftwareSerial.h>
#include <stdio.h>   // pour snprintf
#include <string.h>  // pour strstr, memset, strncpy, strchr, sscanf
#include <stdlib.h>  // pour dtostrf, atof
#include <avr/wdt.h>

#define HTTP_REQUEST_BUFFER_SIZE 256
static uint32_t lastSectorUsed = 1;  // updated when sectorIndex advances
#define MAX_SECTORS 1000UL  // excluding sector 0

/* ====================== Configuration SD ====================== */
#define HISTORY 3    
#define SD_CS_PIN 8
FATFS   fs;
const char* LOG_FILE = "GPS_LOG.CSV";
static uint32_t sectorIndex = 0;   // sector‑append counter

struct LogMeta {
  uint32_t lastSectorIndex;
  uint32_t logCount;         // optional
  char signature[8];         // optional: to verify it's valid
};


// networkfails detection
static uint8_t consecutiveNetFails = 0;
const uint8_t NET_FAIL_THRESHOLD = 5;

// --- Définition des Pins
const uint8_t powerPin = 2;
const uint8_t swRxPin  = 3;
const uint8_t swTxPin  = 4;

// --- Objet Software Serial
SoftwareSerial moduleSerial(swRxPin, swTxPin);

// --- Baud Rate
const unsigned long moduleBaudRate = 9600UL;

// --- Configuration réseau
const char* APN = "em";

// --- Configuration Traccar
const char* TRACCAR_HOST = "trackteur.ve2fpd.com";
const uint16_t TRACCAR_PORT = 5055;
const char* DEVICE_ID = "212910"; // UTILISE TON DEVICE ID

// --- Timing d’envoi
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1UL; // 60 sec

// --- Buffers globaux (diminués pour économiser de la RAM)
#define RESPONSE_BUFFER_SIZE 96          // ← A3 : réduit de 128 à 96 octets
static char responseBuffer[RESPONSE_BUFFER_SIZE];
static uint8_t responseBufferPos = 0;

// --- Stockage des données GPS
float currentLat = 0.0f;
float currentLon = 0.0f;
#define GPS_TIMESTAMP_TRACCAR_BUF_SIZE 25
static char gpsTimestampTraccar[GPS_TIMESTAMP_TRACCAR_BUF_SIZE];

// --- État global
bool setupSuccess = false;

// --- Fonctions Optimisées

// Vide le buffer série
void clearSerialBuffer() {
  while (moduleSerial.available()) moduleSerial.read();
}



void saveLogMetadata(uint32_t currentIndex) {
  wdt_reset(); 
  FRESULT res = PF.open(LOG_FILE);
  if (res != FR_OK) {
    Serial.println(F("PF.open (metadata) failed"));
    return;
  }
  PF.seek(0);  // sector 0 reserved

  struct {
    uint32_t index;
    char signature[8];
  } meta = { currentIndex, "LOGDATA" };

  UINT bw;
  wdt_reset(); 
  res = PF.writeFile(&meta, sizeof(meta), &bw);
  if (res != FR_OK || bw != sizeof(meta)) {
    Serial.println(F("Failed to write metadata."));
    return;
  }

  PF.writeFile(nullptr, 0, &bw); // flush
  Serial.println(F("Metadata updated."));
}

uint32_t loadLogMetadata() {
  PF.open(LOG_FILE);
  PF.seek(0);

  struct {
    uint32_t index;
    char signature[8];
  } meta;

  UINT br;
  FRESULT res = PF.readFile(&meta, sizeof(meta), &br);
  if (res != FR_OK || br != sizeof(meta)) {
    Serial.println(F("Failed to read metadata."));
    lastSectorUsed = 0;
    return 1;
  }

  if (strncmp(meta.signature, "LOGDATA", 7) != 0) {
    Serial.println(F("Metadata invalid. Starting fresh."));
    lastSectorUsed = 0;
    return 1;
  }

  lastSectorUsed = meta.index - 1;
  return meta.index;
}

void resendLastLog() {
  // Nothing to do if we have no saved sectors
  if (lastSectorUsed < 1) return;

  uint32_t s = lastSectorUsed;
  if (PF.open(LOG_FILE) != FR_OK) {
    Serial.print(F("Cannot open log file to resend sector ")); Serial.println(s);
    return;
  }

  // Read just that one sector
  PF.seek(s * 512UL);
  char buf[64];
  UINT br;
  FRESULT res = PF.readFile(buf, sizeof(buf)-1, &br);
  if (res != FR_OK || br < 10) {
    Serial.print(F("Sector ")); Serial.print(s);
    Serial.println(F(" empty or read error—skipping."));
    lastSectorUsed--;
    saveLogMetadata(lastSectorUsed + 1);
    return;
  }
  buf[br] = '\0';

  Serial.print(F("→ Resend sector ")); Serial.print(s);
  Serial.print(F(": “")); Serial.print(buf); Serial.println(F("”"));

  // If already marked sent (#), just bump pointer
  if (buf[0] == '#') {
    Serial.println(F("    Already sent, moving pointer back."));
    lastSectorUsed--;
    saveLogMetadata(lastSectorUsed);
    return;
  }

  // Strip off leading '!' or '#' if present
  char* p = buf;
  if (*p == '!' || *p == '#') p++;

  // Tokenize by commas: timestamp, lat, lon
  char ts[32];
  float lat, lon;
  char* tok = strtok(p, ",");
  if (!tok) { Serial.println(F("    Bad CSV, skipping.")); lastSectorUsed--; return; }
  strncpy(ts, tok, sizeof(ts));

  tok = strtok(NULL, ",");
  if (!tok) { Serial.println(F("    Missing lat, skipping.")); lastSectorUsed--; return; }
  lat = atof(tok);

  tok = strtok(NULL, ",");
  if (!tok) { Serial.println(F("    Missing lon, skipping.")); lastSectorUsed--; return; }
  lon = atof(tok);

  // Attempt the send
  Serial.print(F("    Retrying send: ")); Serial.print(ts);
  Serial.print(F(", ")); Serial.print(lat, 6);
  Serial.print(F(", ")); Serial.println(lon, 6);

  if (sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, lat, lon, ts)) {
    Serial.println(F("    SEND OK! Marking as sent."));
    consecutiveNetFails = 0;
  
    // Overwrite sector[0] = '#'
    PF.seek(s * 512UL);
    buf[0] = '#';
    memset(buf + br, 0, 512 - br);
    PF.writeFile(buf, 512, &br);
    PF.writeFile(nullptr, 0, &br);
    lastSectorUsed--;
  } else {
    consecutiveNetFails++;
    Serial.print(F("    SEND FAIL (#"));
    Serial.print(consecutiveNetFails);
    Serial.println(F("). Will retry same sector next time."));
  
    if (consecutiveNetFails >= NET_FAIL_THRESHOLD) {
      resetGsmModule();
      consecutiveNetFails = 0;
    }
  }

}




void logRealPositionToSd(float lat, float lon, const char* ts)
{
  char latStr[15], lonStr[15];
  dtostrf(lat, 4, 6, latStr);
  dtostrf(lon, 4, 6, lonStr);

  char line[64];
  uint16_t len = snprintf(line, sizeof(line), "!%s,%s,%s\n", ts, latStr, lonStr);

  // Check if current sector is reusable
  bool isReusable = false;
  if (PF.open(LOG_FILE) == FR_OK) {
    PF.seek(sectorIndex * 512UL);
    char firstByte;
    UINT br;
    if (PF.readFile(&firstByte, 1, &br) == FR_OK && br == 1 && firstByte == '#') {
      isReusable = true;
    }
  }

  // If not reusable and not new, skip
  if (!isReusable && sectorIndex <= lastSectorUsed) {
    Serial.println(F("Skipping log: sector not reusable yet."));
    return;
  }

  FRESULT res = PF.open(LOG_FILE);
  if (res != FR_OK) return;
  PF.seek(sectorIndex * 512UL);

  UINT bw;
  res = PF.writeFile(line, len, &bw);
  if (res != FR_OK || bw != len) {
    Serial.println(F("Write line failed."));
    return;
  }

  const uint8_t zeros[16] = {0};
  for (uint16_t i = len; i < 512; i += 16) {
    uint16_t chunk = (512 - i < 16) ? (512 - i) : 16;
    res = PF.writeFile(zeros, chunk, &bw);
    if (res != FR_OK || bw != chunk) {
      Serial.println(F("Zero pad write failed."));
      return;
    }
  }

  PF.writeFile(nullptr, 0, &bw);

  // Log verification
  uint32_t startS = (sectorIndex > HISTORY) ? sectorIndex - HISTORY : 1;
  for (uint32_t s = startS; s <= sectorIndex; s++) {
    res = PF.open(LOG_FILE);
    if (res != FR_OK) { Serial.print(F("PF.open verify err ")); Serial.println(res); return; }
    PF.seek(s * 512UL);
    char buf[64]; UINT br;
    res = PF.readFile(buf, len, &br);
    if (res != FR_OK) { Serial.println(F("readFile error")); return; }

    Serial.print(F("Sector ")); Serial.print(s); Serial.print(F(" read: "));
    Serial.write((uint8_t*)buf, br); Serial.println();
  }

  if (sectorIndex > lastSectorUsed) {
    lastSectorUsed = sectorIndex;
    saveLogMetadata(lastSectorUsed);  // ✅ mise à jour immédiate
  }

  sectorIndex++;
  if (sectorIndex > MAX_SECTORS) sectorIndex = 1;
}



// Lit la réponse série dans responseBuffer
void readSerialResponse(unsigned long waitMillis) {
  unsigned long start = millis();
  memset(responseBuffer, 0, RESPONSE_BUFFER_SIZE);
  responseBufferPos = 0;
  bool anythingReceived = false;
  while (millis() - start < waitMillis) {
    wdt_reset();
    while (moduleSerial.available()) {
      anythingReceived = true;
      if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
        char c = moduleSerial.read();
        if (c != '\0') {
          responseBuffer[responseBufferPos++] = c;
          responseBuffer[responseBufferPos] = '\0';
        }
      } else {
        moduleSerial.read(); // Jette le surplus
      }
    }
    if (!moduleSerial.available()) delay(5);
  }
  // Debug
  if (responseBufferPos > 0) {
    Serial.print(F("Rcvd: ["));
    for (uint8_t i = 0; i < responseBufferPos; i++) {
      char c = responseBuffer[i];
      if (c == '\r') continue;
      else if (c == '\n') Serial.print(F("<LF>"));
      else if (isprint(c)) Serial.print(c);
      else Serial.print('.');
    }
    Serial.println(F("]"));
  } else if (anythingReceived) {
    Serial.println(F("Rcvd: [Empty/Discarded]"));
  }
}

// Envoie une commande simple en attendant une réponse donnée
bool executeSimpleCommand(const char* command, const char* expectedResponse,
                          unsigned long timeoutMillis, uint8_t retries) {
  for (uint8_t i = 0; i < retries; i++) {
    wdt_reset();
    Serial.print(F("Send ["));
    Serial.print(i + 1);
    Serial.print(F("]: "));
    Serial.println(command);
    moduleSerial.println(command);
    readSerialResponse(timeoutMillis);
    if (strstr(responseBuffer, expectedResponse)) {
      Serial.println(F(">> OK Resp."));
      return true;
    }
    if (strstr(responseBuffer, "ERROR"))
      Serial.println(F(">> ERROR Resp."));
    Serial.println(F(">> No/Wrong Resp."));
    if (i < (retries - 1)) delay(500);
  }
  Serial.println(F(">> Failed after retries."));
  return false;
}

// Attends une réponse initiale OK
bool waitForInitialOK(uint8_t maxRetries) {
  for (uint8_t i = 0; i < maxRetries; i++) {
    Serial.print(F("AT (Try "));
    Serial.print(i + 1);
    Serial.print(F(")... "));
    moduleSerial.println("AT");
    readSerialResponse(1000UL);
    if (strstr(responseBuffer, "OK")) {
      Serial.println(F("OK."));
      return true;
    }
    Serial.println(F("No OK."));
    delay(500);
  }
  return false;
}

void resetGsmModule() {
  Serial.println(F("*** Power-cycling GSM module ***"));
  // Turn the module off
  digitalWrite(powerPin, LOW);
  delay(2000);
  clearSerialBuffer();

  // Turn it back on
  digitalWrite(powerPin, HIGH);
  delay(5000);

  // Re-init AT interface
  if (!waitForInitialOK(10)) {
    Serial.println(F("  ERROR: module still unresponsive after reset"));
  } else {
    Serial.println(F("  GSM module is back online"));
    executeSimpleCommand("ATE0", "OK", 1000, 2);
    executeSimpleCommand("AT+CMEE=2", "OK", 1000, 2);
    // (Re-attach PDP here if needed)
  }
}


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

// --- Setup
void setup() {
  // enable the watchdog with an 8-second timeout
  wdt_enable(WDTO_8S);
  
  setupSuccess = false;
  bool currentStepSuccess = true;

  Serial.begin(115200);
  while (!Serial) { ; }
  Serial.println(F("--- Arduino Initialized ---"));

  /* -------- SD card -------- */
  pinMode(SD_CS_PIN, OUTPUT);
  SPI.begin();
  if (PF.begin(&fs) != FR_OK)
    Serial.println(F("Failed to mount SD"));
  else
    Serial.println(F("SD mounted"));

  sectorIndex = loadLogMetadata();
  Serial.print(F("Resuming at sector ")); Serial.println(sectorIndex);

  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
  Serial.println(F("Module power pin configured (D2)."));

  moduleSerial.begin(moduleBaudRate);
  Serial.print(F("Software Serial initialized on Pins RX:"));
  Serial.print(swRxPin);
  Serial.print(F(", TX:"));
  Serial.print(swTxPin);
  Serial.print(F(" at "));
  Serial.print(moduleBaudRate);
  Serial.println(F(" baud."));

  Serial.println(F("Turning module ON..."));
  digitalWrite(powerPin, HIGH);
  delay(5000);
  Serial.println(F("Module boot wait complete."));

  Serial.println(F("Attempting initial communication..."));
  if (!waitForInitialOK(15)) {
    Serial.println(F("FATAL: Module unresponsive."));
    currentStepSuccess = false;
  } else {
    Serial.println(F("Initial communication OK."));
    executeSimpleCommand("ATE0", "OK", 1000UL, 2);
    executeSimpleCommand("AT+CMEE=2", "OK", 1000UL, 2);
  }

  // --- STEP 1 : Network Settings
  if (currentStepSuccess) {
    Serial.println(F("\n=== STEP 1: Network Settings ==="));
    if (!executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 2000UL, 2)) {
      Serial.println(F("WARNING: CIPSHUT failed. Continuing anyway..."));
    }
    if (!executeSimpleCommand("AT+CFUN=0", "OK", 1000UL, 1)) {
      Serial.println(F("WARNING: CFUN=0 failed. Continuing..."));
    }
    currentStepSuccess &= executeSimpleCommand("AT+CNMP=38", "OK", 500UL, 3);
    currentStepSuccess &= executeSimpleCommand("AT+CMNB=1", "OK", 500UL, 3);
  }
  if (currentStepSuccess) {
    Serial.println(F("Turning radio ON (CFUN=1,1)..."));
    moduleSerial.println("AT+CFUN=1,1");
  } else {
    Serial.println(F("Skipping CFUN=1."));
  }
  if (currentStepSuccess) {
    const unsigned long rebootWaitMillis = 500UL;
    Serial.print(F("Waiting "));
    Serial.print(rebootWaitMillis / 1000);
    Serial.println(F("s for reboot..."));
    unsigned long startWait = millis();
    while (millis() - startWait < rebootWaitMillis) { readSerialResponse(100UL); delay(100); }
    Serial.println(F("Attempting communication post-reboot..."));
    if (!waitForInitialOK(15)) {
      Serial.println(F("ERROR: Module unresponsive post-reboot."));
      currentStepSuccess = false;
    } else {
      Serial.println(F("Communication OK post-reboot."));
      executeSimpleCommand("ATE0", "OK", 2000UL, 2);
    }
  }

  // --- STEP 2 : Network Registration
  if (currentStepSuccess) {
    Serial.println(F("\n=== STEP 2: Network Registration ==="));
    bool registered = false;
    const uint8_t maxRegRetries = 20;
    for (uint8_t i = 0; i < maxRegRetries && currentStepSuccess; i++) {
      Serial.print(F("Reg check "));
      Serial.print(i + 1);
      Serial.println(F("..."));
      if (!executeSimpleCommand("AT+CPIN?", "READY", 3000UL, 1)) {
        Serial.println(F("ERROR: SIM not READY."));
        currentStepSuccess = false;
        break;
      }
      executeSimpleCommand("AT+CSQ", "+CSQ", 500UL, 1);
      executeSimpleCommand("AT+COPS?", "+COPS", 3000UL, 1);
      bool creg_ok = false, cereg_ok = false;
      executeSimpleCommand("AT+CREG?", "+CREG:", 500UL, 1);
      if (strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5"))
        creg_ok = true;
      executeSimpleCommand("AT+CEREG?", "+CEREG:", 500UL, 1);
      if (strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5"))
        cereg_ok = true;
      if (creg_ok || cereg_ok) {
        Serial.println(F("Registered."));
        registered = true;
        break;
      } else {
        Serial.println(F("Not registered yet..."));
        delay(2000);
      }
    }
    if (!registered) {
      Serial.println(F("ERROR: Failed network registration."));
      currentStepSuccess = false;
    }
  }

  // --- STEP 3 : PDP Context
  if (currentStepSuccess) {
    Serial.println(F("\n=== STEP 3: PDP Context ==="));
    snprintf(responseBuffer, RESPONSE_BUFFER_SIZE, "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
    currentStepSuccess &= executeSimpleCommand(responseBuffer, "OK", 500UL, 3);
    if (currentStepSuccess) {
      currentStepSuccess &= executeSimpleCommand("AT+CGATT=1", "OK", 500UL, 3);
    }
    if (currentStepSuccess) {
      Serial.println(F("Activating PDP (AT+CGACT=1,1)..."));
      bool activationAttempted = false;
      if (executeSimpleCommand("AT+CGACT=1,1", "OK", 500UL, 1)) {
        Serial.println(F("OK received. Waiting..."));
        activationAttempted = true;
        delay(2000);
      } else {
        if (strstr(responseBuffer, "ERROR")) {
          Serial.println(F("CGACT ERROR."));
          if (strstr(responseBuffer, "148"))
            activationAttempted = true;
          else
            currentStepSuccess = false;
        } else {
          Serial.println(F("CGACT Timeout?"));
          currentStepSuccess = false;
        }
      }
      if (activationAttempted && currentStepSuccess) {
        Serial.println(F("Verifying PDP (AT+CGACT?)..."));
        bool contextActive = false;
        for (uint8_t i = 0; i < 3 && !contextActive; i++) {
          if (executeSimpleCommand("AT+CGACT?", "+CGACT:", 500UL, 1)) {
            if (strstr(responseBuffer, "+CGACT: 1,1")) {
              Serial.println(F("PDP Active."));
              contextActive = true;
              executeSimpleCommand("AT+CGPADDR=1", "+CGPADDR:", 500UL, 1);
            } else {
              Serial.println(F("PDP not active."));
            }
          } else {
            Serial.println(F("No response CGACT?."));
          }
          if (!contextActive && i < 2) delay(2000);
        }
        if (!contextActive) {
          Serial.println(F("ERROR: Failed PDP verification."));
          currentStepSuccess = false;
        }
      } else if (!activationAttempted) {
        Serial.println(F("Skipping PDP verify."));
      }
    }
  }

  // --- STEP 4 : Enable GNSS
  if (currentStepSuccess) {
    Serial.println(F("\n=== STEP 4: Enable GNSS ==="));
    if (executeSimpleCommand("AT+CGNSPWR=1", "OK", 500UL, 3)) {
      Serial.println(F("GNSS Power Enabled."));
      delay(1000);
    } else {
      Serial.println(F("ERROR: Failed to enable GNSS!"));
      currentStepSuccess = false;
    }
  }

  if (currentStepSuccess) {
    Serial.println(F("\n=== SETUP COMPLETE ==="));
    setupSuccess = true;
  } else {
    Serial.println(F("\n--- SETUP FAILED ---"));
    setupSuccess = false;
  }
  for (uint8_t i = 0; i < 5 && lastSectorUsed > 0; i++) {
    resendLastLog();  // flush up to 5 entries max
    wdt_reset();
  }
  Serial.println(F("Entering main loop..."));
  lastSendTime = millis();
}

/* ============================= LOOP =========================== */
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

  readSerialResponse(50UL);
  if (responseBufferPos > 0) {
    if (!strstr(responseBuffer, "+CSQ:") && !strstr(responseBuffer, "+CREG:") &&
        !strstr(responseBuffer, "+CEREG:") && !strstr(responseBuffer, "+CGNSINF:")) {
      Serial.print(F("Idle URC?: "));
      Serial.println(responseBuffer);
    }
  }
  delay(100);
  wdt_reset();
}
