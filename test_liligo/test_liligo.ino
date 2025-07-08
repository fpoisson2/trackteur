/**
 * @file      Traccar.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2025  ShenZhen XinYuan Electronic Technology Co., Ltd
 * @date      2025-03-25
 * @note      This demo uploads the location obtained by the device to [traccar](https://www.traccar.org/).
 *            For more information about [traccar](https://www.traccar.org/), please visit the official website.
 *            This demo only demonstrates uploading the location.
 */



#ifndef SerialGPS
#define SerialGPS Serial2
#endif



#define TINY_GSM_RX_BUFFER          1024 // Set RX buffer to 1Kb

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// If defined, cancel shallow sleep and use delay to replace shallow sleep
// #define DEBUG_SKETCH

// For external LED connection to observe status
// #define EXT_LED_PIN     32

#define REPORT_LOCATION_RATE_SECOND     120

#include "utilities.h"
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// Pins pour le GPS externe Quectel A76K (à adapter selon votre brochage)
#define BOARD_GPS_TX_PIN                    21
#define BOARD_GPS_RX_PIN                    22
#define BOARD_GPS_PPS_PIN                   23
#define BOARD_GPS_WAKEUP_PIN                19  // Pin de contrôle d'alimentation GPS

#include <TinyGPS++.h>

TinyGPSPlus gps;

// It depends on the operator whether to set up an APN. If some operators do not set up an APN,
// they will be rejected when registering for the network. You need to ask the local operator for the specific APN.
// APNs from other operators are welcome to submit PRs for filling.
#define NETWORK_APN     "hologram"             //CHN-CT: China Telecom

String modemName = "UNKOWN";
const char *client_id = "212910";
const char *request_url = "https://serveur1a.trackteur.cc";
const char *post_format = "deviceid=%s&lat=%.7f&lon=%.7f&speed=%.2f&altitude=%.2f&timestamp=%04d-%02d-%02dT%02d:%02d:%02dZ&batt=%u";

void light_sleep_delay(uint32_t ms)
{
#ifdef DEBUG_SKETCH
    delay(ms);
#else
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_light_sleep_start();
#endif
}


#ifdef BOARD_BAT_ADC_PIN
#include <vector>
#include <algorithm>
#include <numeric>

uint32_t getBatteryVoltage()
{
    std::vector<uint32_t> data;
    for (int i = 0; i < 30; ++i) {
        data.push_back(analogReadMilliVolts(BOARD_BAT_ADC_PIN));
        delay(10);
    }
    std::sort(data.begin(), data.end());
    data.erase(data.begin());     // remove min
    data.pop_back();              // remove max
    uint32_t sum = std::accumulate(data.begin(), data.end(), 0);
    double avg = static_cast<double>(sum) / data.size();
    return avg * 2; // facteur x2 car diviseur résistif 100K-100K
}

uint8_t voltageToPercent(uint32_t mv)
{
    if (mv >= 4200) return 100;
    if (mv <= 3500) return 0;
    return (mv - 3500) * 100 / 700;
}
#endif


bool post_location(GPSInfo &info)
{
    // Initialize HTTPS
    modem.https_begin();

    char post_buffer[128];

    // Do not calculate the power consumption, fixed at 100%
    uint8_t battery_percent = 100;
    #ifdef BOARD_BAT_ADC_PIN
        uint32_t batt_mv = getBatteryVoltage();
        battery_percent = voltageToPercent(batt_mv);
        Serial.printf("Battery: %u mV = %u%%\n", batt_mv, battery_percent);
    #endif

    snprintf(post_buffer, sizeof(post_buffer), post_format,
             client_id,
             info.latitude,
             info.longitude,
             info.speed,
             info.altitude,
             info.year, info.month, info.day,
             info.hour, info.minute, info.second,
             battery_percent);

    Serial.print("Request url:"); Serial.println(request_url);
    Serial.print("Post body  :"); Serial.println(post_buffer);

    // Set Post URL
    if (!modem.https_set_url(request_url)) {
        Serial.println("Failed to set the URL. Please check the validity of the URL!");
        return false;
    }
    modem.https_set_user_agent("TinyGSM/LilyGo-A76XX");
    int httpCode = modem.https_post(post_buffer);
    if (httpCode != 200) {
        Serial.print("HTTP post failed ! error code = ");
        Serial.println(httpCode);
        modem.https_end();
        return false;
    }

    modem.https_end();
    return true;
}


bool loopGPS(GPSInfo &info)
{
    const unsigned long GPS_TIMEOUT_MS = 90 * 1000;
    const unsigned long PPS_READ_WINDOW_MS = 5000;

    unsigned long gpsStartTime = millis();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_GPS_PPS_PIN, 1);

    while ((millis() - gpsStartTime) < GPS_TIMEOUT_MS) {
        Serial.println("En attente d'un signal PPS (fix GPS)...");

        esp_light_sleep_start();  // attend PPS

        Serial.println("Réveillé par PPS, lecture NMEA...");

        unsigned long ppsWindowStart = millis();

        while ((millis() - ppsWindowStart) < PPS_READ_WINDOW_MS) {
            while (SerialGPS.available()) {
                int c = SerialGPS.read();
                gps.encode(c); // ne log pas chaque caractère sauf pour debug

                // Vérifie si un fix complet et cohérent est disponible
                if (gps.location.isUpdated() &&
                    gps.location.isValid() &&
                    gps.time.isValid() &&
                    gps.time.isUpdated() &&
                    gps.date.isValid() &&
                    gps.date.isUpdated() &&
                    gps.time.age() < 2000 &&
                    gps.date.year() > 2020)
                {
                    // Remplit la structure GPSInfo
                    info.latitude = gps.location.lat();
                    info.longitude = gps.location.lng();
                    info.speed = gps.speed.kmph();
                    info.altitude = gps.altitude.meters();
                    info.course = gps.course.deg();
                    info.isFix = true;

                    info.year = gps.date.year();
                    info.month = gps.date.month();
                    info.day = gps.date.day();

                    info.hour = gps.time.hour();
                    info.minute = gps.time.minute();
                    info.second = gps.time.second();

                    info.gps_satellite_num = gps.satellites.value();
                    info.beidou_satellite_num = 0;
                    info.glonass_satellite_num = 0;
                    info.galileo_satellite_num = 0;

                    info.HDOP = gps.hdop.hdop();
                    info.PDOP = 0;
                    info.VDOP = 0;

                    // Log
                    Serial.println("===== GPS FIX ACQUIS =====");
                    Serial.printf("Lat: %.6f, Lng: %.6f, Alt: %.2f m\n", info.latitude, info.longitude, info.altitude);
                    Serial.printf("Speed: %.2f km/h, HDOP: %.2f\n", info.speed, info.HDOP);
                    Serial.printf("Satellites: %d, Timestamp: %04d-%02d-%02dT%02d:%02d:%02dZ\n",
                                  info.gps_satellite_num,
                                  info.year, info.month, info.day,
                                  info.hour, info.minute, info.second);

                    return true; // succès
                }
            }
        }

        Serial.println("PPS détecté mais pas de fix GPS complet (date/heure/position). Nouvelle tentative...");
    }

    Serial.println("⏱️ Timeout GNSS atteint, abandon tentative.");
    return false; // échec
}




void gps_sleep(bool enable)
{
    if (enable) {
        Serial.println("Putting Quectel A76K GPS to sleep...");
        // Envoyer la commande de sommeil NMEA au module A76K
        SerialGPS.println("$PMTK161,0*28"); // Commande de sommeil
        delay(100);
        // Optionnel: couper l'alimentation si vous avez un pin de contrôle
        //pinMode(BOARD_GPS_WAKEUP_PIN, OUTPUT);
        //digitalWrite(BOARD_GPS_WAKEUP_PIN, LOW);
    } else {
        Serial.println("Waking up Quectel A76K GPS...");
        // Restaurer l'alimentation GPS
        //pinMode(BOARD_GPS_WAKEUP_PIN, OUTPUT);
        //digitalWrite(BOARD_GPS_WAKEUP_PIN, HIGH);
        //delay(500); // Attendre que le A76K se réveille complètement
        
        // Envoyer une commande de réveil
        SerialGPS.println("$PMTK010,001*2E"); // Commande de réveil
        delay(100);
        
        // Redémarrer le GPS si nécessaire
        SerialGPS.println("$PMTK101*32"); // Hot restart
        delay(100);
    }
}
void modem_sleep() {
    Serial.println("🔕 Mise en veille du modem...");
    gps_sleep(true);  // ton module GPS externe

    modem.sendAT("+CSCLK=1");
    modem.waitResponse(1000);

    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, HIGH);

    // Garder DTR à HIGH pendant deep sleep
    gpio_hold_en((gpio_num_t)MODEM_DTR_PIN);
    gpio_deep_sleep_hold_en();

    delay(500);
}


void modem_wake()
{
    Serial.println("🔔 Réveil du modem...");

    // Met DTR à LOW pour réveiller le modem
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);

    delay(500);  // attendre que le modem se réveille complètement

    Serial.println("✅ Modem réveillé (DTR LOW).");
}



void setup()
{
    Serial.begin(115200); // Set console baud rate
    Serial.println("Start Sketch");

#ifdef EXT_LED_PIN
    pinMode(EXT_LED_PIN, OUTPUT);
    digitalWrite(EXT_LED_PIN, HIGH);
#endif


#ifdef BOARD_LED_PIN
    pinMode(BOARD_LED_PIN, OUTPUT);
    digitalWrite(BOARD_LED_PIN, LOW);
#endif

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
    gpio_hold_en((gpio_num_t )BOARD_POWERON_PIN);
    gpio_deep_sleep_hold_en();
#endif

    // Set modem reset pin ,reset modem
#ifdef MODEM_RESET_PIN
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    gpio_hold_en((gpio_num_t)MODEM_RESET_PIN);
    gpio_deep_sleep_hold_en();
#endif

    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    // Check if the modem is online
    Serial.println("Start modem...");

    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.println(".");
        if (retry++ > 10) {
            digitalWrite(BOARD_PWRKEY_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_PWRKEY_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_PWRKEY_PIN, LOW);
            retry = 0;
        }
    }
    Serial.println();

    while (1) {
        modemName = modem.getModemName();
        if (modemName == "UNKOWN") {
            Serial.println("Unable to obtain module information normally, try again");
            light_sleep_delay(1000);
        } else {
            Serial.print("Model Name:");
            Serial.println(modemName);
            break;
        }
        light_sleep_delay(5000);
    }

    // Check if SIM card is online
    SimStatus sim = SIM_ERROR;
    while (sim != SIM_READY) {
        sim = modem.getSimStatus();
        switch (sim) {
        case SIM_READY:
            Serial.println("SIM card online");
            break;
        case SIM_LOCKED:
            Serial.println("The SIM card is locked. Please unlock the SIM card first.");
            // const char *SIMCARD_PIN_CODE = "123456";
            // modem.simUnlock(SIMCARD_PIN_CODE);
            break;
        default:
            break;
        }
        light_sleep_delay(1000);
    }

    //SIM7672G Can't set network mode
#ifndef TINY_GSM_MODEM_SIM7672
    if (!modem.setNetworkMode(MODEM_NETWORK_AUTO)) {
        Serial.println("Set network mode failed!");
    }
    String mode = modem.getNetworkModes();
    Serial.print("Current network mode : ");
    Serial.println(mode);
#endif

#ifdef NETWORK_APN
    Serial.printf("Set network apn : %s\n", NETWORK_APN);
    modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), NETWORK_APN, "\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Set network apn error !");
    }
#endif

    // Check network registration status and network signal status
    int16_t sq ;
    Serial.print("Wait for the modem to register with the network.");
    RegStatus status = REG_NO_RESULT;
    while (status == REG_NO_RESULT || status == REG_SEARCHING || status == REG_UNREGISTERED) {
        status = modem.getRegistrationStatus();
        switch (status) {
        case REG_UNREGISTERED:
        case REG_SEARCHING:
            sq = modem.getSignalQuality();
            Serial.printf("[%lu] Signal Quality:%d\n", millis() / 1000, sq);
            light_sleep_delay(1000);
            break;
        case REG_DENIED:
            Serial.println("Network registration was rejected, please check if the APN is correct");
            return ;
        case REG_OK_HOME:
            Serial.println("Online registration successful");
            break;
        case REG_OK_ROAMING:
            Serial.println("Network registration successful, currently in roaming mode");
            break;
        default:
            Serial.printf("Registration Status:%d\n", status);
            light_sleep_delay(1000);
            break;
        }
    }
    Serial.println();


    Serial.printf("Registration Status:%d\n", status);
    light_sleep_delay(1000);

    String ueInfo;
    if (modem.getSystemInformation(ueInfo)) {
        Serial.print("Inquiring UE system information:");
        Serial.println(ueInfo);
    }

    if (!modem.setNetworkActive()) {
        Serial.println("Enable network failed!");
    }

    light_sleep_delay(5000);

    String ipAddress = modem.getLocalIP();
    Serial.print("Network IP:"); Serial.println(ipAddress);


    modem.sendAT("+SIMCOMATI");
    modem.waitResponse();

    SerialGPS.begin(9600, SERIAL_8N1, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN);
    
    // Initialiser le pin de contrôle d'alimentation du GPS A76K
    pinMode(BOARD_GPS_WAKEUP_PIN, OUTPUT);
    digitalWrite(BOARD_GPS_WAKEUP_PIN, HIGH); // GPS activé par défaut
    
    delay(1000); // Attendre l'initialisation du A76K
    
    // Configuration du module A76K
    SerialGPS.println("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"); // Activer GGA et RMC
    delay(100);
    SerialGPS.println("$PMTK220,1000*1F"); // Fréquence de mise à jour 1Hz
    delay(100);
    
    Serial.println("Quectel A76K GPS module initialized");

}


void loop()
{
    GPSInfo info;

    // Check if the modem is responsive, otherwise reboot
    bool isPowerOn = modem.testAT(3000);
    if (!isPowerOn) {
        Serial.println("Power Off , restart device");
        Serial.flush(); delay(100);
        esp_restart();
    }

 
    // Le modem reste en veille pendant qu'on cherche un fix
    gps_sleep(false); 
    bool hasFix = loopGPS(info);

    if (hasFix) {
        modem_wake();  // DTR LOW
        bool sent = post_location(info);
        if (!sent) {
            Serial.println("Échec d'envoi. Donnée perdue.");
        }
    } else {
        Serial.println("Pas de fix GPS, on dort quand même.");
    }

    // ✅ Dans tous les cas : endormir modem + GPS avant de dormir
    modem_sleep();  // inclut gps_sleep(true) + DTR HIGH
    Serial.printf("💤 Attente avant prochain cycle : %u secondes...\n", REPORT_LOCATION_RATE_SECOND);
    light_sleep_delay(REPORT_LOCATION_RATE_SECOND * 1000);

}







#ifndef TINY_GSM_FORK_LIBRARY
#error "No correct definition detected, Please copy all the [lib directories](https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX/tree/main/lib) to the arduino libraries directory , See README"
#endif



/*    #ifdef BOARD_BAT_ADC_PIN
        uint32_t batt_mv = getBatteryVoltage();
        if (batt_mv < 3200) {
            Serial.printf("Tension critique (%u mV). Entrée en sommeil profond pour protéger la batterie.\n", batt_mv);
            gps_sleep(true);
            modem.sleepEnable(true);
    
            // Optionnel : couper alimentation GPS si contrôlée
            digitalWrite(BOARD_GPS_WAKEUP_PIN, LOW);
    
            delay(100);
            esp_deep_sleep_start();  // Sommeil illimité
        }
    #endif
*/
