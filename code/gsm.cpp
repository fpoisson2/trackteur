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

GsmModel gsmModel = GSM_SIM7000;  // valeur par défaut

static void detectModel()
{
  executeSimpleCommand("ATI", "", 1500, 1);
  if (strstr(responseBuffer, "A7670"))  gsmModel = GSM_A7670;
  else if (strstr(responseBuffer, "SIM7000")) gsmModel = GSM_SIM7000;

  Serial.print(F(">> Modem detected: "));
  Serial.println(gsmModel == GSM_A7670 ? F("A7670E") : F("SIM7000G"));
}

// --- PDP / pile data -------------------------------------------------
bool openDataStack()
{
  if (gsmModel == GSM_A7670) {
    // A7670E : APN + NETOPEN
    clearSerialBuffer();
    snprintf_P(scratchBuf, sizeof(scratchBuf), PSTR("AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0"), APN);
    if (!executeSimpleCommand(scratchBuf, "OK", 1000, 3)) return false;

    // NETOPEN avec trois tentatives
     for (uint8_t i = 0; i < 3; i++) {
      Serial.print(F("NETOPEN attempt ")); Serial.println(i + 1);
      moduleSerial.println(F("AT+NETOPEN"));
      readSerialResponse(10000UL);
    
      // Si pas de "+NETOPEN:" dans la première réponse, on attend la suite
      if (!strstr(responseBuffer, "+NETOPEN:")) {
        readSerialResponse(10000UL);  // attend la suite potentielle
      }
    
      if (strstr(responseBuffer, "+NETOPEN: 0") || strstr(responseBuffer, "already opened")) {
        Serial.println(F("NETOPEN OK"));
        return true;
      }
    
      Serial.println(F("NETOPEN failed, retrying..."));
      delay(500);
    }

    executeSimpleCommand(F("AT+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\""), "OK", 1000, 2);
    Serial.println(F("NETOPEN ultimately failed."));
    return false;
  }
  /* ---------------- SIM7000G ---------------- */
  snprintf_P(scratchBuf, sizeof(scratchBuf), PSTR("AT+CGDCONT=1,\"IP\",\"%s\""), APN);
  if (!executeSimpleCommand(scratchBuf, "OK", 500, 3)) return false;
  if (!executeSimpleCommand(F("AT+CGATT=1"), "OK", 1500, 3))      return false;
  return executeSimpleCommand(F("AT+CGACT=1,1"), "OK", 1500, 3);
}

bool closeDataStack()
{
  if (gsmModel == GSM_A7670)
    return executeSimpleCommand("AT+NETCLOSE", "+NETCLOSE:", 3000, 2);
  else
    return executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 3000, 2);
}

bool tcpOpen(const char* host, uint16_t port)
{
  clearSerialBuffer();

  if (gsmModel == GSM_A7670) {
    snprintf_P(scratchBuf, sizeof(scratchBuf), PSTR("AT+CIPOPEN=0,\"TCP\",\"%s\",%u"), host, port);
    Serial.print(F("→ TCP open command (A7670): ")); Serial.println(scratchBuf);
    moduleSerial.write((const uint8_t*)scratchBuf, strlen(scratchBuf));
    moduleSerial.write('\r');

    delay(500); // stabilité
    if (waitForSerialResponsePattern("+CIPOPEN: 0,0", 30000UL)) {
      Serial.println(F("✔ TCP connection successful (A7670)"));
      return true;
    }
    Serial.print(F("❌ TCP connection failed (A7670), response: ")); Serial.println(responseBuffer);
    return false;

   } else if (gsmModel == GSM_SIM7000) {
    snprintf_P(scratchBuf, sizeof(scratchBuf), PSTR("AT+CIPSTART=\"TCP\",\"%s\",%u"), host, port);
    Serial.print(F("→ TCP open command (SIM7000): ")); Serial.println(scratchBuf);
    moduleSerial.write((const uint8_t*)scratchBuf, strlen(scratchBuf));
    moduleSerial.write('\r');

    delay(500);
    
    // Read initial response and check for immediate OK
    readSerialResponse(5000UL);
    
    // Check for three possible success patterns:
    // 1. "CONNECT OK" with space (ideal case)
    // 2. "OKCONNECT OK" (as seen in your log)
    // 3. "OK" followed by "CONNECT OK" in a separate response
    if (strstr(responseBuffer, "CONNECT OK") || 
        strstr(responseBuffer, "OKCONNECT OK") ||
        strstr(responseBuffer, "OK")) {
      
      // If we just have OK, wait for the CONNECT OK to follow
      if (!strstr(responseBuffer, "CONNECT") && strstr(responseBuffer, "OK")) {
        Serial.println(F("Got OK, waiting for CONNECT OK..."));
        if (!waitForSerialResponsePattern("CONNECT OK", 10000UL)) {
          Serial.println(F("❌ CONNECT OK not received after initial OK"));
          return false;
        }
      }
      
      Serial.println(F("✔ TCP connection successful (SIM7000)"));
      return true;
    }
    
    Serial.print(F("❌ TCP connection failed (SIM7000), response: ")); Serial.println(responseBuffer);
    return false;
  }

  Serial.println(F("❌ Unknown modem type"));
  return false;
}


bool waitForSerialResponsePattern(const char* pattern,
                                  unsigned long totalTimeout = 30000UL,
                                  unsigned long pollInterval = 200UL)
{
  unsigned long start = millis();
  while (millis() - start < totalTimeout) {
    readSerialResponse(pollInterval);
    if (strstr(responseBuffer, pattern)) return true;
    if (strstr(responseBuffer, "ERROR") || strstr(responseBuffer, "+CME ERROR")) {
      Serial.println(F("❌ Error detected while waiting for pattern."));
      return false;
    }
  }
  Serial.println(F("❌ Timeout waiting for expected response."));
  return false;
}


bool tcpSend(const char* payload, uint16_t len)
{
  // 1) Prépare la commande CIPSEND dans scratchBuf
  if (gsmModel == GSM_A7670)
    snprintf_P(scratchBuf, sizeof(scratchBuf), PSTR("AT+CIPSEND=0,%u"), len);
  else
    snprintf_P(scratchBuf, sizeof(scratchBuf), PSTR("AT+CIPSEND=%u"), len);

  // 2) Vide le FIFO avant dʼenvoyer
  clearSerialBuffer();

  // 3) Envoie CIPSEND et attend '>'
  if (!executeSimpleCommand(scratchBuf, ">", 10000UL, 1)) {
    Serial.println(F("❌ CIPSEND prompt timeout"));
    return false;
  }
  Serial.println(F("✔ Got '>' prompt"));

  // 4) Envoie la charge utile suivie de Ctrl‑Z (0x1A)
  clearSerialBuffer();       // important avant d’envoyer les données
  moduleSerial.write((const uint8_t*)payload, len);
  delay(20);                 // donne un peu de temps pour évacuer
  moduleSerial.write(0x1A);  // fin de transmission

  Serial.print(F("▶️ Payload sent (")); Serial.print(len); Serial.println(F(" bytes)"));

if (waitForAnyPattern("SEND OK", "+CIPSEND:", 10000UL)) {
  Serial.println(F("✔ SEND OK"));
  return true;
}


  Serial.println(F("❌ SEND failed — response:"));
  Serial.println(responseBuffer);
  return false;
}

bool waitForAnyPattern(const char* pattern1, const char* pattern2,
                       unsigned long totalTimeout = 10000UL,
                       unsigned long pollInterval = 200UL) {
  unsigned long start = millis();
  while (millis() - start < totalTimeout) {
    readSerialResponse(pollInterval);
    if (strstr(responseBuffer, pattern1) || strstr(responseBuffer, pattern2)) {
      return true;
    }
    if (strstr(responseBuffer, "ERROR") || strstr(responseBuffer, "+CME ERROR")) {
      Serial.println(F("❌ Error detected while waiting for pattern."));
      return false;
    }
  }
  Serial.println(F("❌ Timeout waiting for expected response."));
  return false;
}
bool tcpClose()
{
  // First try standard close
  const char* cmd    = (gsmModel == GSM_A7670) ? "AT+CIPCLOSE=0" : "AT+CIPCLOSE";
  const char* expect = (gsmModel == GSM_A7670) ? "+CIPCLOSE: 0"  : "CLOSE OK";
  
  clearSerialBuffer();
  moduleSerial.println(cmd);
  
  bool ok = waitForSerialResponsePattern(expect, 10000UL);
  
  if (!ok) {
    // If standard close fails, try to force-close everything
    if (gsmModel == GSM_SIM7000) {
      Serial.println(F("Standard close failed, attempting CIPSHUT"));
      executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 3000, 2);
    }
  }
  
  delay(2000);  // Longer delay to ensure connection fully terminates
  return ok;
}

void readSerialResponse(unsigned long waitMillis) {
  unsigned long start = millis();
  memset(responseBuffer, 0, RESPONSE_BUFFER_SIZE);
  responseBufferPos = 0;
  bool anythingReceived = false;

  while (millis() - start < waitMillis) {
    wdt_reset();
    while (moduleSerial.available()) {
      char c = moduleSerial.read();
      if ((uint8_t)c < 32 || (uint8_t)c > 126) continue;
      if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
        responseBuffer[responseBufferPos++] = c;
        responseBuffer[responseBufferPos] = '\0';
        anythingReceived = true;
      } else {
        Serial.println(F("⚠️ overflow, flushing"));
        while (moduleSerial.available()) moduleSerial.read();
        break;
      }
    }
    // Remove early break for +CIPOPEN: to capture full response
//    if (strstr(responseBuffer, "OK") ||
//        strstr(responseBuffer, "ERROR") ||
//        strstr(responseBuffer, "+NETOPEN:") ||
//        strstr(responseBuffer, "+CME ERROR") ||
//        strstr(responseBuffer, ">")) {
//      break;
//    }
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


bool executeSimpleCommand(const char* command,
                          const char* expectedResponse,
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


bool executeSimpleCommand(const __FlashStringHelper* commandFlash,
                          const char* expectedResponse,
                          unsigned long timeoutMillis, uint8_t retries) {
  for (uint8_t i = 0; i < retries; i++) {
    wdt_reset();
    Serial.print(F("Send [")); Serial.print(i + 1); Serial.print(F("]: "));
    Serial.println(commandFlash);

    // Envoie ligne AT en Flash
    moduleSerial.print(commandFlash);
    moduleSerial.print("\r");

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
  while (moduleSerial.available()) {
    moduleSerial.read();
    wdt_reset(); 
  }
  
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

  detectModel();
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
  if (gsmModel == GSM_SIM7000) {
    executeSimpleCommand("AT+CGNSURC=0", "OK", 1000UL, 2);
    executeSimpleCommand("AT+CGNSTST=0", "OK", 1000UL, 2);   
    executeSimpleCommand("AT+CLTS=0", "OK", 1000UL, 2); 
     executeSimpleCommand("AT+CLTS=0", "OK", 1000UL, 2);
  }
  executeSimpleCommand("AT+CGEREP=0,0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CTZU=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CREG=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CEREG=0", "OK", 1000UL, 2);

  return true;
}
bool step1NetworkSettings() {
  Serial.println(F("\n=== STEP 1: Network Settings ==="));
  closeDataStack();

  if (!executeSimpleCommand("AT+CFUN=0", "OK", 1000UL, 1)) {
    Serial.println(F("WARNING: CFUN=0 failed. Continuing..."));
  }

  snprintf_P(scratchBuf, sizeof(scratchBuf), PSTR("AT+CGDCONT=1,\"IP\",\"%s\""), APN);
  if (!executeSimpleCommand(scratchBuf, "OK", 500UL, 3)) {
    Serial.println(F("WARNING: Early APN config failed."));
  }
  bool ok = true;
  ok &= executeSimpleCommand("AT+CNMP=2", "OK", 500UL, 3);

  // Ne faire CMNB=1 que si ce n’est pas un A7670E
  if (gsmModel != GSM_A7670) {
    ok &= executeSimpleCommand("AT+CMNB=1", "OK", 500UL, 3);
  } else {
    Serial.println(F(">> A7670E détecté : saut de AT+CMNB=1"));
  }

  if (ok) {
    Serial.println(F("Turning radio ON (CFUN=1,1)..."));
    moduleSerial.println("AT+CFUN=1,1");
    delay(500);
  }

  return ok;
}

bool waitForSimReady() {
  const uint8_t MAX_RETRIES = 10;
  const unsigned long RETRY_DELAY_MS = 1500UL;

  for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    moduleSerial.println("AT+CPIN?");
    readSerialResponse(1000UL);

    if (strstr(responseBuffer, "+CPIN: READY")) {
      Serial.println(F("SIM Ready."));
      return true;
    }

    Serial.print(F("SIM not ready (try ")); Serial.print(attempt + 1); Serial.println(F(")"));
    delay(RETRY_DELAY_MS);
  }

  Serial.println(F("SIM non prête après plusieurs tentatives."));
  return false;
}




bool step2NetworkRegistration() {
  Serial.println(F("\n=== STEP 2: Network Registration ==="));
  for (uint8_t i = 0; i < 20; i++) {
    wdt_reset();
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
  return openDataStack();
}
bool step4EnableGNSS() {
  Serial.println(F("\n=== STEP 4: Enable GNSS ==="));

  if (gsmModel == GSM_A7670) {
    // A7670E : activer GNSS avec AT+CGNSSPWR=1 seulement
    bool ok = executeSimpleCommand("AT+CGNSSPWR=1", "OK", 1000, 3);
    if (!ok) Serial.println(F("ERROR: Échec d'activation GNSS pour A7670E."));
    return ok;
  }

  if (gsmModel == GSM_SIM7000) {
    // SIM7000G : GNSS via CGNSPWR et sortie via CGNSTST
    bool ok = true;
    ok &= executeSimpleCommand("AT+CGNSPWR=1", "OK", 500, 3);
    ok &= executeSimpleCommand("AT+CGNSTST=0", "OK", 500, 2);  // sortie NMEA vers UART (facultatif)
    if (!ok) Serial.println(F("ERROR: Échec d'activation GNSS pour SIM7000G."));
    delay(1000); // délai pour init GNSS
    return ok;
  }

  Serial.println(F("Modèle inconnu : GNSS non activé."));
  return false;
}
