/**
 * @file      TraccarGPS.ino
 * @author    Trackteur Team
 * @license   MIT
 * @date      2025-12-08
 * @note      Code pour LilyGo A7670G - Envoi données GPS vers Traccar avec backup SD
 *            - Envoi vers serveur1d.trackteur.cc toutes les 2 minutes
 *            - Sauvegarde sur carte SD en cas d'échec
 *            - Logique de retry avec 3 tentatives
 */

#define TINY_GSM_RX_BUFFER          1024

#include "config.h"
#include "utilities.h"
#include <TinyGsmClient.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>

#ifndef MODEM_DTR_PIN
#error "This board does not support modem sleep function"
#endif

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGPSPlus gps;

// Configuration réseau
String apn = NETWORK_APN;

// Configuration Traccar
const char *client_id = TRACCAR_DEVICE_ID;
const char *request_url = TRACCAR_SERVER_URL;
const char *post_format = "/?id=%s&lat=%.7f&lon=%.7f&speed=%.2f&altitude=%.2f&bearing=%.2f&timestamp=%04d-%02d-%02dT%02d:%02d:%02dZ&hdop=%.2f&batt=%u";

String modemName = "UNKNOWN";
bool sdCardAvailable = false;
unsigned long lastReportTime = 0;
unsigned long lastGpsFixTime = 0;

// Fonction pour formater la date/heure ISO 8601
String formatTimestamp(TrackteurGPSInfo &info) {
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             info.year, info.month, info.day,
             info.hour, info.minute, info.second);
    return String(timestamp);
}

// Sauvegarde des données sur carte SD
bool saveToSD(TrackteurGPSInfo &info) {
    if (!sdCardAvailable) {
        Serial.println("SD card not available");
        return false;
    }

    File file = SD.open("/gps_data.csv", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open SD file for writing");
        return false;
    }

    // Format CSV: timestamp,lat,lon,speed,altitude,bearing,hdop
    char csvLine[256];
    snprintf(csvLine, sizeof(csvLine), "%04d-%02d-%02d %02d:%02d:%02d,%.7f,%.7f,%.2f,%.2f,%.2f,%.2f\n",
             info.year, info.month, info.day,
             info.hour, info.minute, info.second,
             info.latitude, info.longitude, info.speed, info.altitude, info.course, info.HDOP);

    file.print(csvLine);
    file.close();

    Serial.println("Data saved to SD card:");
    Serial.print(csvLine);
    return true;
}

// Initialisation de la carte SD
bool initSDCard() {
    Serial.println("Initializing SD card...");

    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Card Mount Failed");
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return false;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    // Créer le header du fichier CSV s'il n'existe pas
    if (!SD.exists("/gps_data.csv")) {
        File file = SD.open("/gps_data.csv", FILE_WRITE);
        if (file) {
            file.println("timestamp,latitude,longitude,speed,altitude,bearing,hdop");
            file.close();
            Serial.println("Created CSV file with header");
        }
    }

    return true;
}

// Envoi de la position avec retry
bool post_location(TrackteurGPSInfo &info) {
    int retryCount = 0;
    bool success = false;

    while (retryCount < MAX_RETRY_ATTEMPTS && !success) {
        if (retryCount > 0) {
            Serial.printf("Retry attempt %d/%d\n", retryCount + 1, MAX_RETRY_ATTEMPTS);
            delay(2000); // Attendre 2 secondes avant retry
        }

        // Initialiser HTTPS
        modem.https_begin();

        char post_buffer[256];
        uint8_t battery_percent = 100;

        // Construire l'URL complète avec les paramètres
        snprintf(post_buffer, sizeof(post_buffer), post_format,
                 client_id,
                 info.latitude,
                 info.longitude,
                 info.speed,
                 info.altitude,
                 info.course,
                 info.year, info.month, info.day,
                 info.hour, info.minute, info.second,
                 info.HDOP,
                 battery_percent);

        String fullUrl = String(request_url) + String(post_buffer);

        Serial.print("Request URL: ");
        Serial.println(fullUrl);

        // Configurer l'URL
        if (!modem.https_set_url(fullUrl.c_str())) {
            Serial.println("Failed to set the URL");
            retryCount++;
            continue;
        }

        modem.https_set_user_agent("TinyGSM/LilyGo-A76XX-Trackteur");

        // Envoyer la requête GET (Traccar utilise GET avec paramètres URL)
        int httpCode = modem.https_get();

        Serial.printf("HTTP Response Code: %d\n", httpCode);

        if (httpCode == 200) {
            Serial.println("Location sent successfully!");
            success = true;
        } else {
            Serial.printf("HTTP request failed! Error code: %d\n", httpCode);
            retryCount++;
        }

        modem.https_end();
    }

    // Si tous les retry ont échoué, sauvegarder sur SD
    if (!success) {
        Serial.println("All retry attempts failed. Saving to SD card...");
        saveToSD(info);
    }

    return success;
}

// Mise en veille du modem
void modem_enter_sleep(uint32_t ms) {
    Serial.printf("Enter modem sleep mode. Will wake up in %u seconds\n", ms / 1000);

#ifdef BOARD_LED_PIN
    digitalWrite(BOARD_LED_PIN, LOW);
#endif

    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, HIGH);

    if (!modem.sleepEnable(true)) {
        Serial.println("Modem sleep failed!");
    } else {
        Serial.println("Modem entered sleep mode");
    }

    delay(ms);

    digitalWrite(MODEM_DTR_PIN, LOW);

#ifdef BOARD_LED_PIN
    digitalWrite(BOARD_LED_PIN, HIGH);
#endif

    delay(500);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n========================================");
    Serial.println("Trackteur - LilyGo A7670G GPS Tracker");
    Serial.println("Server: serveur1d.trackteur.cc");
    Serial.println("========================================\n");

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
    gpio_hold_en((gpio_num_t)BOARD_POWERON_PIN);
    gpio_deep_sleep_hold_en();
#endif

#ifdef MODEM_RESET_PIN
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
    delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    gpio_hold_en((gpio_num_t)MODEM_RESET_PIN);
    gpio_deep_sleep_hold_en();
#endif

#ifdef MODEM_FLIGHT_PIN
    pinMode(MODEM_FLIGHT_PIN, OUTPUT);
    digitalWrite(MODEM_FLIGHT_PIN, HIGH);
#endif

    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);

    // Démarrage du modem
    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(MODEM_POWERON_PULSE_WIDTH_MS);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    Serial.println("Starting modem...");
    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 30) {
            digitalWrite(BOARD_PWRKEY_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_PWRKEY_PIN, HIGH);
            delay(MODEM_POWERON_PULSE_WIDTH_MS);
            digitalWrite(BOARD_PWRKEY_PIN, LOW);
            retry = 0;
        }
    }
    Serial.println("\nModem ready!");

    // Vérifier le modèle du modem
    modemName = modem.getModemName();
    Serial.printf("Modem: %s\n", modemName.c_str());

    // Vérifier la carte SIM
    SimStatus sim = SIM_ERROR;
    while (sim != SIM_READY) {
        sim = modem.getSimStatus();
        if (sim == SIM_READY) {
            Serial.println("SIM card ready");
        } else if (sim == SIM_LOCKED) {
            Serial.println("SIM card locked. Please unlock first.");
        }
        delay(1000);
    }

    // Configuration réseau
#ifdef NETWORK_APN
    Serial.printf("Setting network APN: %s\n", NETWORK_APN);
    modem.setNetworkAPN(NETWORK_APN);
#endif

    // Enregistrement réseau
    Serial.print("Registering on network...");
    RegStatus status = REG_NO_RESULT;
    while (status != REG_OK_HOME && status != REG_OK_ROAMING) {
        status = modem.getRegistrationStatus();
        if (status == REG_DENIED) {
            Serial.println("\nNetwork registration denied!");
            return;
        }
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nNetwork registered!");

    // Activation réseau
    Serial.println("Activating network...");
    retry = 3;
    while (retry--) {
        if (modem.setNetworkActive()) {
            break;
        }
        Serial.println("Failed, retrying...");
        delay(3000);
    }

    String ipAddress = modem.getLocalIP();
    Serial.printf("IP Address: %s\n", ipAddress);

    // Initialisation GPS
    SerialGPS.begin(9600, SERIAL_8N1, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN);

    // Envoyer la commande de wake et hot start au module GPS
    sendPMTKCommand("$PMTK010,001*2E"); // Wake command
    sendPMTKCommand("$PMTK101*32");     // Hot start (optionnel, pour un fix plus rapide)

    // Initialisation carte SD
#ifdef USE_SD_CARD
    sdCardAvailable = initSDCard();
    if (sdCardAvailable) {
        Serial.println("SD card ready for backup");
    } else {
        Serial.println("SD card not available - backup disabled");
    }
#endif

    Serial.println("\n========================================");
    Serial.println("Setup complete! Starting GPS tracking...");
    Serial.println("========================================\n");
}

void loop() {
#ifdef EXT_LED_PIN
    digitalWrite(EXT_LED_PIN, HIGH);
#endif

    // Vérifier si le modem répond
    bool isPowerOn = modem.testAT(3000);
    if (!isPowerOn) {
        Serial.println("Modem not responding - restarting...");
        Serial.flush();
        delay(100);
        esp_restart();
    }

    // Read GPS data
    static String gps_line = "";
    while (SerialGPS.available() > 0) {
        char c = SerialGPS.read();
        gps_line += c;
        if (c == '\n') {
            Serial.print("GPS RAW: ");
            Serial.print(gps_line);
            for (unsigned int i = 0; i < gps_line.length(); i++){
                gps.encode(gps_line[i]);
            }
            gps_line = "";
        }
    }

    if (gps.location.isUpdated() && gps.location.isValid()) {
        // Reset timeout car on a un fix GPS
        lastGpsFixTime = millis();

        TrackteurGPSInfo info;
        info.isFix = 2; // 2 for 3D fix
        info.latitude = gps.location.lat();
        info.longitude = gps.location.lng();
        info.speed = gps.speed.kmph();
        info.altitude = gps.altitude.meters();
        info.course = gps.course.deg();
        info.year = gps.date.year();
        info.month = gps.date.month();
        info.day = gps.date.day();
        info.hour = gps.time.hour();
        info.minute = gps.time.minute();
        info.second = gps.time.second();
        info.HDOP = gps.hdop.value();
        info.gps_satellite_num = gps.satellites.value();

        unsigned long currentTime = millis();
        if (currentTime - lastReportTime >= GPS_REPORT_INTERVAL * 1000 || lastReportTime == 0) {
            bool sent = post_location(info);
            lastReportTime = currentTime;

            if (sent) {
                modem_enter_sleep(GPS_REPORT_INTERVAL * 1000);
            } else {
                delay(GPS_REPORT_INTERVAL * 1000);
            }
        }
    } else if (gps.location.isUpdated()) {
        Serial.println("GPS fix not available, waiting...");

        // Timeout sans fix GPS - entrer en sleep pour économiser la batterie
        if (lastGpsFixTime == 0) {
            lastGpsFixTime = millis(); // Initialiser au premier passage
        }
        if (millis() - lastGpsFixTime >= GPS_FIX_TIMEOUT * 1000) {
            Serial.println("GPS fix timeout - entering sleep to save battery...");
            modem_enter_sleep(GPS_REPORT_INTERVAL * 1000);
            lastGpsFixTime = millis(); // Reset après sleep
        }
    }


    if (millis() > 5000 && gps.charsProcessed() < 10) {
        Serial.println(F("No GPS detected: check wiring."));
        delay(1000);
    }

#ifdef EXT_LED_PIN
    digitalWrite(EXT_LED_PIN, LOW);
#endif
}
