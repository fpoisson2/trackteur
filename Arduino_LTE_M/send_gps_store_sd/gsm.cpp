/* =======================================================================
 * Projet      : Traceur GPS LTE avec SD et Traccar
 * Fichier     : gsm.cpp
 * Description : Gère la communication série avec le module SIM7000G.
 *               Fournit les fonctions de configuration réseau, gestion
 *               GNSS, PDP context, et commandes AT avec accusé de réception.
 *               Comprend également la logique de redémarrage du module.
 *
 * Auteur      : Francis Poisson-Gagnon
 * Dépôt Git   : https://github.com/fpoisson2/trackteur
 * Matériel    : SIM7000G + Arduino Nano (ATmega328P)
 * ======================================================================= */


#include "common.h"
#include "gsm.h"

void readSerialResponse(unsigned long waitMillis) {
  unsigned long start = millis();
  memset(responseBuffer, 0, RESPONSE_BUFFER_SIZE);
  responseBufferPos = 0;
  bool anythingReceived = false;

  while (millis() - start < waitMillis) {
    wdt_reset();
    while (moduleSerial.available()) {
      anythingReceived = true;
      if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
        char c = moduleSerial.read();
        if (c != '\0') {
          responseBuffer[responseBufferPos++] = c;
          responseBuffer[responseBufferPos] = '\0';
        }
      } else {
        moduleSerial.read(); // Jette le surplus
      }
    }
    if (!moduleSerial.available()) delay(5);
  }

  if (responseBufferPos > 0) {
    Serial.print(F("Rcvd: ["));
    for (uint8_t i = 0; i < responseBufferPos; i++) {
      char c = responseBuffer[i];
      if (c == '\r') continue;
      else if (c == '\n') Serial.print(F("<LF>"));
      else if (isprint(c)) Serial.print(c);
      else Serial.print('.');
    }
    Serial.println(F("]"));
  } else if (anythingReceived) {
    Serial.println(F("Rcvd: [Empty/Discarded]"));
  }
}

bool executeSimpleCommand(const char* command, const char* expectedResponse,
                          unsigned long timeoutMillis, uint8_t retries) {
  for (uint8_t i = 0; i < retries; i++) {
    wdt_reset();
    Serial.print(F("Send [")); Serial.print(i + 1); Serial.print(F("]: "));
    Serial.println(command);
    moduleSerial.println(command);
    readSerialResponse(timeoutMillis);
    if (strstr(responseBuffer, expectedResponse)) {
      Serial.println(F(">> OK Resp."));
      return true;
    }
    if (strstr(responseBuffer, "ERROR")) Serial.println(F(">> ERROR Resp."));
    Serial.println(F(">> No/Wrong Resp."));
    if (i < retries - 1) delay(500);
  }
  Serial.println(F(">> Failed after retries."));
  return false;
}

bool waitForInitialOK(uint8_t maxRetries) {
  for (uint8_t i = 0; i < maxRetries; i++) {
    Serial.print(F("AT (Try ")); Serial.print(i + 1); Serial.print(F(")... "));
    moduleSerial.println("AT");
    readSerialResponse(1000UL);
    if (strstr(responseBuffer, "OK")) {
      Serial.println(F("OK."));
      return true;
    }
    Serial.println(F("No OK."));
    delay(500);
  }
  return false;
}

void resetGsmModule() {
  Serial.println(F("*** Power-cycling GSM module ***"));
  digitalWrite(powerPin, LOW);
  delay(2000);
  while (moduleSerial.available()) moduleSerial.read(); // clear serial buffer
  digitalWrite(powerPin, HIGH);
  delay(5000);

  if (!waitForInitialOK(10)) {
    Serial.println(F("  ERROR: module still unresponsive after reset"));
  } else {
    Serial.println(F("  GSM module is back online"));
    executeSimpleCommand("ATE0", "OK", 1000, 2);
    executeSimpleCommand("AT+CMEE=2", "OK", 1000, 2);
  }
}

// Vide le buffer série
void clearSerialBuffer() {
  while (moduleSerial.available()) moduleSerial.read();
}


void initializeModulePower() {
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
  Serial.println(F("Module power pin configured (D2)."));
  moduleSerial.begin(moduleBaudRate);
  Serial.print(F("Software Serial initialized on Pins RX:"));
  Serial.print(swRxPin);
  Serial.print(F(", TX:"));
  Serial.print(swTxPin);
  Serial.print(F(" at "));
  Serial.print(moduleBaudRate);
  Serial.println(F(" baud."));
  Serial.println(F("Turning module ON..."));
  digitalWrite(powerPin, HIGH);
  delay(5000);
  Serial.println(F("Module boot wait complete."));
}

bool initialCommunication() {
  Serial.println(F("Attempting initial communication..."));
  if (!waitForInitialOK(15)) {
    Serial.println(F("FATAL: Module unresponsive."));
    return false;
  }
  Serial.println(F("Initial communication OK."));
  executeSimpleCommand("ATE0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CMEE=2", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CGNSURC=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CGNSTST=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CTZU=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CLTS=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CREG=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CEREG=0", "OK", 1000UL, 2);

  return true;
}

bool step1NetworkSettings() {
  Serial.println(F("\n=== STEP 1: Network Settings ==="));
  if (!executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 2000UL, 2)) {
    Serial.println(F("WARNING: CIPSHUT failed. Continuing anyway..."));
  }
  if (!executeSimpleCommand("AT+CFUN=0", "OK", 1000UL, 1)) {
    Serial.println(F("WARNING: CFUN=0 failed. Continuing..."));
  }
  bool ok = true;
  ok &= executeSimpleCommand("AT+CNMP=38", "OK", 500UL, 3);
  ok &= executeSimpleCommand("AT+CMNB=1", "OK", 500UL, 3);
  if (ok) {
    Serial.println(F("Turning radio ON (CFUN=1,1)..."));
    moduleSerial.println("AT+CFUN=1,1");
    delay(500);
  }
  return ok;
}

bool waitForSimReady() {
  Serial.println(F("Waiting for SIM readiness..."));
  for (uint8_t i = 0; i < 10; i++) {
    if (executeSimpleCommand("AT+CPIN?", "+CPIN:", 1000UL, 1) && strstr(responseBuffer, "READY")) {
      Serial.println(F("SIM Ready."));
      return true;
    }
    delay(1000);
  }
  Serial.println(F("ERROR: SIM not READY after wait."));
  return false;
}

bool step2NetworkRegistration() {
  Serial.println(F("\n=== STEP 2: Network Registration ==="));
  for (uint8_t i = 0; i < 20; i++) {
    Serial.print(F("Reg check ")); Serial.print(i + 1); Serial.println(F("..."));
    executeSimpleCommand("AT+CSQ", "+CSQ", 500UL, 1);
    executeSimpleCommand("AT+COPS?", "+COPS", 3000UL, 1);
    executeSimpleCommand("AT+CREG?", "+CREG:", 500UL, 1);
    bool creg_ok = strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5");
    executeSimpleCommand("AT+CEREG?", "+CEREG:", 500UL, 1);
    bool cereg_ok = strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5");
    if (creg_ok || cereg_ok) {
      Serial.println(F("Registered."));
      return true;
    }
    delay(2000);
  }
  Serial.println(F("ERROR: Failed network registration."));
  return false;
}

bool step3PDPContext() {
  Serial.println(F("\n=== STEP 3: PDP Context ==="));
  char cmd[RESPONSE_BUFFER_SIZE];
  snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
  if (!executeSimpleCommand(cmd, "OK", 500UL, 3)) return false;
  if (!executeSimpleCommand("AT+CGATT=1", "OK", 500UL, 3)) return false;
  return executeSimpleCommand("AT+CGACT=1,1", "OK", 500UL, 3);
}

bool step4EnableGNSS() {
  Serial.println(F("\n=== STEP 4: Enable GNSS ==="));
  if (!executeSimpleCommand("AT+CGNSPWR=1", "OK", 500UL, 3)) {
    Serial.println(F("ERROR: Failed to enable GNSS!"));
    return false;
  }
  delay(1000); // Laisse le temps au GNSS de s'initialiser
  return true;
}
