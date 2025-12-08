/**
 * @file      utilities.h
 * @author    Trackteur Team
 * @license   MIT
 * @date      2025-12-08
 * @note      Configuration des pins pour LilyGo A7670G
 */

#pragma once

// Définir le board utilisé (décommenter celui que vous utilisez)
#define LILYGO_T_A7670G

#ifdef LILYGO_T_A7670G

#define TINY_GSM_RX_BUFFER          1024

#define SerialAT                    Serial1

// Pins du modem
#define MODEM_TX_PIN                26
#define MODEM_RX_PIN                27
#define BOARD_PWRKEY_PIN            4
#define BOARD_POWERON_PIN           12
#define MODEM_DTR_PIN               25
#define MODEM_RI_PIN                33
#define MODEM_RESET_PIN             5
#define MODEM_RESET_LEVEL           HIGH

// Durée d'impulsion pour allumer le modem (ms)
#define MODEM_POWERON_PULSE_WIDTH_MS    1000

// GPS pins (pour A7670G avec GPS intégré)
#define MODEM_GPS_ENABLE_GPIO       -1
#define MODEM_GPS_ENABLE_LEVEL      HIGH

// Pins carte SD (SPI)
#define BOARD_MISO_PIN              2
#define BOARD_MOSI_PIN              15
#define BOARD_SCK_PIN               14
#define BOARD_SD_CS_PIN             13

// LED (optionnel)
// #define BOARD_LED_PIN            -1

// ADC pour batterie
#define BOARD_ADC_PIN               35

// GPS PINS
#ifndef SerialGPS
#define SerialGPS Serial2
#endif
#define BOARD_GPS_TX_PIN                    21
#define BOARD_GPS_RX_PIN                    22

// Modes GNSS disponibles
// #define GNSS_MODE_GPS_BDS_GALILEO_SBAS_QZSS    6
#define GNSS_MODE_GPS_GLONASS_BDS              3

// Structure pour stocker les informations GPS
struct TrackteurGPSInfo {
    uint8_t isFix;              // 0 = pas de fix, 1 = fix 2D, 2 = fix 3D
    float latitude;             // Latitude en degrés
    float longitude;            // Longitude en degrés
    float speed;                // Vitesse en km/h
    float altitude;             // Altitude en mètres
    float course;               // Direction en degrés (0-360)
    uint16_t year;              // Année
    uint8_t month;              // Mois (1-12)
    uint8_t day;                // Jour (1-31)
    uint8_t hour;               // Heure (0-23)
    uint8_t minute;             // Minute (0-59)
    uint8_t second;             // Seconde (0-59)
    uint8_t gps_satellite_num;  // Nombre de satellites GPS
    uint8_t beidou_satellite_num; // Nombre de satellites BeiDou
    uint8_t glonass_satellite_num; // Nombre de satellites GLONASS
    uint8_t galileo_satellite_num; // Nombre de satellites Galileo
    float PDOP;                 // Position Dilution of Precision
    float HDOP;                 // Horizontal Dilution of Precision
    float VDOP;                 // Vertical Dilution of Precision
};

// Fonction utilitaire pour envoyer des commandes PMTK au module GPS
void sendPMTKCommand(const char* command) {
    SerialGPS.println(command);
    Serial.print("Sent PMTK command: ");
    Serial.println(command);
    delay(100); // Petite pause après l'envoi de la commande
}

#endif // LILYGO_T_A7670G
