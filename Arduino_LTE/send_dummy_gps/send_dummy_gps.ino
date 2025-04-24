#include <SoftwareSerial.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------
   Brochage – adapte si besoin
   --------------------------------------------------*/
const uint8_t PWR_PIN = 2;     // HIGH → mise sous tension du module
const uint8_t RX_PIN  = 3;     // Arduino RX  ←  Module TX
const uint8_t TX_PIN  = 4;     // Arduino TX  →  Module RX
SoftwareSerial modem(RX_PIN, TX_PIN);

/* --------------------------------------------------
   Paramètres utilisateur
   --------------------------------------------------*/
const char APN[]            = "mobile.bm";
const char TRACCAR_HOST[]   = "trackteur.ve2fpd.com";
const uint16_t TRACCAR_PORT = 5055;
const char DEVICE_ID[]      = "212910";

/* Position factice – remplace par ton GPS */
const float dummyLat = 46.8139;
const float dummyLon = -71.2082;

/* Périodicité d’envoi (ms) */
const unsigned long SEND_PERIOD_MS = 5000UL;

/* Buffer circulaire pour les réponses */
#define RES_BUF 256
char  resBuf[RES_BUF];
uint16_t resPos = 0;

/* État */
unsigned long lastSend    = 0;
bool modemReady           = false;
volatile bool registered  = false;

/* --------------------------------------------------
   Déclarations anticipées
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
  delay(5000);                              // laisse le temps de booter

  /* On boucle jusqu’à ce que “AT” réponde “OK” */
  while (!sendAT("AT", "OK", 2000)) {
    Serial.println(F("... toujours en attente de AT OK"));
    delay(500);
  }

  sendAT("ATE0", "OK");                     // écho OFF
  sendAT("AT+CMEE=2", "OK");                // erreurs verbeuses

  char cmd[64];
  snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
  sendAT(cmd, "OK");

  sendAT("AT+CEREG=2", "OK");               // URC +CEREG détaillé

  /* Attacher LTE */
  if (!waitReg(30000)) return;              // 30 s maxi

  /* Activer la pile socket */
  if (!netOpen())   return;

  modemReady = true;
  lastSend   = millis();
}

/* ==================================================
   Arduino LOOP
   ==================================================*/
void loop()
{
  collect(50);                              // écouler les URC

  if (modemReady && millis() - lastSend >= SEND_PERIOD_MS) {

    if (tcpOpen()) {
      if (tcpSend(dummyLat, dummyLon)) {
        Serial.println(F("Traccar packet sent ✔"));
      } else {
        Serial.println(F("Traccar packet failed ✘"));
      }
    }
    netClose();                             // on ferme puis on attend 3 s
    lastSend = millis();
  }
}

/* ==================================================
   Fonctions utilitaires
   ==================================================*/

/* Envoi d’une commande AT et attente de la chaîne `expect` */
bool sendAT(const char *cmd, const char *expect, unsigned long timeout)
{
  flushRx();
  modem.println(cmd);

  unsigned long t0 = millis();
  bool ok = false;
  while (millis() - t0 < timeout) {
    collect(10);                            // lit un petit paquet
    if (strstr(resBuf, expect)) {           // trouvé !
      ok = true;
      break;
    }
  }

  Serial.print(F("AT> ")); Serial.print(cmd);
  Serial.print(F(" → ")); Serial.println(ok ? F("OK") : F("FAIL"));
  return ok;
}

/* Vidange RX + remise à zéro du buffer */
void flushRx()
{
  while (modem.available()) modem.read();
  memset(resBuf, 0, sizeof(resBuf));
  resPos = 0;
}

/* Collecte des données reçues et mise à jour des flags */
void collect(unsigned long ms)
{
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    if (!modem.available()) continue;
    char c = modem.read();
    if (resPos < RES_BUF - 1) {
      resBuf[resPos++] = c;
      resBuf[resPos] = 0;
    }

    if (isRegistered(resBuf)) registered = true;
  }

  /* ——— filtre anti-spam ——— */
  if (resPos == 0) return;

  if (strcmp(resBuf, "OK\r\n") == 0)            { /* ignore */ return; }
  if (strcmp(resBuf, "ERROR\r\n") == 0)         { /* ignore */ return; }
  if (strstr(resBuf, "IP ERROR"))               { /* ignore */ return; }
  if (strncmp(resBuf, "+NETOPEN:", 9) == 0)     { /* ignore */ return; }
  if (strncmp(resBuf, "+NETCLOSE:", 10) == 0)   { /* ignore */ return; }

  Serial.print(F("<< "));
  Serial.write(resBuf, resPos);
  Serial.println();
}

/* Parse strict du +CEREG */
bool isRegistered(const char *buf)
{
  /* On cherche le début exact de “+CEREG:” dans le buffer */
  const char *p = strstr(buf, "+CEREG:");
  if (!p) return false;

  int n, stat;
  /*           +CEREG: <n> , <stat>   */
  if (sscanf(p, "+CEREG: %d,%d", &n, &stat) == 2) {
    return stat == 1 || stat == 5;          // 1 = home, 5 = roaming
  }
  return false;
}

/* Attente de l’attach LTE */
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

/* Activation PDP / pile socket */
bool netOpen()
{
  /* 1) Essai direct -------------------------------------------------- */
  if (sendAT("AT+NETOPEN", "+NETOPEN: 0", 60000)) {
    Serial.println(F("NETOPEN OK"));
    return true;
  }

  /* 2) Si “already opened”, on ferme quand même puis on retente ------- */
  if (strstr(resBuf, "already opened")) {
    Serial.println(F("NETOPEN dit « already opened » → on force NETCLOSE"));
  } else {
    Serial.println(F("NETOPEN a échoué → tentative de NETCLOSE"));
  }

  /* On ferme proprement (30 s max, ignore le résultat) */
  sendAT("AT+NETCLOSE", "+NETCLOSE:", 30000);

  /* 3) Nouvelle tentative NETOPEN ------------------------------------ */
  if (sendAT("AT+NETOPEN", "+NETOPEN: 0", 60000)) {
    Serial.println(F("NETOPEN OK après reset"));
    return true;
  }

  Serial.println(F("Échec NETOPEN même après reset PDP"));
  return false;
}

/* Ouverture du socket TCP vers Traccar */
bool tcpOpen()
{
  char cmd[96];
  snprintf(cmd, sizeof(cmd),
           "AT+CIPOPEN=0,\"TCP\",\"%s\",%u",
           TRACCAR_HOST, TRACCAR_PORT);
  return sendAT(cmd, "+CIPOPEN: 0,0", 30000);
}

/* Envoi du GET “style OsmAnd” */
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

/* Fermeture du socket + temporisation fixe */
void netClose()
{
  sendAT("AT+CIPCLOSE=0", "+CIPCLOSE: 0", 10000);
  delay(3000);            // le socket repasse en “INITIAL” en ≤ 3 s
}
