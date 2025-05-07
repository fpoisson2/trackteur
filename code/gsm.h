#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

enum GsmModel : uint8_t { GSM_SIM7000, GSM_A7670, GSM_SIM7070 };
extern GsmModel gsmModel;        // défini dans gsm.cpp

bool executeSimpleCommand(const char* command, const char* expectedResponse,
                          unsigned long timeoutMillis, uint8_t retries);
bool executeSimpleCommand(const __FlashStringHelper* command,
                          const char* expectedResponse,
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

bool tcpOpen (const char* host, uint16_t port);
bool tcpSend (const char* payload, uint16_t len);
bool tcpClose();

bool waitForSerialResponsePattern(const char* pattern,
                                  unsigned long totalTimeout = 30000UL,
                                  unsigned long pollInterval = 200UL);

bool waitForAnyPattern(const char* pattern1, const char* pattern2,
                       unsigned long totalTimeout = 10000UL,
                       unsigned long pollInterval = 200UL);


#endif // GSM_H
