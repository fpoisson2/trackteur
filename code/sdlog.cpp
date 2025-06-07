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
#include "gsm.h"

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
    lastSectorUsed = 0;
    saveLogMetadata(1);
  }

  if (strncmp(meta.signature, "LOGDATA", 7) != 0) {
    lastSectorUsed = 0;
    saveLogMetadata(1);
  }

  if (meta.index == 0 || meta.index > MAX_SECTORS) {
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

  wdt_reset();

  // Balayage à l'envers pour trouver le dernier secteur marqué '!'
  uint32_t found = 0;
  for (int32_t s = lastSectorUsed; s >= 1; s--) {
    if (PF.open(LOG_FILE) != FR_OK) continue;
    PF.seek(s * 512UL);
    char mark;
    UINT br;
    if (PF.readFile(&mark, 1, &br) == FR_OK && br == 1 && mark == '!') {
      found = s;
      break;
    }
  }

  if (found == 0) {
    DBGLN(F("Aucun secteur à renvoyer (!)."));
    return;
  }

  uint32_t s = found;

  // Lit la ligne complète
  PF.seek(s * 512UL);
  char buf[128];
  UINT br;
  if (PF.readFile(buf, sizeof(buf) - 1, &br) != FR_OK || br < 10) {
    DBG(F("Secteur ")); DBG(s); DBGLN(F(" vide ou invalide."));
    return;
  }
  buf[br] = '\0';

  INFO(F("→ Renvoi secteur ")); DBG(s); INFO(F(" : «")); DBG(buf); DBGLN(F("»"));

  // Parse et envoie
  char* p = buf;
  if (*p == '!' || *p == '#') p++;
  char ts[32]; float lat, lon;
  char* tok = strtok(p, ","); if (!tok) return;
  strncpy(ts, tok, sizeof(ts)); ts[sizeof(ts)-1] = '\0';
  tok = strtok(nullptr, ","); if (!tok) return;
  lat = atof(tok);
  tok = strtok(nullptr, ","); if (!tok) return;
  lon = atof(tok);

  wdt_reset();

  if (sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, lat, lon, ts)) {
    DBGLN(F("    Renvoi OK."));

    PF.seek(s * 512UL);
    char mark = '#';
    PF.writeFile(&mark, 1, &br);
    PF.writeFile(nullptr, 0, &br);

    sectorIndex = s;   

    lastSectorUsed = max(lastSectorUsed, s);  // conserve le plus haut atteint
    saveLogMetadata(lastSectorUsed + 1);
    consecutiveNetFails = 0;
  } else {
    INFO(F("    Échec renvoi (#")); DBG(++consecutiveNetFails); DBGLN(F(")"));
    if (consecutiveNetFails >= NET_FAIL_THRESHOLD) {
      PowerOff(); while (true);
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
  uint32_t startSector = sectorIndex;
  
  // Vérifie d'abord si le secteur actuel est réutilisable
  if (PF.open(LOG_FILE) == FR_OK) {
    PF.seek(sectorIndex * 512UL);
    char firstByte;
    UINT br;
    if (PF.readFile(&firstByte, 1, &br) == FR_OK && br == 1 && firstByte == '#') {
      isReusable = true;
    }
  }
  
  // Si non réutilisable, cherche un secteur `#` dans la plage valide
  if (!isReusable) {
    DBGLN(F("Secteur non réutilisable, recherche..."));
    for (uint32_t s = 1; s <= lastSectorUsed; s++) {
      PF.seek(s * 512UL);
      char mark;
      UINT br;
      if (PF.readFile(&mark, 1, &br) == FR_OK && br == 1 && mark == '#') {
        sectorIndex = s;
        isReusable = true;
        DBGLN(F("Secteur recyclable trouvé à "));
        DBGLN(sectorIndex);
        break;
      }
    }
  }
  
  if (!isReusable && sectorIndex <= lastSectorUsed) {
    DBGLN(F("Aucun secteur recyclable disponible. Log ignoré."));
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

    INFO(F("Secteur ")); INFO(s); INFO(F(" : "));
    Serial.write((uint8_t*)buf, br); DBGLN();
  }

  /*--- mise à jour de l’index et de la méta-donnée ---*/
  lastSectorUsed = max(lastSectorUsed, sectorIndex);   // ne jamais diminuer

  uint32_t nextSector = sectorIndex + 1;               // secteur où l’on écrira
  if (nextSector > MAX_SECTORS) nextSector = 1;        // rotation circulaire

  saveLogMetadata(nextSector);                         // toujours sauver le pointeur
  sectorIndex = nextSector;                            // et l’utiliser immédiatement

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
