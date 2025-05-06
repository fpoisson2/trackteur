#include <SoftwareSerial.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------
   Pin configuration
   --------------------------------------------------*/
const uint8_t PWR_PIN = 2;     // HIGH → Power on module
const uint8_t RX_PIN  = 3;     // Arduino RX  ←  Module TX
const uint8_t TX_PIN  = 4;     // Arduino TX  →  Module RX
SoftwareSerial modem(RX_PIN, TX_PIN);

/* --------------------------------------------------
   User parameters
   --------------------------------------------------*/
const char APN[]            = "onomondo";
const char TRACCAR_HOST[]   = "trackteur.ve2fpd.com";
const uint16_t TRACCAR_PORT = 5055;
const char DEVICE_ID[]      = "212910";

/* Dummy position – replace with GPS */
const float dummyLat = 46.8139;
const float dummyLon = -71.2082;

/* Send period (ms) */
const unsigned long SEND_PERIOD_MS = 5000UL;

/* Retry parameters */
const uint8_t MAX_RETRIES = 3;
const unsigned long RETRY_DELAY_MS = 2000;

/* Response buffer */
#define RES_BUF 256
char resBuf[RES_BUF];
uint16_t resPos = 0;

/* State */
unsigned long lastSend    = 0;
bool modemReady           = false;
volatile bool registered  = false;

/* --------------------------------------------------
   Function declarations
   --------------------------------------------------*/
bool sendAT(const char *cmd, const char *expect, unsigned long timeout = 2000);
void flushRx();
void collect(unsigned long ms);
bool isRegistered(const char *buf);
bool waitReg(unsigned long ms);
bool netOpen();
bool tcpOpen();
bool tcpSend(float lat, float lon);
void netClose();
bool checkSIM();

bool getGps(float &lat, float &lon)
{
  flushRx();
  modem.println("AT+CGPSINFO");

  unsigned long t0 = millis();
  while (millis() - t0 < 3000UL) {
    if (modem.available()) break;
  }

  // Lis jusqu'à 256 caractères ou timeout
  t0 = millis();
  resPos = 0;
  while (millis() - t0 < 1000UL && resPos < RES_BUF - 1) {
    if (modem.available()) {
      char c = modem.read();
      if (c >= 32 && c <= 126) {  // ASCII printable
        resBuf[resPos++] = c;
        resBuf[resPos] = 0;
      }
    }
  }

  const char* p = strstr(resBuf, "+CGPSINFO:");
  if (!p) {
    Serial.println(F("ERROR: No +CGPSINFO in response"));
    Serial.print(F("RECV: ")); Serial.println(resBuf);
    return false;
  }

  char buf[128];
  strncpy(buf, p + 11, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char* eol = strpbrk(buf, "\r\n");
  if (eol) *eol = '\0';

  // Token parsing
  char* tok = strtok(buf, ",");
  if (!tok || strlen(tok) < 3) return false;
  float latRaw = atof(tok);

  tok = strtok(nullptr, ",");  if (!tok) return false;
  char ns = *tok;

  tok = strtok(nullptr, ",");  if (!tok || strlen(tok) < 3) return false;
  float lonRaw = atof(tok);

  tok = strtok(nullptr, ",");  if (!tok) return false;
  char ew = *tok;

  if (latRaw == 0.0 || lonRaw == 0.0) {
    Serial.println(F("No fix yet"));
    return false;
  }

  // DMM → DD
  int degLat = (int)(latRaw / 100);
  float minLat = latRaw - degLat * 100;
  lat = degLat + minLat / 60.0f;

  int degLon = (int)(lonRaw / 100);
  float minLon = lonRaw - degLon * 100;
  lon = degLon + minLon / 60.0f;

  if (ns == 'S') lat = -lat;
  if (ew == 'W') lon = -lon;

  Serial.print(F("Parsed Lat: ")); Serial.print(lat, 6);
  Serial.print(F(" Lon: ")); Serial.println(lon, 6);
  return true;
}


bool step4EnableGNSS()
{
  Serial.println(F("Enabling GNSS ..."));
  // Si module A7670E → AT+CGPS=1
  if (!sendAT("AT+CGNSSPWR=1", "OK", 2000)) {
    Serial.println(F("ERROR: A7670E GNSS start failed"));
    return false;
  }
  return true;
}


/* ==================================================
   Arduino SETUP
   ==================================================*/
void setup()
{
  Serial.begin(9600);
  while (!Serial);

  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  modem.begin(9600);

  Serial.println(F("Booting A7670E ..."));
  digitalWrite(PWR_PIN, HIGH);
  delay(5000); // Module boot

  // Robust startup with retries
  for (uint8_t i = 0; i < MAX_RETRIES; i++) {
    if (sendAT("AT", "OK", 2000)) break;
    Serial.println(F("... waiting for AT OK"));
    if (i == MAX_RETRIES - 1) {
      Serial.println(F("Modem init failed"));
      return;
    }
    delay(RETRY_DELAY_MS);
  }

  sendAT("ATE0", "OK");                     // Echo OFF
  sendAT("AT+CMEE=2", "OK");                // Verbose errors

  // Check SIM card
  if (!checkSIM()) {
    Serial.println(F("SIM card not detected"));
    return;
  }

  // Reset data stack with retries
  for (uint8_t i = 0; i < MAX_RETRIES; i++) {
    if (sendAT("AT+CIPSHUT", "SHUT OK", 5000)) break;
    Serial.println(F("Retrying CIPSHUT..."));
    if (i == MAX_RETRIES - 1) {
      Serial.println(F("CIPSHUT failed"));
      // Continue, as stack may still be usable
    }
    delay(RETRY_DELAY_MS);
  }

  sendAT("AT+CFUN=0", "OK", 3000);
  delay(3000);
  sendAT("AT+CFUN=1", "OK", 3000);
  delay(3000);

  // Configure APN
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0", APN);
  sendAT(cmd, "OK");

  sendAT("AT+CEREG=2", "OK");               // Verbose URC

  if (!waitReg(30000)) {
    Serial.println(F("Network registration failed"));
    return;
  }

  if (!netOpen()) {
    Serial.println(F("Network open failed"));
    return;
  }

if (!step4EnableGNSS()) {
  Serial.println(F("GNSS init failed"));
  return;
}

  modemReady = true;
  lastSend = millis();
}

/* ==================================================
   Arduino LOOP
   ==================================================*/
void loop()
{
  collect(50);                              // Process URCs

  if (modemReady && millis() - lastSend >= SEND_PERIOD_MS) {
    bool success = false;
    for (uint8_t i = 0; i < MAX_RETRIES; i++) {
      if (tcpOpen()) {
        float lat, lon;
        if (getGps(lat, lon) && tcpSend(lat, lon)) {
          Serial.println(F("Traccar packet sent ✔"));
          success = true;
          break;
        } else {
          Serial.println(F("Traccar packet failed ✘"));
        }
      }
      Serial.print(F("Retry TCP connection/send #")); Serial.println(i + 1);
      netClose();
      delay(RETRY_DELAY_MS);
    }
    
    if (!success) Serial.println(F("All TCP send retries failed"));
    
    netClose();
    lastSend = millis();
  }
}

/* ==================================================
   Utility functions
   ==================================================*/

/* Send AT command and wait for expected response */
bool sendAT(const char *cmd, const char *expect, unsigned long timeout)
{
  flushRx();
  modem.println(cmd);

  unsigned long t0 = millis();
  bool ok = false;
  while (millis() - t0 < timeout) {
    collect(10);
    if (strstr(resBuf, expect)) {
      ok = true;
      break;
    }
  }

  Serial.print(F("AT> ")); Serial.print(cmd);
  Serial.print(F(" → ")); Serial.println(ok ? F("OK") : F("FAIL"));
  return ok;
}

/* Clear RX and reset buffer */
void flushRx()
{
  while (modem.available()) modem.read();
  memset(resBuf, 0, sizeof(resBuf));
  resPos = 0;
}

/* Collect received data and update flags */
void collect(unsigned long ms)
{
  static char lastPrinted[RES_BUF] = {0}; // Track last printed response
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    if (!modem.available()) continue;
    char c = modem.read();
    if (c < 0x20 || c > 0x7E) continue; // Skip non-printable characters
    if (resPos < RES_BUF - 1) {
      resBuf[resPos++] = c;
      resBuf[resPos] = 0;
    }

    if (isRegistered(resBuf)) registered = true;
  }

  // Anti-spam filter
  if (resPos == 0) return;

  // Skip common responses to avoid repetition
  if (strcmp(resBuf, "OK\r\n") == 0) return;
  if (strcmp(resBuf, "ERROR\r\n") == 0) return;
  if (strstr(resBuf, "IP ERROR")) return;
  if (strncmp(resBuf, "+NETOPEN:", 9) == 0) return;
  if (strncmp(resBuf, "+NETCLOSE:", 10) == 0) return;

  // Only print if different from last printed response
  if (strcmp(resBuf, lastPrinted) != 0) {
    Serial.print(F("<< "));
    Serial.write(resBuf, resPos);
    Serial.println();
    strncpy(lastPrinted, resBuf, RES_BUF); // Update last printed
  }
}

/* Parse +CEREG response */
bool isRegistered(const char *buf)
{
  const char *p = strstr(buf, "+CEREG:");
  if (!p) return false;

  int n, stat;
  if (sscanf(p, "+CEREG: %d,%d", &n, &stat) == 2) {
    return stat == 1 || stat == 5;          // 1 = home, 5 = roaming
  }
  return false;
}

/* Check SIM card presence */
bool checkSIM()
{
  for (uint8_t i = 0; i < MAX_RETRIES; i++) {
    flushRx();
    modem.println("AT+CPIN?");
    collect(2000);
    if (strstr(resBuf, "+CPIN: READY")) {
      Serial.println(F("SIM card ready"));
      return true;
    }
    Serial.println(F("SIM check failed, retrying..."));
    delay(RETRY_DELAY_MS);
  }
  return false;
}

/* Wait for LTE registration */
bool waitReg(unsigned long ms)
{
  Serial.println(F("Waiting for network ..."));
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    if (registered) {
      Serial.println(F("Registered via URC ✔"));
      return true;
    }
    sendAT("AT+CEREG?", "+CEREG", 2000);
    if (isRegistered(resBuf)) {
      Serial.println(F("Registered via polling ✔"));
      return true;
    }
    delay(2000);
  }
  Serial.println(F("No service"));
  return false;
}

/* Activate PDP/socket stack with retries */
bool netOpen()
{
  for (uint8_t i = 0; i < MAX_RETRIES; i++) {
    if (sendAT("AT+NETOPEN", "+NETOPEN: 0", 60000)) {
      Serial.println(F("NETOPEN OK"));
      return true;
    }

    if (strstr(resBuf, "already opened")) {
      Serial.println(F("NETOPEN says 'already opened' → forcing NETCLOSE"));
    } else {
      Serial.println(F("NETOPEN failed → trying NETCLOSE"));
    }

    sendAT("AT+NETCLOSE", "+NETCLOSE:", 30000);
    delay(RETRY_DELAY_MS);
  }

  Serial.println(F("NETOPEN failed after retries"));
  return false;
}

/* Open TCP socket to Traccar */
bool tcpOpen()
{
  char cmd[96];
  snprintf(cmd, sizeof(cmd),
           "AT+CIPOPEN=0,\"TCP\",\"%s\",%u",
           TRACCAR_HOST, TRACCAR_PORT);
  return sendAT(cmd, "+CIPOPEN: 0,0", 30000);
}

/* Send OsmAnd-style GET request */
bool tcpSend(float lat, float lon)
{
  char req[200], latStr[16], lonStr[16];
  dtostrf(lat, 0, 6, latStr);
  dtostrf(lon, 0, 6, lonStr);
  snprintf(req, sizeof(req),
           "GET /?id=%s&lat=%s&lon=%s HTTP/1.1\r\n"
           "Host: %s\r\nConnection: close\r\n\r\n",
           DEVICE_ID, latStr, lonStr, TRACCAR_HOST);

  int len = strlen(req);
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%d", len);
  if (!sendAT(cmd, ">", 3000)) return false;

  flushRx();
  modem.print(req);
  modem.write(0x1A);                        // Ctrl-Z
  collect(8000);

  return strstr(resBuf, "+CIPSEND: 0") && strstr(resBuf, ",");
}

/* Close socket with timeout */
void netClose()
{
  sendAT("AT+CIPCLOSE=0", "+CIPCLOSE: 0", 10000);
  delay(3000);
}
