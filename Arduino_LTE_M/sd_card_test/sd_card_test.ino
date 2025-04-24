#include <SPI.h>
#include <SD.h>

const uint8_t chipSelect = 8;  // SD card CS pin

void setup() {
  Serial.begin(9600);
  while (!Serial) { /* wait for Serial on Leonardo/Micro */ }

  Serial.print(F("Initializing SD card... "));
  if (!SD.begin(chipSelect)) {
    Serial.println(F("initialization failed!"));
    while (1) {
      // halt here
    }
  }
  Serial.println(F("initialization done."));

  // 1) Write to a file
  Serial.print(F("Opening test.txt for writing... "));
  File testFile = SD.open("test.txt", FILE_WRITE);
  if (testFile) {
    testFile.println(F("Hello from Arduino SD test!"));
    testFile.close();
    Serial.println(F("done."));
  } else {
    Serial.println(F("failed to open file for writing"));
  }

  // 2) Read the file back
  Serial.print(F("Reading test.txt: "));
  testFile = SD.open("test.txt");
  if (testFile) {
    while (testFile.available()) {
      Serial.write(testFile.read());
    }
    testFile.close();
    Serial.println(); // newline after content
  } else {
    Serial.println(F("failed to open file for reading"));
  }

  // 3) List all files in root
  Serial.println(F("Files on SD card:"));
  listRootFiles();
}

void loop() {
  // nothing to do here
}

void listRootFiles() {
  File root = SD.open("/");
  printDirectory(root, 0);
  root.close();
}

void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println(F("/"));
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print(F("\t\t"));
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
