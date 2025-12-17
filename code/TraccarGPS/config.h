/**
 * @file      config.h
 * @author    Trackteur Team
 * @date      2025-12-08
 * @note      Configuration pour LilyGo A7670G - Traccar GPS Tracker
 */

#pragma once

// ========================================
// CONFIGURATION MODEM
// ========================================
#define TINY_GSM_MODEM_A7670

// ========================================
// CONFIGURATION TRACCAR
// ========================================

// ID unique de votre appareil (OBLIGATOIRE)
#define TRACCAR_DEVICE_ID       "DEVICE_ID_001"

// URL du serveur Traccar (HTTPS)
#define TRACCAR_SERVER_URL      "https://serveur1e.trackteur.cc"

// ========================================
// CONFIGURATION RÉSEAU
// ========================================

// APN Hologram
#define NETWORK_APN             "hologram"

// ========================================
// PARAMÈTRES GPS
// ========================================

// Intervalle d'envoi des positions (en secondes)
// 120 = 2 minutes
#define GPS_REPORT_INTERVAL     120

// Timeout sans fix GPS avant sleep (en secondes)
// 60 = 1 minute - évite de vider la batterie à l'intérieur
#define GPS_FIX_TIMEOUT         60

// ========================================
// PARAMÈTRES RÉSEAU
// ========================================

// Nombre de tentatives de retry en cas d'échec
#define MAX_RETRY_ATTEMPTS      3

// ========================================
// CARTE SD
// ========================================

// Activer/désactiver la sauvegarde sur carte SD
#define USE_SD_CARD             true

// Pins de la carte SD
#define SD_CS_PIN               13
#define SD_MISO_PIN             2
#define SD_MOSI_PIN             15
#define SD_SCK_PIN              14

// ========================================
// MODE DEBUG (décommenter pour activer)
// ========================================

// Afficher toutes les commandes AT
// #define DUMP_AT_COMMANDS

// LED externe de statut (optionnel)
// #define EXT_LED_PIN          32
