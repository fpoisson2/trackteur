#include "config.h"
#include "gsm.h"

// Modèle GSM en cours d'utilisation :
// GSM_SIM7000, GSM_A7670, ou GSM_SIM7070.
// Utilisé pour adapter les commandes AT selon le module détecté.
GsmModel gsmModel = GSM_SIM7070;

// --- Intervalle entre les lectures GPS (en millisecondes) ---
// Ce délai détermine la fréquence à laquelle le GPS est interrogé.
// Exemple : 30000UL signifie une lecture toutes les 60 secondes.
const unsigned long GPS_POLL_INTERVAL = 30000UL;  // 30 secondes

// --- Paramètres de connexion au réseau mobile et au serveur Traccar ---

// APN (Access Point Name) : dépend du fournisseur de carte SIM utilisé.
// - "em" pour Emnify
// - "hologram" pour Hologram.io
const char* APN = "hologram";

// Adresse du serveur Traccar où envoyer les données GPS.
// Peut être un nom de domaine ou une adresse IP.
const char* TRACCAR_HOST = "serveur1a.trackteur.cc";

// Port TCP utilisé par le serveur Traccar pour recevoir les données (par défaut : 5055).
const uint16_t TRACCAR_PORT = 80;

// Identifiant unique de l'appareil, utilisé par Traccar pour associer les données GPS à un appareil spécifique.
// Doit correspondre à celui configuré dans l’interface de Traccar.
const char* DEVICE_ID = "202501";
