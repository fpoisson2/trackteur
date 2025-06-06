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

#include "config.h"
#include "logging.h"
#include "common.h"
#include "gsm.h"

// --- PDP / pile data -------------------------------------------------
bool openDataStack()
{
  if (gsmModel == GSM_A7670) {
    // A7670E : APN + NETOPEN
    clearSerialBuffer();
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0"), APN);
    if (!executeSimpleCommand(responseBuffer, "OK", 1000, 3)) return false;


    // NETOPEN avec trois tentatives
     for (uint8_t i = 0; i < 3; i++) {
      moduleSerial.println(F("AT+NETOPEN"));
      readSerialResponse(10000UL);
    
      // Si pas de "+NETOPEN:" dans la première réponse, on attend la suite
      if (!strstr(responseBuffer, "+NETOPEN:")) {
        readSerialResponse(10000UL);  // attend la suite potentielle
      }
    
      if (strstr(responseBuffer, "+NETOPEN: 0") || strstr(responseBuffer, "already opened")) {
        executeSimpleCommand("AT+CIPRXGET=1", "OK", 1000, 2);
        return true;
      }
      delay(500);
    }
    return false;
  }
  if (gsmModel == GSM_SIM7070) {
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CGDCONT=1,\"IP\",\"%s\""), APN);
    if (!executeSimpleCommand(responseBuffer, "OK", 500, 3)) return false;
    return executeSimpleCommand("AT+CNACT=0,1", "ACTIVE", 8000, 2);
  }
  if (gsmModel == GSM_SIM7000) {
    bool ok = true;
  
    ok &= executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 3000, 2);
    ok &= executeSimpleCommand("AT+CIPMUX=0", "OK", 1000, 2);
    ok &= executeSimpleCommand("AT+CIPRXGET=1", "OK", 1000, 2);
  
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CSTT=\"%s\",\"\",\"\""), APN);
    ok &= executeSimpleCommand(responseBuffer, "OK", 1000, 2);
  
    ok &= executeSimpleCommand("AT+CIICR", "OK", 5000, 2);
    ok &= executeSimpleCommand("AT+CIFSR", ".", 3000, 2);  // On vérifie simplement qu’une IP est renvoyée
  
    return ok;
  }

}

bool closeDataStack()
{
  if (gsmModel == GSM_A7670) {
    return executeSimpleCommand("AT+NETCLOSE", "+NETCLOSE:", 3000, 2);
  }
  else if (gsmModel == GSM_SIM7070) {
    // On essaie de fermer la socket (si elle existe)
    executeSimpleCommand("AT+CACLOSE=0", "OK", 2000, 1);  // Ignorer l'échec
    // On tente de désactiver la session PDP même si elle n’est pas active
    //executeSimpleCommand("AT+CNACT=0,0", "OK", 3000, 1);   // Idem : ignorer erreur
    delay(500); 
    return true;  // On considère que c'est "fermé" même si aucune session n'était active
  }
  else {
    return executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 3000, 2);
  }
}


bool tcpOpen(const char* host, uint16_t port)
{
  clearSerialBuffer();

  if (gsmModel == GSM_A7670) {
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CIPOPEN=0,\"TCP\",\"%s\",%u"), host, port);

    moduleSerial.write((const uint8_t*)responseBuffer, strlen(responseBuffer));
    moduleSerial.write('\r');

    delay(500); // stabilité
    if (waitForSerialResponsePattern("+CIPOPEN: 0,0", 30000UL)) {
      INFOLN(F("✔ TCP connection successful (A7670)"));
      return true;
    }
    return false;

   } else if (gsmModel == GSM_SIM7000) {
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CIPSTART=\"TCP\",\"%s\",%u"), host, port);
    moduleSerial.write((const uint8_t*)responseBuffer, strlen(responseBuffer));
    moduleSerial.write('\r');

    delay(500);
    readSerialResponse(2000UL);
    
    // Check for three possible success patterns:
    // 1. "CONNECT OK" with space (ideal case)
    // 2. "OKCONNECT OK" (as seen in your log)
    // 3. "OK" followed by "CONNECT OK" in a separate response
    if (strstr(responseBuffer, "CONNECT OK") || 
        strstr(responseBuffer, "OKCONNECT OK") ||
        strstr(responseBuffer, "OK")) {
      
      // If we just have OK, wait for the CONNECT OK to follow
      if (!strstr(responseBuffer, "CONNECT") && strstr(responseBuffer, "OK")) {
        if (!waitForSerialResponsePattern("CONNECT OK", 10000UL)) {
          return false;
        }
      }
      return true;
  }
   }
  else if (gsmModel == GSM_SIM7070) {
  
    // 3) Vider le buffer
    clearSerialBuffer();
  
    // 4) Construire et envoyer CAOPEN
    snprintf_P(responseBuffer, sizeof(responseBuffer),
               PSTR("AT+CAOPEN=0,0,\"TCP\",\"%s\",%u"),
               host, port);
  
    if (!executeSimpleCommand(responseBuffer, "+CAOPEN: 0,0", 15000UL, 1)) {
      return false;
    }

    executeSimpleCommand("AT+CASTATE?", "", 1000UL, 1);
  
    INFOLN(F("TCP connection successful"));
    return true;
  }
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
      DBGLN(F("❌ Error detected while waiting for pattern."));
      return false;
    }
  }
  DBGLN(F("❌ Timeout waiting for expected response."));
  return false;
}
bool tcpSend(const char* payload, uint16_t len)
{
  clearSerialBuffer();

  // Construction de la commande d’envoi
  if (gsmModel == GSM_A7670) {
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CIPSEND=0,%u"), len);
  } else if (gsmModel == GSM_SIM7070) {
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CASEND=0,%u"), len);
  } else {
    snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CIPSEND=%u"), len);
  }

  moduleSerial.println(responseBuffer);        //  <-- ligne manquante
  //DBGLN(responseBuffer);

  // 2) attendre le caractère '>'
  unsigned long t0 = millis();
  while (millis() - t0 < 30000UL) {
    if (moduleSerial.find(">")) break;        // <- stoppe dès qu'on voit '>'
  }

  if (millis() - t0 >= 30000UL) {
    DBGLN(F("❌ prompt '>' timeout"));
    return false;
  }


  // 3) envoyer le payload tout de suite
  moduleSerial.write((const uint8_t*)payload, len);

  // 4) attendre SEND OK (ou +CASEND)
  // 4) Attendre la confirmation
  if (gsmModel == GSM_SIM7070) {
    return waitForAnyPattern("OK", "+CASEND: 0,0", 10000UL); // ← "OK"
  } else { // SIM7000
    return waitForAnyPattern("SEND OK", "+CIPSEND:", 10000UL);
  }
  return true;
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
      DBGLN(F("❌ Error detected while waiting for pattern."));
      return false;
    }
  }
  DBGLN(F("❌ Timeout waiting for expected response."));
  return false;
}
bool tcpClose() {
  bool ok = false;

  if (gsmModel == GSM_A7670) {
    const char* cmd    = (gsmModel == GSM_A7670) ? "AT+CIPCLOSE=0" : "AT+CIPCLOSE";
    const char* expect = (gsmModel == GSM_A7670) ? "+CIPCLOSE: 0"  : "CLOSE OK";

    clearSerialBuffer();
    moduleSerial.println(cmd);
    ok = waitForSerialResponsePattern(expect, 10000UL);

    if (!ok && gsmModel == GSM_SIM7000) {
      DBGLN(F("Standard close failed, attempting CIPSHUT"));
      ok = executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 3000, 2);
    }
  }
  else if (gsmModel == GSM_SIM7070) {
    // On saute toute tentative CIPCLOSE, non supportée
    ok = executeSimpleCommand("AT+CACLOSE=0", "OK", 2000, 2);
    delay(500); 
  }

  delay(1500);  // Suffisant pour garantir la fermeture sans bloquer trop
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
      if ((uint8_t)c < 1 || c == 127) continue;
      if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
        responseBuffer[responseBufferPos++] = c;
        responseBuffer[responseBufferPos] = '\0';
        anythingReceived = true;
      } else {
        DBGLN(F("⚠️ overflow, flushing"));
        while (moduleSerial.available()) moduleSerial.read();
        break;
      }
    }

    if (!moduleSerial.available()) delay(5);
  }

  if (responseBufferPos > 0) {
    DBG(F("Rcvd: ["));
    for (uint8_t i = 0; i < responseBufferPos; i++) {
      char c = responseBuffer[i];
      if (c == '\r') continue;
      else if (c == '\n') DBG(F("<LF>"));
      else if (isprint(c)) DBG(c);
      else DBG('.');
    }
    DBGLN(F("]"));
  } else if (anythingReceived) {
    DBGLN(F("Rcvd: [Empty/Discarded]"));
  }
}


bool executeSimpleCommand(const char* command,
                          const char* expectedResponse,
                          unsigned long timeoutMillis, uint8_t retries) {
  for (uint8_t i = 0; i < retries; i++) {
    wdt_reset();
    DBG(F("Send: ")); DBGLN(command);

    moduleSerial.println(command);
    readSerialResponse(timeoutMillis);
    DBG(F("AT< "));
    DBG(responseBuffer);
    if (strstr(responseBuffer, expectedResponse)) {
      return true;
    }
    if (i < retries - 1) delay(500);
  }
  return false;
}


bool executeSimpleCommand(const __FlashStringHelper* commandFlash,
                          const char* expectedResponse,
                          unsigned long timeoutMillis, uint8_t retries) {
  for (uint8_t i = 0; i < retries; i++) {
    wdt_reset();
    DBG(F("Send [")); DBG(i + 1); DBG(F("]: "));

    // Envoie ligne AT en Flash
    moduleSerial.print(commandFlash);
    moduleSerial.print("\r");

    readSerialResponse(timeoutMillis);
    DBG(F("AT< "));
    DBG(responseBuffer);
    if (strstr(responseBuffer, expectedResponse)) {
      return true;
    }
    if (i < retries - 1) delay(500);
  }
  return false;
}


bool waitForInitialOK(uint8_t maxRetries) {
  for (uint8_t i = 0; i < maxRetries; i++) {
    DBG(F("AT (Try ")); DBG(i + 1); DBG(F(")... "));
    moduleSerial.println("AT");
    readSerialResponse(1000UL);
    if (strstr(responseBuffer, "OK")) {
      DBGLN(F("OK."));
      return true;
    }
    DBGLN(F("No OK."));
    delay(500);
  }
  return false;
}

void resetGsmModule() {
  digitalWrite(powerPin, LOW);
  delay(2000);
  while (moduleSerial.available()) moduleSerial.read(); // clear serial buffer
  digitalWrite(powerPin, HIGH);
  delay(5000);

  if (!waitForInitialOK(10)) {
    DBGLN(F("  ERROR: module still unresponsive after reset"));
  } else {
    DBGLN(F("  GSM module is back online"));
    executeSimpleCommand(F("ATE0"), "OK", 1000, 2);
    executeSimpleCommand(F("AT+CMEE=2"), "OK", 1000, 2);
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
  moduleSerial.begin(moduleBaudRate);
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
  delay(1200);
  digitalWrite(powerPin, HIGH);
  delay(300);
  digitalWrite(powerPin, LOW);
  delay(5000);
}

bool initialAT() {
  DBGLN(F("Attempting initial communication..."));
  if (!waitForInitialOK(15)) {
    DBGLN(F("FATAL: Module unresponsive."));
    return false;
  }
}

bool initialCommunication() {
  executeSimpleCommand(F("ATE0"), "OK", 1000UL, 2);
  executeSimpleCommand(F("AT+CMEE=2"), "OK", 1000UL, 2);

  // Config spécifique SIM7000
  if (gsmModel == GSM_SIM7000) {
    executeSimpleCommand(F("AT+CGNSURC=0"), "OK", 1000UL, 2);
    executeSimpleCommand(F("AT+CGNSTST=0"), "OK", 1000UL, 2);   
    executeSimpleCommand(F("AT+CLTS=0"), "OK", 1000UL, 2); 
    // Activation bandes LTE et GSM
    executeSimpleCommand(F("AT+CBANDCFG=\"CAT-M\",1,2,3,4,5,8,12,13,18,19,20,26,28,39"), "OK", 2000UL, 2);
  }

  // Config spécifique SIM7070G
  else if (gsmModel == GSM_SIM7070) {
    // Selon firmware : parfois CBANDCFG fonctionne, parfois non
    executeSimpleCommand("AT+CBANDCFG=\"CAT-M\",1,2,3,4,5,8,12,13,18,19,20,25,26,28,66,71,85", "OK", 2000UL, 2);
  }

  executeSimpleCommand("AT+CGEREP=0,0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CTZU=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CREG=0", "OK", 1000UL, 2);
  executeSimpleCommand("AT+CEREG=0", "OK", 1000UL, 2);

  return true;
}


bool step1NetworkSettings() {
  INFOLN(F("Configuration du réseau"));
  closeDataStack();

  //if (!executeSimpleCommand("AT+CFUN=0", "OK", 1000UL, 1)) {
  //  DBGLN(F("WARNING: CFUN=0 failed. Continuing..."));
  //}

  snprintf_P(responseBuffer, sizeof(responseBuffer), PSTR("AT+CGDCONT=1,\"IP\",\"%s\""), APN);
  if (!executeSimpleCommand(responseBuffer, "OK", 500UL, 3)) {
    DBGLN(F("WARNING: Early APN config failed."));
  }
  bool ok = true;


  // Ne faire CMNB=1 que si ce n’est pas un A7670E
  if (gsmModel != GSM_A7670) {
    ok &= executeSimpleCommand("AT+CNMP=13", "OK", 500UL, 3);
    //ok &= executeSimpleCommand("AT+CMNB=0", "OK", 500UL, 3);
  }
  else {
      ok &= executeSimpleCommand("AT+CNMP=2", "OK", 500UL, 3);
  }

  if (ok) {
    DBGLN(F("Turning radio ON..."));
    moduleSerial.println(F("AT+CFUN=1,1"));
    delay(500);
  }

  return ok;
}

bool waitForSimReady() {
  const uint8_t MAX_RETRIES = 10;
  const unsigned long RETRY_DELAY_MS = 1500UL;

  for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    moduleSerial.println(F("AT+CPIN?"));
    readSerialResponse(1000UL);

    if (strstr(responseBuffer, "+CPIN: READY")) {
      INFOLN(F("SIM prête"));
      return true;
    }

    DBG(F("SIM pas prete")); DBG(attempt + 1); DBGLN(F(")"));
    delay(RETRY_DELAY_MS);
  }

  INFOLN(F("SIM non prête après plusieurs tentatives."));
  return false;
}




bool step2NetworkRegistration() {
  DBGLN(F("\n=== STEP 2: Network Registration ==="));
  for (uint8_t i = 0; i < 20; i++) {
    wdt_reset();
    DBG(F("Reg check ")); DBG(i + 1); DBGLN(F("..."));
    executeSimpleCommand("AT+CSQ", "+CSQ", 500UL, 1);
    executeSimpleCommand("AT+COPS?", "+COPS", 3000UL, 1);
    executeSimpleCommand("AT+CREG?", "+CREG:", 500UL, 1);
    bool creg_ok = strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5");
    executeSimpleCommand("AT+CEREG?", "+CEREG:", 500UL, 1);
    bool cereg_ok = strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5");
    if (creg_ok || cereg_ok) {
      DBGLN(F("Registered."));
      return true;
    }
    delay(2000);
  }
  DBGLN(F("ERREUR: enregistrement au reseau."));
  return false;
}

bool step3PDPContext() {
  return openDataStack();
}

bool step4EnableGNSS() {
  DBGLN(F("\n=== Enable GNSS ==="));

  if (gsmModel == GSM_A7670) {
    executeSimpleCommand("AT+CGNSSPWR=0", "OK", 1000, 3);
    delay(500);
    // A7670E : activer GNSS avec AT+CGNSSPWR=1 seulement
    bool ok = executeSimpleCommand("AT+CGNSSPWR=1", "OK", 1000, 3);
    if (!ok) INFOLN(F("ERROR: Échec d'activation GNSS pour A7670E."));
    return ok;
  }

  if (gsmModel == GSM_SIM7000 || gsmModel == GSM_SIM7070)  {
    executeSimpleCommand("AT+CGNSPWR=0", "OK", 1000, 3);
    // SIM7000G : GNSS via CGNSPWR et sortie via CGNSTST
    bool ok = true;
    ok &= executeSimpleCommand("AT+CGNSPWR=1", "OK", 500, 3);
    if (!ok) {
      DBGLN(F("⚠ CGNSPWR=1 échoué. Tentative de récupération..."));
      closeDataStack();
      delay(1500);
      ok = executeSimpleCommand("AT+CGNSPWR=1", "OK", 500, 3);
      if (!ok) {
        DBGLN(F("❌ GNSS toujours bloqué. Module à redémarrer."));
        PowerOff(); while(true);  // ou marque l'état OFF et attends dans loop()
      }
    }
    delay(1000); // délai pour init GNSS
    return ok;
  }

  INFOLN(F("Modèle inconnu : GNSS non activé."));
  return false;
}

bool disableGNSS() {
  DBGLN(F("\n=== Disable GNSS ==="));

  if (gsmModel == GSM_A7670) {
    // A7670E : désactiver GNSS avec AT+CGNSSPWR=0
    bool ok = executeSimpleCommand("AT+CGNSSPWR=0", "OK", 1000, 3);
    if (!ok) INFOLN(F("ERROR: Échec de désactivation GNSS pour A7670E."));
    return ok;
  }

  if (gsmModel == GSM_SIM7000 || gsmModel == GSM_SIM7070) {
    // SIM7000G / SIM7070G : désactivation via CGNSPWR=0
    bool ok = executeSimpleCommand("AT+CGNSPWR=0", "OK", 500, 3);
    if (!ok) INFOLN(F("ERROR: Échec de désactivation GNSS pour SIM7xxx."));
    delay(500);  // laisse le temps au module
    return ok;
  }

  DBGLN(F("Modèle inconnu : GNSS non désactivé."));
  return false;
}

void PowerOff() {
          executeSimpleCommand(F("AT+CPOF"), "OK", 5000, 2);
        executeSimpleCommand(F("AT+CPOWD=1"), "OK", 5000, 2);
}
