#include "config.h"

// --- Intervalle entre les lectures GPS (en millisecondes) ---
const unsigned long GPS_POLL_INTERVAL = 60000UL;  // 60 secondes

// --- Paramètres Traccar et APN ---
const char* APN = "em";                        // ou "onomondo", "hologram", etc.
const char* TRACCAR_HOST = "trackteur.ve2fpd.com";
const uint16_t TRACCAR_PORT = 5055;
const char* DEVICE_ID = "212910";
