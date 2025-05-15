#include "config.h"

// --- Intervalle entre les lectures GPS (en millisecondes) ---
// Ce délai détermine la fréquence à laquelle le GPS est interrogé.
// Exemple : 60000UL signifie une lecture toutes les 60 secondes.
const unsigned long GPS_POLL_INTERVAL = 10000UL;  // 60 secondes

// --- Paramètres de connexion au réseau mobile et au serveur Traccar ---

// APN (Access Point Name) : dépend du fournisseur de carte SIM utilisé.
// - "em" pour Emnify
// - "onomondo" pour Onomondo
// - "hologram" pour Hologram.io
const char* APN = "onomondo";

// Adresse du serveur Traccar où envoyer les données GPS.
// Peut être un nom de domaine ou une adresse IP.
const char* TRACCAR_HOST = "trackteur2.ve2fpd.com";

// Port TCP utilisé par le serveur Traccar pour recevoir les données (par défaut : 5055).
const uint16_t TRACCAR_PORT = 80;

// Identifiant unique de l'appareil, utilisé par Traccar pour associer les données GPS à un appareil spécifique.
// Doit correspondre à celui configuré dans l’interface de Traccar.
const char* DEVICE_ID = "212910";
