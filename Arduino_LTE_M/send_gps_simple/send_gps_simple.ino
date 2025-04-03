#include <SoftwareSerial.h>
#include <stdio.h>  // Include for snprintf
#include <string.h> // Include for strstr, memset, strchr, sscanf
#include <stdlib.h> // Include for dtostrf, atof

// --- Pin Definitions ---
const int powerPin = 2;
const int swRxPin = 3;
const int swTxPin = 4;

// --- Software Serial Object ---
SoftwareSerial moduleSerial(swRxPin, swTxPin);

// --- Baud Rate ---
const long moduleBaudRate = 9600;

// --- Network Configuration ---
const char* APN = "em";

// --- Traccar Configuration ---
const char* TRACCAR_HOST = "trackteur.ve2fpd.com";
const int TRACCAR_PORT = 5055;
const char* DEVICE_ID = "212910"; // <<< IMPORTANT: Use YOUR Traccar Device ID

// --- Timing for Sending Data ---
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 60000;

// --- Buffers (Reduced Sizes) ---
#define RESPONSE_BUFFER_SIZE 256 // Reduced from 300
char responseBuffer[RESPONSE_BUFFER_SIZE];
uint16_t responseBufferPos = 0;

#define CMD_BUFFER_SIZE 128 // Reduced from 150
char cmdBuffer[CMD_BUFFER_SIZE];

#define HTTP_REQUEST_BUFFER_SIZE 320 // Reduced from 350
char httpRequestBuffer[HTTP_REQUEST_BUFFER_SIZE];

// --- GPS Data Storage ---
float currentLat = 0.0;
float currentLon = 0.0;
#define GPS_TIMESTAMP_TRACCAR_BUF_SIZE 25 // YYYY-MM-DD%20HH:MM:SS + null
char gpsTimestampTraccar[GPS_TIMESTAMP_TRACCAR_BUF_SIZE];

// --- Global State ---
bool setupSuccess = false;

// --- Function Declarations ---
bool waitForInitialOK(int maxRetries);
bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries);
void clearSerialBuffer();
void readSerialResponse(unsigned long waitMillis);
bool getGpsData(float &lat, float &lon, char* timestampOutput); // Corrected function included below
bool sendGpsToTraccar(const char* host, int port, const char* deviceId, float lat, float lon, const char* timestampStr);


// ========================================================================
// SETUP
// ========================================================================
void setup() {
  setupSuccess = false;
  bool currentStepSuccess = true;

  Serial.begin(9600);
  while (!Serial) { ; }
  Serial.println(F("--- Arduino Initialized ---"));

  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
  Serial.println(F("Module power pin configured (D2)."));

  moduleSerial.begin(moduleBaudRate);
  Serial.print(F("Software Serial initialized on Pins RX:"));
  Serial.print(swRxPin); Serial.print(F(", TX:")); Serial.print(swTxPin);
  Serial.print(F(" at ")); Serial.print(moduleBaudRate); Serial.println(F(" baud."));

  Serial.println(F("Turning module ON..."));
  digitalWrite(powerPin, HIGH);
  delay(5000);
  Serial.println(F("Module boot wait complete."));

  Serial.println(F("Attempting initial communication..."));
  if (!waitForInitialOK(15)) {
    Serial.println(F("FATAL: Module unresponsive."));
    currentStepSuccess = false;
  } else {
      Serial.println(F("Initial communication OK."));
      executeSimpleCommand("ATE0", "OK", 1000, 2);
      executeSimpleCommand("AT+CMEE=2", "OK", 1000, 2);
  }

  // STEP 1: Network Config
  if (currentStepSuccess) {
    Serial.println(F("\n=== STEP 1: Network Settings ==="));
    executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 2);
    currentStepSuccess &= executeSimpleCommand("AT+CFUN=0", "OK", 5000, 3);
    currentStepSuccess &= executeSimpleCommand("AT+CNMP=38", "OK", 2000, 3);
    currentStepSuccess &= executeSimpleCommand("AT+CMNB=1", "OK", 2000, 3);
  }
  // Reboot with CFUN=1,1
  if (currentStepSuccess) {
      Serial.println(F("Turning radio ON (CFUN=1,1)..."));
      moduleSerial.println("AT+CFUN=1,1");
  } else { Serial.println(F("Skipping CFUN=1.")); }

  // Wait and Re-check after reboot
  if (currentStepSuccess) {
      const unsigned long rebootWaitMillis = 30000;
      Serial.print(F("Waiting ")); Serial.print(rebootWaitMillis / 1000); Serial.println(F("s for reboot..."));
      unsigned long startWait = millis();
      while(millis() - startWait < rebootWaitMillis) { readSerialResponse(100); delay(100); }
      Serial.println(F("Attempting communication post-reboot..."));
      if (!waitForInitialOK(15)) { Serial.println(F("ERROR: Module unresponsive post-reboot.")); currentStepSuccess = false; }
      else { Serial.println(F("Communication OK post-reboot.")); executeSimpleCommand("ATE0", "OK", 1000, 2); }
  }

  // STEP 2: Registration Check
    if (currentStepSuccess) {
      Serial.println(F("\n=== STEP 2: Network Registration ==="));
      bool registered = false;
      const int maxRegRetries = 20;
      for (int i = 0; i < maxRegRetries && currentStepSuccess; i++) {
          Serial.print(F("Reg check ")); Serial.print(i + 1); Serial.println(F("..."));
          if (!executeSimpleCommand("AT+CPIN?", "READY", 3000, 1)) { Serial.println(F("ERROR: SIM not READY.")); currentStepSuccess = false; break; }
          executeSimpleCommand("AT+CSQ", "+CSQ", 2000, 1);
          executeSimpleCommand("AT+COPS?", "+COPS", 3000, 1);
          bool creg_ok = false, cereg_ok = false;
          executeSimpleCommand("AT+CREG?", "+CREG:", 2000, 1); if (strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5")) creg_ok = true;
          executeSimpleCommand("AT+CEREG?", "+CEREG:", 2000, 1); if (strstr(responseBuffer, ",1") || strstr(responseBuffer, ",5")) cereg_ok = true;
          if (creg_ok || cereg_ok) { Serial.println(F("Registered.")); registered = true; break; }
          else { Serial.println(F("Not registered yet...")); delay(5000); }
      }
      if (!registered) { Serial.println(F("ERROR: Failed network registration.")); currentStepSuccess = false; }
  }

  // STEP 3: PDP Context
  if (currentStepSuccess) {
      Serial.println(F("\n=== STEP 3: PDP Context ==="));
      snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
      currentStepSuccess &= executeSimpleCommand(cmdBuffer, "OK", 5000, 3);
      if (currentStepSuccess) { currentStepSuccess &= executeSimpleCommand("AT+CGATT=1", "OK", 15000, 3); }
      if (currentStepSuccess) {
          Serial.println(F("Activating PDP (AT+CGACT=1,1)..."));
          bool activationAttempted = false;
          if (executeSimpleCommand("AT+CGACT=1,1", "OK", 20000, 1)) { Serial.println(F("OK received. Waiting...")); activationAttempted = true; delay(5000); }
          else { if (strstr(responseBuffer,"ERROR")) { Serial.println(F("CGACT ERROR.")); if(strstr(responseBuffer, "148")) activationAttempted = true; else currentStepSuccess = false; } else { Serial.println(F("CGACT Timeout?")); currentStepSuccess = false; } }
          if (activationAttempted && currentStepSuccess) {
              Serial.println(F("Verifying PDP (AT+CGACT?)..."));
              bool contextActive = false;
              for (int i=0; i<3 && !contextActive; i++) {
                  if (executeSimpleCommand("AT+CGACT?", "+CGACT:", 5000, 1)) { if (strstr(responseBuffer, "+CGACT: 1,1")) { Serial.println(F("PDP Active.")); contextActive = true; executeSimpleCommand("AT+CGPADDR=1", "+CGPADDR:", 2000, 1); } else { Serial.println(F("PDP not active.")); } }
                  else { Serial.println(F("No response CGACT?.")); }
                  if (!contextActive && i < 2) delay(5000);
              }
              if (!contextActive) { Serial.println(F("ERROR: Failed PDP verification.")); currentStepSuccess = false; }
          } else if (!activationAttempted) { Serial.println(F("Skipping PDP verify.")); }
      }
  }

   // STEP 4: Enable GNSS
   if (currentStepSuccess) {
        Serial.println(F("\n=== STEP 4: Enable GNSS ==="));
        if (executeSimpleCommand("AT+CGNSPWR=1", "OK", 2000, 3)) { Serial.println(F("GNSS Power Enabled.")); delay(1000); }
        else { Serial.println(F("ERROR: Failed to enable GNSS!")); currentStepSuccess = false; }
   }

  // Final Setup Result
  if (currentStepSuccess) { Serial.println(F("\n=== SETUP COMPLETE ===")); setupSuccess = true; }
  else { Serial.println(F("\n--- SETUP FAILED ---")); setupSuccess = false; }

  Serial.println(F("Entering main loop..."));
  lastSendTime = millis();

} // End setup()


// ========================================================================
// LOOP
// ========================================================================
void loop() {
  if (setupSuccess && (millis() - lastSendTime >= sendInterval)) {
    Serial.print(F("\n--- Interval Timer (")); Serial.print(millis() / 1000); Serial.println(F("s) ---"));

    if (getGpsData(currentLat, currentLon, gpsTimestampTraccar)) {
      Serial.print(F("GPS OK: Lat=")); Serial.print(currentLat, 6); Serial.print(F(" Lon=")); Serial.print(currentLon, 6); Serial.print(F(" Time=")); Serial.println(gpsTimestampTraccar);
      if (sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, currentLat, currentLon, gpsTimestampTraccar)) { Serial.println(F(">>> Send OK.")); }
      else { Serial.println(F(">>> Send FAIL.")); }
    } else { Serial.println(F(">>> No GPS fix, skipping send.")); }
    lastSendTime = millis(); // Reset timer
  }

  // Read URCs
  readSerialResponse(50);
  if(responseBufferPos > 0) { if (strstr(responseBuffer, "+CSQ:") == NULL && strstr(responseBuffer, "+CREG:") == NULL && strstr(responseBuffer, "+CEREG:") == NULL && strstr(responseBuffer, "+CGNSINF:") == NULL ) { Serial.print(F("Idle URC?: ")); Serial.println(responseBuffer); } }

  delay(100);
} // End loop()


// ========================================================================
// Get and Format GPS Data (Optimized Parsing - Corrected Declaration)
// ========================================================================
bool getGpsData(float &lat, float &lon, char* timestampOutput) {
    Serial.println(F("Requesting GNSS info (AT+CGNSINF)..."));
    moduleSerial.println("AT+CGNSINF");
    readSerialResponse(3000);

    char* startOfResponse = strstr(responseBuffer, "+CGNSINF:");
    if (startOfResponse == NULL) {
        Serial.println(F("ERROR: No '+CGNSINF:' in response."));
        return false;
    }

    // Pointer skipping the "+CGNSINF: " part (10 chars)
    char* dataStart = startOfResponse + 10;

    int gnssRunStatus = 0;
    int fixStatus = 0;

    // *** CORRECTION: Define size and declare local buffer ***
    const int TEMP_TIMESTAMP_BUF_SIZE = 20; // yyyyMMddHHmmss.sss + null
    char tempTimestamp[TEMP_TIMESTAMP_BUF_SIZE] = {0};
    // *** END CORRECTION ***

    // Use sscanf on the data part. Read timestamp as a string first into the local buffer.
    // Use %19[^,] to prevent buffer overflow (19 chars + null) for tempTimestamp
    int parsedFields = sscanf(dataStart, "%d,%d,%19[^,],%f,%f",
                                &gnssRunStatus, &fixStatus, tempTimestamp, &lat, &lon);

    if (parsedFields < 5) {
         Serial.println(F("ERROR: Failed parsing +CGNSINF."));
         Serial.print(F(" Parsed: ")); Serial.println(parsedFields);
         Serial.print(F(" Data starts at: ")); Serial.println(dataStart);
         return false;
    }

    Serial.print(F("GNSS Status: run=")); Serial.print(gnssRunStatus); Serial.print(F(", fix=")); Serial.println(fixStatus);

    if (gnssRunStatus != 1 || fixStatus != 1) {
        Serial.println(F("No valid fix."));
        return false;
    }

    // --- Format Timestamp ---
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int timeParsed = sscanf(tempTimestamp, "%4d%2d%2d%2d%2d%2d",
                             &year, &month, &day, &hour, &minute, &second);

    if (timeParsed < 6) {
        Serial.println(F("ERROR: Failed parsing time components."));
        Serial.print(F(" Timestamp str: ")); Serial.println(tempTimestamp);
         return false;
    }

    snprintf(timestampOutput, GPS_TIMESTAMP_TRACCAR_BUF_SIZE,
             "%04d-%02d-%02d%%20%02d:%02d:%02d",
             year, month, day, hour, minute, second);

    return true; // Success!
}


// ========================================================================
// Send GPS Data to Traccar Server (Includes timestamp)
// ========================================================================
bool sendGpsToTraccar(const char* host, int port, const char* deviceId, float lat, float lon, const char* timestampStr) {
    bool connectSuccess = false;
    bool sendSuccess = false;
    bool overallSuccess = false;

    // 1. Connect
    Serial.println(F("Connecting to Traccar..."));
    snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    if (executeSimpleCommand(cmdBuffer, "OK", 20000, 1)) {
        if (strstr(responseBuffer, "CONNECT OK")) { connectSuccess = true; Serial.println(F("Connect OK (immediate).")); }
        else if (strstr(responseBuffer, "ALREADY CONNECT")) { connectSuccess = true; Serial.println(F("Already connected.")); }
        else {
            readSerialResponse(5000); // Wait for async
            if (strstr(responseBuffer, "CONNECT OK")) { connectSuccess = true; Serial.println(F("Connect OK (async).")); }
            else if (strstr(responseBuffer, "ALREADY CONNECT")) { connectSuccess = true; Serial.println(F("Already connected (async).")); }
            else { Serial.println(F("Connect failed (post-OK check).")); connectSuccess = false; }
        }
    } else { Serial.println(F("CIPSTART command failed.")); connectSuccess = false; }

    // 2. Send
    if (connectSuccess) {
        char latStr[15]; char lonStr[15];
        dtostrf(lat, 4, 6, latStr); dtostrf(lon, 4, 6, lonStr);
        snprintf(httpRequestBuffer, HTTP_REQUEST_BUFFER_SIZE,
                 "GET /?id=%s&lat=%s&lon=%s&timestamp=%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: SIM7000-Arduino\r\nConnection: close\r\n\r\n",
                 deviceId, latStr, lonStr, timestampStr, host);
        int dataLength = strlen(httpRequestBuffer);
        Serial.print(F("Sending CIPSEND (len ")); Serial.print(dataLength); Serial.println(F(")..."));
        // Check combined length against buffer size and typical module limits (~1460)
        if (dataLength >= HTTP_REQUEST_BUFFER_SIZE || dataLength >= 1460) {
             Serial.println(F("ERROR: Request too long!"));
             // We might still be connected, so don't set connectSuccess=false, just prevent sending
             sendSuccess = false;
        } else {
            snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CIPSEND=%d", dataLength);
            moduleSerial.println(cmdBuffer);
            readSerialResponse(2000); // Wait for ">"
            if (strstr(responseBuffer, ">")) {
                Serial.println(F("Got '>'. Sending HTTP..."));
                moduleSerial.print(httpRequestBuffer);
                readSerialResponse(10000); // Wait for SEND OK
                if (strstr(responseBuffer, "SEND OK")) { sendSuccess = true; Serial.println(F("SEND OK.")); readSerialResponse(2000); /* Clear potential server response */ }
                else { sendSuccess = false; Serial.println(F("SEND FAIL/TIMEOUT?")); }
            } else { sendSuccess = false; Serial.println(F("No '>' prompt.")); }
        }
    } else { Serial.println(F("Skipping send (no connection).")); sendSuccess = false; }

    // 3. Close / Cleanup
    // Always close if connectSuccess was true, even if sendSuccess was false (e.g., request too long)
    if (connectSuccess) {
        Serial.println(F("Closing connection..."));
        if (!executeSimpleCommand("AT+CIPCLOSE=0", "CLOSE OK", 5000, 1)) { Serial.println(F("CIPCLOSE failed?")); }
    } else {
        // If we never connected, ensure state is clean
         executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 1);
    }

    delay(1000);
    overallSuccess = connectSuccess && sendSuccess; // Overall success requires both connect and send
    return overallSuccess;
}

// ========================================================================
// HELPER FUNCTIONS (waitForInitialOK, executeSimpleCommand, readSerialResponse, clearSerialBuffer)
// ========================================================================
void clearSerialBuffer() { while(moduleSerial.available()) moduleSerial.read(); }

void readSerialResponse(unsigned long waitMillis) {
    unsigned long start = millis();
    memset(responseBuffer, 0, RESPONSE_BUFFER_SIZE);
    responseBufferPos = 0;
    bool anythingReceived = false;
    while (millis() - start < waitMillis) {
        while (moduleSerial.available()) {
            anythingReceived = true;
            if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
                char c = moduleSerial.read();
                if (c != '\0') { responseBuffer[responseBufferPos++] = c; responseBuffer[responseBufferPos] = '\0'; }
            } else { moduleSerial.read(); } // Discard if full
        }
        if (!moduleSerial.available()) delay(5);
    }
    if (responseBufferPos > 0) {
        Serial.print(F("Rcvd: ["));
        for(uint16_t i=0; i<responseBufferPos; i++) {
            char c = responseBuffer[i];
            if (c == '\r') continue; // Skip CR
            else if (c == '\n') Serial.print(F("<LF>"));
            else if (isprint(c)) Serial.print(c);
            else Serial.print('.');
        }
        Serial.println(F("]"));
    } else if (anythingReceived) { Serial.println(F("Rcvd: [Empty/Discarded]")); }
}

bool waitForInitialOK(int maxRetries) {
    for (int i = 0; i < maxRetries; i++) {
        Serial.print(F("AT (Try ")); Serial.print(i + 1); Serial.print(F(")... "));
        moduleSerial.println("AT"); readSerialResponse(1000);
        if (strstr(responseBuffer, "OK")) { Serial.println(F("OK.")); return true; }
        Serial.println(F("No OK.")); delay(500);
    } return false;
}

bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries) {
     for (int i=0; i < retries; i++) {
        Serial.print(F("Send [")); Serial.print(i+1); Serial.print(F("]: ")); Serial.println(command);
        moduleSerial.println(command); readSerialResponse(timeoutMillis);
        if (strstr(responseBuffer, expectedResponse)) { Serial.println(F(">> OK Resp.")); return true; }
        if (strstr(responseBuffer, "ERROR")) Serial.println(F(">> ERROR Resp."));
        Serial.println(F(">> No/Wrong Resp."));
        if (i < retries - 1) delay(500);
     } Serial.println(F(">> Failed after retries.")); return false;
}
