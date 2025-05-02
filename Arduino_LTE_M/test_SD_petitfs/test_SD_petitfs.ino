/*
  AppendGPSLog.ino
  ----------------
  Write "Line N" at the start of sector N in GPS_LOG.CSV using Petit FatFs,
  then read it back and print via Serial. Each iteration advances to the next sector
  and, after writing, rereads the last HISTORY sectors.
*/

#include <SPI.h>
#include <PF.h>
#include <diskio.h>

#define SD_CS_PIN 8
#define HISTORY 10  // number of previous sectors to reread

FATFS fs;
const char* filename = "GPS_LOG.CSV";

uint32_t sectorIndex = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  pinMode(SD_CS_PIN, OUTPUT);
  SPI.begin();
  Serial.println(F("SPI initted"));

  FRESULT res = PF.begin(&fs);
  if (res != FR_OK) {
    Serial.print(F("PF.begin failed: "));
    Serial.println(res, DEC);
    while (1);
  }
  Serial.println(F("Petit FatFs mounted"));
}

void loop() {
  // Prepare the line: "Line N\n"
  char line[32];
  uint16_t len = snprintf(line, sizeof(line), "Line %lu\n", sectorIndex);

  // Write at sectorIndex
  {
    FRESULT res = PF.open(filename);
    if (res != FR_OK) { Serial.print(F("PF.open failed: ")); Serial.println(res, DEC); while (1); }

    uint32_t sectorStart = sectorIndex * 512UL;
    Serial.print(F("Writing at sector "));
    Serial.print(sectorIndex);
    Serial.print(F(" (offset "));
    Serial.print(sectorStart);
    Serial.println(F(")"));

    UINT bw;
    PF.seek(sectorStart);
    res = PF.writeFile(line, len, &bw);
    if (res != FR_OK) { Serial.println(F("writeFile error")); while (1); }
    PF.writeFile(nullptr, 0, &bw);

    Serial.print(F("Wrote: "));
    Serial.print(line);
  }

  // After writing, reread the last HISTORY sectors including current
  uint32_t startSector = (sectorIndex > HISTORY ? sectorIndex - HISTORY : 0);
  for (uint32_t s = startSector; s <= sectorIndex; s++) {
    FRESULT res = PF.open(filename);
    if (res != FR_OK) { Serial.print(F("PF.open verify failed: ")); Serial.println(res, DEC); while (1); }

    uint32_t secOff = s * 512UL;
    PF.seek(secOff);
    char readBuf[32];
    UINT br;
    res = PF.readFile(readBuf, len, &br);
    if (res != FR_OK) { Serial.println(F("readFile error")); while (1); }

    Serial.print(F("Sector "));
    Serial.print(s);
    Serial.print(F(" read: "));
    Serial.write((uint8_t*)readBuf, br);
    Serial.println();
  }

  sectorIndex++;
  delay(1000);
}
