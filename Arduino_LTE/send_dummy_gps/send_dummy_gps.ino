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
        if (tcpSend(dummyLat, dummyLon)) {
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
