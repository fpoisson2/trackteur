/* =======================================================================
 * Projet      : Traceur GPS LTE avec SD et Traccar
 * Fichier     : sdlog.cpp
 * Description : Gère le journalisation des positions GPS sur carte SD à
 *               l’aide de Petit FatFs (via PF). Implémente une stratégie
 *               sectorielle avec marquage `!` (à envoyer) et `#` (envoyé).
 *               Fournit les fonctions pour :
 *                 - Initialiser la SD
 *                 - Sauvegarder/charger les métadonnées
 *                 - Logger une position
 *                 - Réenvoyer les logs non transmis
 *                 - Nettoyer les secteurs à l’amorçage
 *
 * Auteur      : Francis Poisson-Gagnon
 * Dépôt Git   : https://github.com/fpoisson2/trackteur
 * Matériel    : Arduino Nano (ATmega328P) + SIM7000G + carte SD
 * ======================================================================= */

#include "config.h"
#include "logging.h"
#include "sdlog.h"

void saveLogMetadata(uint32_t currentIndex) {
  wdt_reset(); 
  FRESULT res = PF.open(LOG_FILE);
  if (res != FR_OK) {
    DBGLN(F("PF.open (metadata) failed"));
    return;
  }
  PF.seek(0);  // secteur 0 réservé

  struct {
    uint32_t index;
    char signature[8];
  } meta = { currentIndex, "LOGDATA" };

  UINT bw;
  wdt_reset(); 
  res = PF.writeFile(&meta, sizeof(meta), &bw);
  if (res != FR_OK || bw != sizeof(meta)) {
    DBGLN(F("Échec écriture metadata."));
    return;
  }

  PF.writeFile(nullptr, 0, &bw); // flush
  DBGLN(F("Metadata mise à jour."));
}

uint32_t loadLogMetadata() {
  PF.open(LOG_FILE);
  PF.seek(0);

  struct {
    uint32_t index;
    char signature[8];
  } meta;

  UINT br;
  FRESULT res = PF.readFile(&meta, sizeof(meta), &br);

  if (res != FR_OK || br != sizeof(meta)) {
    DBGLN(F("Lecture metadata échouée. Réinitialise index à 1."));
    lastSectorUsed = 0;
    saveLogMetadata(1);
  }

  if (strncmp(meta.signature, "LOGDATA", 7) != 0) {
    DBGLN(F("Signature invalide. Réinitialise index à 1."));
    lastSectorUsed = 0;
    saveLogMetadata(1);
  }

  if (meta.index == 0 || meta.index > MAX_SECTORS) {
    DBGLN(F("Index corrompu. Réinitialise index à 1."));
    lastSectorUsed = 0;
    saveLogMetadata(1);
  }

  lastSectorUsed = meta.index - 1;
  return meta.index;
}


void resendLastLog() {
  if (!sdAvailable) {
    DBGLN(F("SD non disponible. Ignoré."));
    return;
  }
  if (lastSectorUsed < 1) return;
  wdt_reset();

  uint32_t s = lastSectorUsed;
  // Ouvre le fichier de log
  if (PF.open(LOG_FILE) != FR_OK) {
    DBG(F("Impossible d'ouvrir pour renvoi secteur "));
    DBGLN(s);
    return;
  }

  // Positionne au début du secteur
  PF.seek(s * 512UL);

  // Lit la ligne (max 127 bytes + '\0')
  char buf[128];
  UINT br;
  FRESULT res = PF.readFile(buf, sizeof(buf) - 1, &br);
  if (res != FR_OK || br < 10) {
    DBG(F("Secteur ")); DBG(s); DBGLN(F(" vide ou erreur."));
    lastSectorUsed--;
    saveLogMetadata(lastSectorUsed + 1);
    return;
  }
  buf[br] = '\0';

  // Affiche le contenu à renvoyer
  DBG(F("→ Renvoi secteur ")); DBG(s);
  DBG(F(" : «")); DBG(buf); DBGLN(F("»"));

  // Si déjà marqué envoyé, décrémente et quitte
  if (buf[0] == '#') {
    DBGLN(F("    Déjà envoyé."));
    lastSectorUsed--;
    saveLogMetadata(lastSectorUsed);
    return;
  }

  // Extrait timestamp, lat, lon
  char* p = buf;
  if (*p == '!' || *p == '#') p++;
  char ts[32];
  float lat, lon;
  char* tok = strtok(p, ",");
  if (!tok) { lastSectorUsed--; return; }
  strncpy(ts, tok, sizeof(ts)); ts[sizeof(ts)-1] = '\0';
  tok = strtok(nullptr, ",");
  if (!tok) { lastSectorUsed--; return; }
  lat = atof(tok);
  tok = strtok(nullptr, ",");
  if (!tok) { lastSectorUsed--; return; }
  lon = atof(tok);

  wdt_reset();
  // Tente l'envoi réseau
  if (sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, lat, lon, ts)) {
    DBGLN(F("    Renvoi OK."));
    consecutiveNetFails = 0;

    // Marque le secteur comme envoyé en réécrivant juste '#'
    PF.seek(s * 512UL);
    char mark = '#';
    PF.writeFile(&mark, 1, &br);
    PF.writeFile(nullptr, 0, &br);  // flush

    lastSectorUsed--;
    saveLogMetadata(lastSectorUsed + 1);
  } else {
    // Échec : incremente compteur et éventuellement reset du module
    consecutiveNetFails++;
    DBG(F("    Échec renvoi (#")); DBG(consecutiveNetFails); DBGLN(F(")"));
    if (consecutiveNetFails >= NET_FAIL_THRESHOLD) {
      resetGsmModule();
      consecutiveNetFails = 0;
    }
  }
}



void logRealPositionToSd(float lat, float lon, const char* ts) {
  if (!sdAvailable) {
    DBGLN(F("SD non disponible. Ignoré."));
    return;
  }
  char latStr[15], lonStr[15];
  dtostrf(lat, 4, 6, latStr);
  dtostrf(lon, 4, 6, lonStr);

  char line[64];
  uint16_t len = snprintf(line, sizeof(line), "!%s,%s,%s\n", ts, latStr, lonStr);

  bool isReusable = false;
  if (PF.open(LOG_FILE) == FR_OK) {
    PF.seek(sectorIndex * 512UL);
    char firstByte;
    UINT br;
    if (PF.readFile(&firstByte, 1, &br) == FR_OK && br == 1 && firstByte == '#') {
      isReusable = true;
    }
  }

  if (!isReusable && sectorIndex <= lastSectorUsed) {
    DBGLN(F("Secteur non réutilisable, log ignoré."));
    return;
  }

  FRESULT res = PF.open(LOG_FILE);
  if (res != FR_OK) return;
  PF.seek(sectorIndex * 512UL);

  UINT bw;
  res = PF.writeFile(line, len, &bw);
  if (res != FR_OK || bw != len) {
    DBGLN(F("Erreur écriture ligne."));
    return;
  }

  const uint8_t zeros[16] = {0};
  for (uint16_t i = len; i < 512; i += 16) {
    uint16_t chunk = (512 - i < 16) ? (512 - i) : 16;
    res = PF.writeFile(zeros, chunk, &bw);
    if (res != FR_OK || bw != chunk) {
      DBGLN(F("Erreur padding zéro."));
      return;
    }
  }

  PF.writeFile(nullptr, 0, &bw);
  delay(100);

  uint32_t startS = (sectorIndex > HISTORY) ? sectorIndex - HISTORY : 1;
  for (uint32_t s = startS; s <= sectorIndex; s++) {
    res = PF.open(LOG_FILE);
    if (res != FR_OK) return;
    PF.seek(s * 512UL);
    char buf[64]; UINT br;
    res = PF.readFile(buf, len, &br);
    if (res != FR_OK) return;

    DBG(F("Secteur ")); DBG(s); DBG(F(" : "));
    Serial.write((uint8_t*)buf, br); DBGLN();
  }

  if (sectorIndex > lastSectorUsed) {
    lastSectorUsed = sectorIndex;
    saveLogMetadata(lastSectorUsed);
  }

  sectorIndex++;
  if (sectorIndex > MAX_SECTORS) sectorIndex = 1;
}


void initializeSD() {
  pinMode(SD_CS_PIN, OUTPUT);
  SPI.begin();

  FRESULT res = PF.begin(&fs);
  if (res != FR_OK) {
    INFOLN(F("Échec de montage de la carte SD."));
    sdAvailable = false;
  } else {
    INFOLN(F("Carte SD montée avec succès."));
    sdAvailable = true;
  }
}
