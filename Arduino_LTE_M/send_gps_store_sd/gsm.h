#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

bool executeSimpleCommand(const char* command, const char* expectedResponse,
                          unsigned long timeoutMillis, uint8_t retries);
void readSerialResponse(unsigned long waitMillis);
bool waitForInitialOK(uint8_t maxRetries);
void resetGsmModule();
bool sendGpsToTraccar(const char* host, uint16_t port, const char* deviceId,
                      float lat, float lon, const char* timestampStr);
void clearSerialBuffer();
bool initialCommunication();
bool step1NetworkSettings();
bool waitForSimReady();
bool step2NetworkRegistration();
bool step3PDPContext();
bool step4EnableGNSS();
void initializeModulePower();

#endif // GSM_H
