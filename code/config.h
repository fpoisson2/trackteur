#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Niveau de journalisation (log) ---
// Définit le niveau de détails des messages de debug dans le moniteur série.
// 0 : Aucun log (production silencieuse)
// 1 : Info (messages importants seulement)
// 2 : Debug (détails complets, utile pour le développement et le dépannage)
#define LOG_LEVEL 2

// --- Configuration système ---
// Intervalle entre chaque lecture GPS, exprimé en millisecondes.
// La valeur réelle est définie dans le fichier .ino ou un .cpp.
extern const unsigned long GPS_POLL_INTERVAL;

// --- Paramètres réseau et Traccar ---
// APN : nom du point d'accès (Access Point Name) pour accéder au réseau mobile.
// Dépend du fournisseur de carte SIM (ex. "em", "onomondo", "hologram").
extern const char* APN;

// Adresse du serveur Traccar (peut être un nom de domaine ou une IP).
extern const char* TRACCAR_HOST;

// Port TCP utilisé par Traccar pour recevoir les données (par défaut : 5055).
extern const uint16_t TRACCAR_PORT;

// Identifiant unique de l'appareil, utilisé par Traccar pour reconnaître la source des données GPS.
extern const char* DEVICE_ID;

#endif
