/**
 * @file      config_example.h
 * @author    Trackteur Team
 * @date      2025-12-08
 * @note      Fichier de configuration d'exemple
 *            Copiez ce fichier en config.h et modifiez selon vos besoins
 */

#pragma once

// ========================================
// CONFIGURATION TRACCAR
// ========================================

// ID unique de votre appareil (OBLIGATOIRE - ne pas laisser DEVICE_ID_001)
// Utilisez un identifiant unique pour chaque tracker
// Exemples: "TRUCK_001", "CAR_PARIS_01", "FLEET_VEHICLE_123"
#define TRACCAR_DEVICE_ID       "DEVICE_ID_001"

// URL du serveur Traccar
// Format: http://[serveur]:[port]
#define TRACCAR_SERVER_URL      "http://serveur1d.trackteur.cc:5055"

// ========================================
// CONFIGURATION RÉSEAU
// ========================================

// APN de votre opérateur mobile
// Décommentez la ligne correspondant à votre opérateur

// France
// #define NETWORK_APN "internet"           // Orange/Sosh
// #define NETWORK_APN "free"               // Free Mobile
// #define NETWORK_APN "sl2sfr"             // SFR
// #define NETWORK_APN "mmsbouygtel.com"    // Bouygues Telecom

// Belgique
// #define NETWORK_APN "internet.be"        // Proximus
// #define NETWORK_APN "mworld.be"          // Orange Belgium
// #define NETWORK_APN "web.be"             // Base/Telenet

// Suisse
// #define NETWORK_APN "gprs.swisscom.ch"   // Swisscom
// #define NETWORK_APN "internet"           // Sunrise
// #define NETWORK_APN "internet"           // Salt

// Canada
// #define NETWORK_APN "internet.com"       // Bell
// #define NETWORK_APN "media.bell.ca"      // Bell (alternatif)
// #define NETWORK_APN "internet.com"       // Rogers

// ========================================
// PARAMÈTRES GPS
// ========================================

// Intervalle d'envoi des positions (en secondes)
// Par défaut: 120 secondes = 2 minutes
#define GPS_REPORT_INTERVAL     120

// Activation du mode AGPS (GPS assisté) pour fix plus rapide
#define ENABLE_AGPS             true

// Mode GNSS (satellites utilisés)
// Options disponibles:
// - GNSS_MODE_GPS_BDS_GALILEO_SBAS_QZSS (recommandé pour A7670)
// - GNSS_MODE_GPS_GLONASS_BDS
#define GPS_MODE                GNSS_MODE_GPS_BDS_GALILEO_SBAS_QZSS

// ========================================
// PARAMÈTRES RÉSEAU
// ========================================

// Nombre de tentatives de retry en cas d'échec d'envoi
#define MAX_RETRY_ATTEMPTS      3

// Délai entre chaque retry (en millisecondes)
#define RETRY_DELAY_MS          2000

// Timeout pour les requêtes HTTP (en millisecondes)
#define HTTP_TIMEOUT_MS         30000

// ========================================
// CARTE SD
// ========================================

// Activer/désactiver la sauvegarde sur carte SD
#define USE_SD_CARD             true

// Nom du fichier CSV sur la carte SD
#define SD_CSV_FILENAME         "/gps_data.csv"

// Taille maximale du fichier CSV (en octets)
// Quand la taille est atteinte, le fichier est renommé en backup
#define SD_MAX_FILE_SIZE        1048576  // 1 MB

// ========================================
// MODE DEBUG
// ========================================

// Activer les messages de debug détaillés
// #define DEBUG_MODE

// Afficher toutes les commandes AT envoyées au modem
// #define DUMP_AT_COMMANDS

// Afficher les trames GPS brutes
// #define DEBUG_GPS_RAW

// ========================================
// ÉCONOMIE D'ÉNERGIE
// ========================================

// Activer le mode veille du modem entre les envois
#define ENABLE_MODEM_SLEEP      true

// Activer le mode veille de l'ESP32 entre les envois
#define ENABLE_ESP_SLEEP        true

// ========================================
// LED DE STATUT (optionnel)
// ========================================

// Pin de la LED externe pour indication visuelle
// Décommentez et définissez le pin si vous avez une LED externe
// #define EXT_LED_PIN          32

// Comportement de la LED:
// - Allumée: Lecture GPS en cours
// - Éteinte: Mode veille
// - Clignotante: Envoi de données

// ========================================
// PARAMÈTRES AVANCÉS
// ========================================

// Taille du buffer de réception du modem
#define MODEM_RX_BUFFER_SIZE    1024

// Timeout pour la détection du modem (en millisecondes)
#define MODEM_INIT_TIMEOUT      30000

// Timeout pour l'obtention d'un fix GPS (en millisecondes)
#define GPS_FIX_TIMEOUT         300000  // 5 minutes

// Redémarrage automatique si pas de fix GPS après ce délai
#define AUTO_RESTART_NO_GPS     false

// ========================================
// NOTES
// ========================================

/*
 * IMPORTANT:
 *
 * 1. ID UNIQUE: Assurez-vous que chaque tracker a un TRACCAR_DEVICE_ID unique
 *
 * 2. APN: L'APN est souvent nécessaire. Contactez votre opérateur si vous
 *    ne connaissez pas l'APN correct.
 *
 * 3. CARTE SIM: Assurez-vous que:
 *    - Le code PIN est désactivé
 *    - Le forfait data est actif
 *    - La carte SIM fonctionne dans votre région
 *
 * 4. ANTENNES:
 *    - Antenne GPS: À l'extérieur ou près d'une fenêtre pour meilleur signal
 *    - Antenne 4G: Bien connectée pour la transmission de données
 *
 * 5. BATTERIE:
 *    - Mode veille activé = ~2-3 mA
 *    - Mode veille désactivé = ~50-100 mA
 *    - Ajustez selon vos besoins autonomie vs fréquence de mise à jour
 *
 * 6. CARTE SD:
 *    - Optionnelle mais recommandée comme backup
 *    - Format FAT32
 *    - Taille: 1GB à 32GB recommandé
 */
