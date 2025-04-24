#include <SoftwareSerial.h>

// --- Pin Definitions ---
const int powerPin = 2;       
const int swRxPin  = 3;       
const int swTxPin  = 4;       

SoftwareSerial moduleSerial(swRxPin, swTxPin);

// --- Baud Rate ---
const long moduleBaud = 9600;

// --- Buffer pour les réponses AT ---
#define BUF_SIZE 256
char respBuf[BUF_SIZE];
uint16_t respPos = 0;

// --- Fonctions utilitaires ---
void clearBuf() {
  respPos = 0;
  memset(respBuf, 0, BUF_SIZE);
}

void readResponse(unsigned long timeout) {
  unsigned long t0 = millis();
  clearBuf();
  while (millis() - t0 < timeout) {
    while (moduleSerial.available()) {
      if (respPos < BUF_SIZE - 1) {
        char c = moduleSerial.read();
        respBuf[respPos++] = c;
      } else {
        moduleSerial.read();
      }
    }
  }
  // Affiche la réponse brute
  Serial.print(F("Réponse brute ("));
  Serial.print(respPos);
  Serial.print(F(" bytes): "));
  for (uint16_t i = 0; i < respPos; i++) {
    char c = respBuf[i];
    if (c == '\r') Serial.print("<CR>");
    else if (c == '\n') Serial.print("<LF>");
    else Serial.print(c);
  }
  Serial.println();
}

// Envoie un AT et affiche l’output
bool atCommand(const char* cmd, const char* expect, unsigned long timeout=1000) {
  Serial.print(F(">> Envoi AT: "));
  Serial.println(cmd);
  moduleSerial.println(cmd);
  readResponse(timeout);
  bool ok = strstr(respBuf, expect) != nullptr;
  Serial.print(F("   -> trouve \""));
  Serial.print(expect);
  Serial.print(F("\" ? "));
  Serial.println(ok ? F("OUI") : F("NON"));
  return ok;
}

// Essaie d'obtenir un fix GPS
bool tryGetGpsFix(float &lat, float &lon) {
  Serial.println(F(">> Lecture AT+CGNSINF"));
  moduleSerial.println("AT+CGNSINF");
  readResponse(2000);

  char* p = strstr(respBuf, "+CGNSINF:");
  if (!p) {
    Serial.println(F("   !! +CGNSINF: non trouvé"));
    return false;
  }
  Serial.println(F("   +CGNSINF trouvé"));

  int run, fix;
  if (sscanf(p + 9, "%d,%d", &run, &fix) < 2) {
    Serial.println(F("   !! Impossible de parser run/fix"));
    return false;
  }
  Serial.print(F("   run=")); Serial.print(run);
  Serial.print(F(", fix=")); Serial.println(fix);
  if (run != 1 || fix != 1) {
    Serial.println(F("   Pas de fix valide"));
    return false;
  }

  // Extraction lat/lon
  char* fld = p;
  for (int i = 0; i < 3; i++) {
    fld = strchr(fld + 1, ',');
    if (!fld) {
      Serial.print(F("   !! virgule "));
      Serial.print(i+1);
      Serial.println(F(" non trouvée"));
      return false;
    }
  }
  lat = atof(fld + 1);
  fld = strchr(fld + 1, ',');
  if (!fld) {
    Serial.println(F("   !! virgule après lat non trouvée"));
    return false;
  }
  lon = atof(fld + 1);
  Serial.print(F("   Parsed lat="));
  Serial.print(lat, 6);
  Serial.print(F(", lon="));
  Serial.println(lon, 6);
  return true;
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {;}

  pinMode(powerPin, OUTPUT);
  Serial.println(F("→ Alimentation module ON"));
  digitalWrite(powerPin, HIGH);
  delay(5000);

  moduleSerial.begin(moduleBaud);
  Serial.println(F("→ Activation GNSS"));
  if (!atCommand("AT+CGNSPWR=1", "OK", 2000)) {
    Serial.println(F("!! ERREUR: AT+CGNSPWR"));
  }
}

void loop() {
  static unsigned long last = 0;
  const unsigned long interval = 3000;

  if (millis() - last >= interval) {
    last = millis();
    float lat, lon;
    Serial.println(F("\n=== Nouvelle tentative GPS ==="));
    if (tryGetGpsFix(lat, lon)) {
      Serial.print(F(">>> FIX obtenu: "));
      Serial.print(lat, 6);
      Serial.print(F(" , "));
      Serial.println(lon, 6);
      // Vous pouvez commenter la ligne suivante pour laisser tourner
      while (1) { delay(1000); }
    } else {
      Serial.println(F(">>> Pas de fix, réessaie dans 3s"));
    }
  }
}
