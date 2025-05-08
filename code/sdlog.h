#ifndef SDLOG_H
#define SDLOG_H

#include "common.h"

void saveLogMetadata(uint32_t currentIndex);
uint32_t loadLogMetadata();
void resendLastLog();
void logRealPositionToSd(float lat, float lon, const char* ts);

// Fonctions définies ailleurs mais utilisées ici
bool sendGpsToTraccar(const char* host, uint16_t port, const char* deviceId,
                      float lat, float lon, const char* timestampStr);
void resetGsmModule();
void initializeSD();

#endif
