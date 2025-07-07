/**
 * @file      Traccar_GPS_stable_sleep.ino (verbose init + report intelligent)
 * @author    Lewis He (adapted by Francis)
 * @brief     Uploads GPS position to Traccar with robust acquisition, detailed modem init,
 *            reporting every 30s when moving, every 15 min if stopped, and battery reading.
 */

// ─────────────── Options de trace
#define DEBUG_SKETCH              // commente pour prod
// #define DUMP_AT_COMMANDS        // affiche tous les AT envoyés

// ─────────────── Includes
#include <Arduino.h>
#include "utilities.h"
#include <TinyGsmClient.h>
#include <TinyGPS++.h>
#include "esp_sleep.h"

// ─────────────── Constantes
#define TINY_GSM_RX_BUFFER 1024
#define NETWORK_APN        "hologram"
#define GPS_TIMEOUT_SEC    90ul      // timeout acquisition GPS
#define MODEM_RETRY_AT     5         // essais AT avant PWRKEY

static const char *TRACKCAR_URL = "https://serveur1a.trackteur.cc";
static const char *DEVICE_ID    = "212910";
static const char *POST_FMT     =
  "deviceid=%s&lat=%.7f&lon=%.7f&timestamp=%s&altitude=%.2f&speed=%.2f&bearing=%.2f&hdop=%.2f&batt=%u";

// Pins
#ifndef SerialGPS
#define SerialGPS Serial2
#endif
#define GPS_TX_PIN 21
#define GPS_RX_PIN 22
#ifndef MODEM_DTR_PIN
#define MODEM_DTR_PIN 25
#endif
#ifndef BOARD_LED_PIN
#define BOARD_LED_PIN 2
#endif

// Log macro
#if defined(DEBUG_SKETCH)
  #define LOG(x)  do{ Serial.println(x);}while(0)
#else
  #define LOG(x)
#endif

// Globals
#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, Serial);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

TinyGPSPlus gps;
GPSInfo     lastFix = {0};

// Timers pour la logique intelligente
unsigned long lastMovingReport = 0;
unsigned long lastStillReport = 0;
const uint32_t MOVING_REPORT_RATE_MS = 30ul * 1000;         // 30 s en mouvement
const uint32_t STILL_CHECK_RATE_MS   = 60ul * 1000;         // check chaque 1 min si immobile
const uint32_t STILL_REPORT_RATE_MS  = 15ul * 60ul * 1000;  // rapport toutes les 15 min si immobile
bool wasMoving = false;

// Mesure batterie
#ifdef BOARD_BAT_ADC_PIN
#include <vector>
#include <algorithm>
#include <numeric>
uint32_t getBatteryVoltage()
{
    std::vector<uint32_t> data;
    for (int i = 0; i < 30; ++i) {
        uint32_t val = analogReadMilliVolts(BOARD_BAT_ADC_PIN);
        data.push_back(val);
        delay(30);
    }
    std::sort(data.begin(), data.end());
    data.erase(data.begin());
    data.pop_back();
    int sum = std::accumulate(data.begin(), data.end(), 0);
    double average = static_cast<double>(sum) / data.size();
    return  average * 2; // x2 car pont diviseur
}

uint8_t voltageToPercent(uint32_t mv)
{
    if (mv >= 4200) return 100;
    if (mv <= 3500) return 0;
    return (uint8_t)((mv - 3500) * 100 / 700);
}
#endif

// Helpers
String isoTime(const GPSInfo &g)
{
  char buf[30];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", g.year, g.month, g.day, g.hour, g.minute, g.second);
  return String(buf);
}

bool httpsPost(const GPSInfo &g)
{
  uint8_t battPct = 100;
#ifdef BOARD_BAT_ADC_PIN
  uint32_t battMv = getBatteryVoltage();
  battPct = voltageToPercent(battMv);
  Serial.printf("Battery: %u mV = %u%%\n", battMv, battPct);
#endif

  char body[256];
  snprintf(body, sizeof(body), POST_FMT, DEVICE_ID, g.latitude, g.longitude,
           isoTime(g).c_str(), g.altitude, g.speed, g.course, g.HDOP, battPct);
  LOG(F("POST: ")); Serial.println(body);

  modem.https_begin();
  if (!modem.https_set_url(TRACKCAR_URL)) {
    LOG(F("URL fail"));
    modem.https_end();
    return false;
  }
  modem.https_set_user_agent("TinyGSM/LilyGo");
  int code = modem.https_post(body);
  modem.https_end();
  LOG(F("HTTP: ")); Serial.println(code);
  return (code == 200);
}

bool waitGpsFix(GPSInfo &out)
{
  unsigned long start = millis();
  while (millis() - start < GPS_TIMEOUT_SEC * 1000ul) {
    while (SerialGPS.available()) gps.encode(SerialGPS.read());
    if (gps.location.isValid()) {
      out.isFix     = true;
      out.latitude  = gps.location.lat();
      out.longitude = gps.location.lng();
      out.altitude  = gps.altitude.isValid() ? gps.altitude.meters() : 0;
      out.speed     = gps.speed.isValid()    ? gps.speed.kmph()      : 0;
      out.course    = gps.course.isValid()   ? gps.course.deg()      : 0;
      out.HDOP      = gps.hdop.isValid()     ? gps.hdop.value() / 100. : 99.99;
      out.year   = gps.date.isValid() ? gps.date.year()  : 1970;
      out.month  = gps.date.isValid() ? gps.date.month() : 1;
      out.day    = gps.date.isValid() ? gps.date.day()   : 1;
      out.hour   = gps.time.isValid() ? gps.time.hour()  : 0;
      out.minute = gps.time.isValid() ? gps.time.minute(): 0;
      out.second = gps.time.isValid() ? gps.time.second(): 0;
      LOG(F("✔ GPS fix"));
      return true;
    }
    delay(200);
  }
  LOG(F("✖ GPS timeout"));
  return false;
}

void enterSleep(uint32_t ms)
{
  digitalWrite(BOARD_LED_PIN, LOW);
  pinMode(MODEM_DTR_PIN, OUTPUT); digitalWrite(MODEM_DTR_PIN, HIGH);
  modem.sleepEnable(true);

  LOG("→ ESP light-sleep …");
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();

  digitalWrite(MODEM_DTR_PIN, LOW);
  modem.sleepEnable(false);
  digitalWrite(BOARD_LED_PIN, HIGH);
  delay(500);
}

// Setup
void setup()
{
  #ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
  #endif

  pinMode(BOARD_LED_PIN, OUTPUT); digitalWrite(BOARD_LED_PIN, HIGH);
  Serial.begin(115200);
  LOG("Booting …");

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH); delay(100); digitalWrite(BOARD_PWRKEY_PIN, LOW);

  Serial.print("Start modem");
  uint8_t retry = 0;
  while (!modem.testAT(1000)) {
    Serial.print('.');
    if (++retry > MODEM_RETRY_AT) {
      digitalWrite(BOARD_PWRKEY_PIN, HIGH); delay(600);
      digitalWrite(BOARD_PWRKEY_PIN, LOW);
      retry = 0;
    }
  }
  Serial.println();

  String modemName;
  do {
    modemName = modem.getModemName();
    if (modemName == "UNKOWN") {
      Serial.println("Modem name unknown — retry");
      delay(500);
    }
  } while (modemName == "UNKOWN");
  Serial.print("Model Name: "); Serial.println(modemName);

  SimStatus sim = SIM_ERROR;
  Serial.print("Checking SIM");
  while ((sim = modem.getSimStatus()) != SIM_READY) {
    Serial.print('.'); delay(500);
  }
  Serial.println(" → SIM READY");

  Serial.printf("Setting APN to %s\n", NETWORK_APN);
  modem.sendAT(GF("+CGDCONT=1,\"IP\",\"" NETWORK_APN "\"")); modem.waitResponse();

  Serial.print("Registering network");
  RegStatus reg;
  while ((reg = modem.getRegistrationStatus()) != REG_OK_HOME && reg != REG_OK_ROAMING) {
    int16_t sq = modem.getSignalQuality();
    Serial.printf(" — SQ=%d\n", sq);
    delay(1000);
  }
  Serial.println(" → REGISTERED");

  if (!modem.setNetworkActive()) {
    Serial.println("Enable data context failed.");
  }

  String ip = modem.getLocalIP();
  Serial.print("IP Address: "); Serial.println(ip);

  LOG("Initialisation terminée ✓");
}

// Loop principal
void loop()
{
  static bool everReportedStill = false;
  unsigned long now = millis();

  // 1. Fix GPS
  GPSInfo g = {0};
  bool gotFix = waitGpsFix(g);

  if (!gotFix) {
    LOG("No GPS fix, sleep.");
    enterSleep(10000);
    return;
  }

  // 2. Mouvement ?
  bool isMoving = (g.speed >= 1.0);

  if (isMoving) {
    // Rapport toutes les 30s
    if (!wasMoving || (now - lastMovingReport > MOVING_REPORT_RATE_MS)) {
      if (httpsPost(g)) {
        lastFix = g;
        lastMovingReport = now;
        wasMoving = true;
        everReportedStill = false;
        LOG("Moving: report sent, sleep 30s.");
      } else {
        LOG("Moving: report fail, retry 3s");
        delay(3000);
      }
    }
    enterSleep(MOVING_REPORT_RATE_MS);
  } else {
    // Rapport toutes les 15 min, check chaque 1 min
    if (!everReportedStill || (now - lastStillReport > STILL_REPORT_RATE_MS)) {
      if (httpsPost(g)) {
        lastFix = g;
        lastStillReport = now;
        everReportedStill = true;
        LOG("Still: heartbeat sent (15min), sleep 1min.");
      } else {
        LOG("Still: heartbeat fail, retry 3s.");
        delay(3000);
      }
    } else {
      LOG("Still: no report (wait next 15min), sleep 1min.");
    }
    wasMoving = false;
    enterSleep(STILL_CHECK_RATE_MS);
  }
}

#ifndef TINY_GSM_FORK_LIBRARY
#error "Install TinyGSM fork per LilyGO instructions"
#endif
