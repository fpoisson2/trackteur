#include <SoftwareSerial.h>
#include <stdio.h>  // Include for snprintf
#include <string.h> // Include for strstr, memset
#include <stdlib.h> // Include for dtostrf (float to string)

// --- Pin Definitions ---
const int powerPin = 2; // Pin to turn the module ON (set HIGH)
const int swRxPin = 3;  // Software Serial RX pin (connect to module's TX)
const int swTxPin = 4;  // Software Serial TX pin (connect to module's RX)

// --- Software Serial Object ---
SoftwareSerial moduleSerial(swRxPin, swTxPin);

// --- Baud Rate ---
const long moduleBaudRate = 9600;

// --- Network Configuration ---
const char* APN = "em"; // Your APN

// --- Traccar Configuration ---
const char* TRACCAR_HOST = "trackteur.ve2fpd.com";
const int TRACCAR_PORT = 5055;
const char* DEVICE_ID = "212910"; // <<< IMPORTANT: Use YOUR Traccar Device ID

// --- Dummy GPS Data ---
const float dummyLat = 46.8139;
const float dummyLon = -71.2082;

// --- Timing for Sending Data ---
unsigned long lastSendTime = 0;
// Send every 60 seconds (60 * 1000 milliseconds)
const unsigned long sendInterval = 60000;

// --- Response Buffer ---
#define RESPONSE_BUFFER_SIZE 250 // Slightly increased for potentially larger responses
char responseBuffer[RESPONSE_BUFFER_SIZE];
uint16_t responseBufferPos = 0;

// --- Command Buffer ---
#define CMD_BUFFER_SIZE 150 // Increased for AT+CIPSTART command
char cmdBuffer[CMD_BUFFER_SIZE];

// --- HTTP Request Buffer ---
#define HTTP_REQUEST_BUFFER_SIZE 300
char httpRequestBuffer[HTTP_REQUEST_BUFFER_SIZE];

// --- Global State ---
bool setupSuccess = false; // Flag to indicate if core setup completed

// --- Function Declarations ---
bool waitForInitialOK(int maxRetries);
bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries);
void clearSerialBuffer();
void readSerialResponse(unsigned long waitMillis);
bool sendGpsToTraccar(const char* host, int port, const char* deviceId, float lat, float lon);


// ========================================================================
// SETUP - Performs the core module initialization and network setup
// (Modified version using AT+CGACT and stricter checks)
// ========================================================================
void setup() {
  // Assume failure until proven otherwise
  setupSuccess = false;
  bool currentStepSuccess = true; // Track success within setup

  // --- Basic Arduino & Serial Setup ---
  Serial.begin(9600);
  while (!Serial) { ; }
  Serial.println(F("--- Arduino Initialized ---"));

  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW); // Start OFF
  Serial.println(F("Module power pin configured (D2)."));

  moduleSerial.begin(moduleBaudRate);
  Serial.print(F("Software Serial initialized on Pins RX:"));
  Serial.print(swRxPin); Serial.print(F(", TX:")); Serial.print(swTxPin);
  Serial.print(F(" at ")); Serial.print(moduleBaudRate); Serial.println(F(" baud."));

  // --- Power On Module ---
  Serial.println(F("Turning module ON (Setting D2 HIGH)..."));
  digitalWrite(powerPin, HIGH);
  Serial.println(F("Waiting 5 seconds for module boot...")); // Increased wait
  delay(5000);
  Serial.println(F("Module boot wait complete."));

  // --- Wait for Initial AT -> OK ---
  Serial.println(F("Attempting initial communication..."));
  if (!waitForInitialOK(15)) {
    Serial.println(F("FATAL: Module did not respond with OK initially. Halting setup."));
    currentStepSuccess = false;
  } else {
      Serial.println(F("Initial communication successful."));
       // Optional but recommended: Disable echo and enable verbose errors early
      executeSimpleCommand("ATE0", "OK", 1000, 2); // Disable echo
      executeSimpleCommand("AT+CMEE=2", "OK", 1000, 2); // Enable verbose errors
  }

  // --- STEP 1: Configure Network Settings ---
  if (currentStepSuccess) {
    Serial.println(F("\n=== STEP 1: Configure Network Settings ==="));
    executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 2); // Try to clear old connections

    currentStepSuccess &= executeSimpleCommand("AT+CFUN=0", "OK", 5000, 3);
    currentStepSuccess &= executeSimpleCommand("AT+CNMP=38", "OK", 2000, 3); // Prefer LTE
    currentStepSuccess &= executeSimpleCommand("AT+CMNB=1", "OK", 2000, 3);  // Prefer NB-IoT
  }
  if (currentStepSuccess) {
      Serial.println(F("Turning radio ON (CFUN=1) and rebooting..."));
      moduleSerial.println("AT+CFUN=1,1"); // Command causes reboot, no OK expected here
  } else {
      Serial.println(F("Skipping CFUN=1 due to previous errors."));
  }

  // --- Wait and Re-check Communication Post-Reset ---
  if (currentStepSuccess) {
      const unsigned long rebootWaitMillis = 30000;
      Serial.print(F("Waiting ")); Serial.print(rebootWaitMillis / 1000); Serial.println(F("s for module reboot/network search..."));
      unsigned long startWait = millis();
      while(millis() - startWait < rebootWaitMillis) {
           readSerialResponse(100); // Read URCs
           delay(100);
      }
      Serial.println(F("Reboot wait finished. Attempting communication..."));
      if (!waitForInitialOK(15)) {
          Serial.println(F("ERROR: Module unresponsive after CFUN=1,1 reset."));
          currentStepSuccess = false;
      } else {
          Serial.println(F("Communication re-established after reset."));
          currentStepSuccess = true;
          executeSimpleCommand("ATE0", "OK", 1000, 2); // Ensure echo is still off
      }
  }

  // --- STEP 2: Check Network Registration ---
  if (currentStepSuccess) {
      Serial.println(F("\n=== STEP 2: Check Network Registration ==="));
      bool registered = false;
      const int maxRegRetries = 20;
      for (int i = 0; i < maxRegRetries && currentStepSuccess; i++) {
          Serial.print(F("Registration check attempt ")); Serial.print(i + 1); Serial.print(F("/")); Serial.println(maxRegRetries);
          if (!executeSimpleCommand("AT+CPIN?", "READY", 3000, 1)) {
            Serial.println(F("ERROR: SIM not READY. Check SIM card."));
            currentStepSuccess = false;
            break;
          }
          executeSimpleCommand("AT+CSQ", "+CSQ", 2000, 1);
          executeSimpleCommand("AT+COPS?", "+COPS", 3000, 1); // Check operator/mode

          bool creg_ok = false;
          executeSimpleCommand("AT+CREG?", "+CREG:", 2000, 1);
          if (strstr(responseBuffer, "+CREG: ") != NULL && (strstr(responseBuffer, ",1") != NULL || strstr(responseBuffer, ",5") != NULL)) {
              Serial.println(F("DEBUG: CREG Registered")); creg_ok = true;
          } else { Serial.println(F("DEBUG: CREG Not Registered")); }

          bool cereg_ok = false;
          executeSimpleCommand("AT+CEREG?", "+CEREG:", 2000, 1);
          if (strstr(responseBuffer, "+CEREG: ") != NULL && (strstr(responseBuffer, ",1") != NULL || strstr(responseBuffer, ",5") != NULL)) {
              Serial.println(F("DEBUG: CEREG Registered")); cereg_ok = true;
          } else { Serial.println(F("DEBUG: CEREG Not Registered")); }

          if (cereg_ok || creg_ok) {
              Serial.println(F("Registered on network."));
              registered = true; break;
          } else {
              Serial.println(F("Not registered yet...")); delay(5000);
          }
      }
      if (!registered) {
          Serial.println(F("ERROR: Failed to register on the network."));
          currentStepSuccess = false;
      }
  }

  // --- STEP 3: Setup APN and Activate PDP Context ---
  if (currentStepSuccess) {
      Serial.println(F("\n=== STEP 3: Setup APN and Activate PDP Context ==="));

      // Define PDP Context (CID=1) with the specified APN
      snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
      currentStepSuccess &= executeSimpleCommand(cmdBuffer, "OK", 5000, 3);
  }
  if (currentStepSuccess) {
      // Attach to Packet Service
      currentStepSuccess &= executeSimpleCommand("AT+CGATT=1", "OK", 15000, 3);
  }
  if (currentStepSuccess) {
      // *** MODIFICATION: Use AT+CGACT instead of AT+CNACT ***
      Serial.println(F("Attempting PDP context activation (AT+CGACT=1,1)..."));
      bool activationAttempted = false;
      if (executeSimpleCommand("AT+CGACT=1,1", "OK", 20000, 1)) { // Wait for OK
          Serial.println(F("AT+CGACT=1,1 returned OK. Waiting a bit for activation to settle..."));
          activationAttempted = true;
          delay(5000); // *** ADDED DELAY: Wait 5 seconds after OK ***
      } else {
           // Check buffer for specific CME ERROR codes if needed
           if (strstr(responseBuffer,"ERROR") != NULL) {
               Serial.println(F("AT+CGACT=1,1 command failed with ERROR."));
               // Example check: +CME ERROR: 148 (already active?)
               if(strstr(responseBuffer, "148") != NULL) {
                    Serial.println(F("(Error suggests already active - will check status)"));
                    activationAttempted = true; // Proceed to check status
               } else {
                    currentStepSuccess = false; // Other errors are likely fatal
               }
           } else {
                Serial.println(F("AT+CGACT=1,1 command failed (Timeout or unknown)."));
                currentStepSuccess = false; // Failed to even attempt activation
           }
      }

      // *** MODIFICATION: Verify using AT+CGACT? and trust its state ***
      if (activationAttempted && currentStepSuccess) {
          Serial.println(F("Verifying PDP context status (AT+CGACT?)..."));
          bool contextActive = false;
          for (int i=0; i<3; i++) { // Retry check a few times
              if (executeSimpleCommand("AT+CGACT?", "+CGACT:", 5000, 1)) {
                    // Look for "+CGACT: 1,1" indicating CID 1 is active (state 1)
                    if (strstr(responseBuffer, "+CGACT: 1,1") != NULL) {
                        Serial.println(F("PDP Context active confirmed via AT+CGACT?."));
                        contextActive = true;
                        // Optionally, check CGPADDR here just for logging the IP
                        executeSimpleCommand("AT+CGPADDR=1", "+CGPADDR:", 2000, 1);
                        break; // Exit check loop
                    } else {
                         Serial.println(F("AT+CGACT? response received, but context 1 is not active (state != 1)."));
                         // Print the actual response for debugging state
                         Serial.print(F("CGACT Response was: ")); Serial.println(responseBuffer);
                    }
              } else {
                  Serial.println(F("Failed to get response for AT+CGACT?."));
              }
              if (!contextActive && i < 2) {
                  Serial.println(F("Retrying CGACT? check in 5 seconds..."));
                  delay(5000);
              }
          } // end for loop (CGACT? retries)

          if (!contextActive) {
              Serial.println(F("ERROR: Failed to verify ACTIVE PDP context via AT+CGACT? after multiple checks."));
              currentStepSuccess = false; // <<<<<<<<<<<<< THIS IS THE CRITICAL CHANGE
          }
      } else if (!activationAttempted) {
           Serial.println(F("Skipping context verification as activation command failed."));
           // currentStepSuccess should already be false here
      }
  }

  // --- End of Core Setup Sequence ---
  if (currentStepSuccess) {
      Serial.println(F("\n=== CORE CONFIGURATION AND PDP CONTEXT ACTIVATION COMPLETE ==="));
      setupSuccess = true; // Set the global flag
  } else {
       Serial.println(F("\n--- ERRORS ENCOUNTERED DURING CORE SETUP / PDP ACTIVATION ---"));
       Serial.println(F("Traccar reporting will be disabled."));
       setupSuccess = false;
  }

  Serial.println(F("\nSetup sequence complete. Entering main loop."));
  lastSendTime = millis(); // Initialize timer for first send
} // End setup()


// ========================================================================
// LOOP - Sends GPS data periodically if setup was successful
// ========================================================================
void loop() {
  // Check if it's time to send data and if setup was successful
  if (setupSuccess && (millis() - lastSendTime >= sendInterval)) {
    Serial.print(F("\n--- Time to send GPS data ("));
    Serial.print(millis() / 1000); Serial.println(F("s) ---"));

    if (sendGpsToTraccar(TRACCAR_HOST, TRACCAR_PORT, DEVICE_ID, dummyLat, dummyLon)) {
      Serial.println(F(">>> Successfully sent GPS data to Traccar."));
    } else {
      Serial.println(F(">>> Failed to send GPS data to Traccar."));
      // Optional: Implement retry logic or error counting here
    }

    lastSendTime = millis(); // Reset the timer regardless of success/failure for next interval
  }

  // Read and print any unexpected URCs from the module during idle periods
  // Keep this short to avoid blocking the sending interval check too much
  readSerialResponse(50);
  if(responseBufferPos > 0) {
     // Filter out noisy URCs if needed, e.g. ignore "+CSQ: ..." if it floods the log
     if (strstr(responseBuffer, "+CSQ:") == NULL && strstr(responseBuffer, "+CREG:") == NULL && strstr(responseBuffer, "+CEREG:") == NULL) {
        Serial.print(F("Idle URC?: "));
        Serial.println(responseBuffer);
     }
  }

  // Small delay to prevent high CPU usage and allow time for serial reading
  delay(100);

} // End loop()


// ========================================================================
// Send GPS Data to Traccar Server (Corrected CIPSTART Handling)
// ========================================================================
bool sendGpsToTraccar(const char* host, int port, const char* deviceId, float lat, float lon) {
    bool connectSuccess = false;
    bool sendSuccess = false;
    bool overallSuccess = false;

    // --- 1. Establish TCP Connection using AT+CIPSTART ---
    Serial.println(F("Attempting to connect to Traccar server..."));
    snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);

    // Send CIPSTART command. Expect "OK" initially.
    // The global responseBuffer will contain the result after this call.
    if (executeSimpleCommand(cmdBuffer, "OK", 20000, 1)) { // Long timeout for connection

        // *** MODIFICATION START ***
        // Check the buffer content captured by executeSimpleCommand *immediately*.
        // Do NOT call readSerialResponse again here yet, as it would clear the buffer.
        if (strstr(responseBuffer, "CONNECT OK") != NULL) {
            Serial.println(F("Connection established (CONNECT OK found in initial response)."));
            connectSuccess = true;
        } else if (strstr(responseBuffer, "ALREADY CONNECT") != NULL) {
            Serial.println(F("Connection already established (ALREADY CONNECT found in initial response)."));
            connectSuccess = true;
        } else {
            // If we got OK but not CONNECT OK/ALREADY CONNECT immediately,
            // *then* wait a bit longer for a potential delayed asynchronous URC.
            Serial.println(F("OK received, but not CONNECT OK/ALREADY CONNECT yet. Waiting for async response..."));
            readSerialResponse(5000); // Now wait specifically for delayed URC

            if (strstr(responseBuffer, "CONNECT OK") != NULL) {
                Serial.println(F("Connection established (CONNECT OK found as async response)."));
                connectSuccess = true;
            } else if (strstr(responseBuffer, "ALREADY CONNECT") != NULL) {
                Serial.println(F("Connection already established (ALREADY CONNECT found as async response)."));
                connectSuccess = true;
            } else if (strstr(responseBuffer, "CONNECT FAIL") != NULL) {
                 Serial.println(F("Connection failed (CONNECT FAIL URC)."));
                 connectSuccess = false;
            } else if (strstr(responseBuffer, "ERROR") != NULL) {
                // Should be rare if initial command returned OK, but possible (e.g., DNS error URC)
                 Serial.println(F("Connection failed (Async ERROR URC after OK)."));
                 connectSuccess = false;
            } else {
                Serial.println(F("Connection status uncertain (OK received, but no clear CONNECT OK/FAIL/ERROR followed). Assuming failure."));
                Serial.print(F("Buffer content after async wait: ")); Serial.println(responseBuffer); // Log buffer content
                connectSuccess = false; // Be conservative
            }
        }
         // *** MODIFICATION END ***

    } else {
        // executeSimpleCommand failed (likely timed out or got ERROR directly)
        Serial.println(F("AT+CIPSTART command failed or timed out."));
        // Check buffer for specific errors if needed
        if (strstr(responseBuffer, "CONNECT FAIL") != NULL) {
             Serial.println(F("Connection failed (CONNECT FAIL URC during command wait)."));
        } else if (strstr(responseBuffer, "ERROR") != NULL) {
             Serial.println(F("Connection failed (ERROR response to CIPSTART)."));
        }
        connectSuccess = false;
    }

    // --- 2. Send Data using AT+CIPSEND if Connected ---
    if (connectSuccess) {
        // Format the HTTP GET request (OsmAnd protocol)
        char latStr[15];
        char lonStr[15];
        dtostrf(lat, 4, 6, latStr); // width=4 (min), precision=6, output=latStr
        dtostrf(lon, 4, 6, lonStr); // width=4 (min), precision=6, output=lonStr

        snprintf(httpRequestBuffer, HTTP_REQUEST_BUFFER_SIZE,
                 "GET /?id=%s&lat=%s&lon=%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: SIM7000-Arduino\r\nConnection: close\r\n\r\n",
                 deviceId, latStr, lonStr, host);

        Serial.print(F("Preparing to send data (AT+CIPSEND), Length: "));
        int dataLength = strlen(httpRequestBuffer);
        Serial.println(dataLength);

        snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CIPSEND=%d", dataLength); // Use CIPSEND with length

        // Send AT+CIPSEND command. We expect the ">" prompt.
        moduleSerial.println(cmdBuffer);
        readSerialResponse(2000); // Wait for the ">"

        if (strstr(responseBuffer, ">") != NULL) {
            Serial.println(F("Received '>' prompt. Sending HTTP request..."));
            // Send the actual HTTP data
            moduleSerial.print(httpRequestBuffer); // Use print, not println

            // Wait for SEND OK (or potentially ERROR)
            readSerialResponse(10000); // Wait longer for network transmission + server response proxying

            if (strstr(responseBuffer, "SEND OK") != NULL) {
                Serial.println(F("Data send confirmed (SEND OK)."));
                sendSuccess = true;

                Serial.println(F("Waiting briefly for server HTTP response data..."));
                readSerialResponse(5000); // Read any immediate follow-up data (+CIPRXGET etc.)
                if (responseBufferPos > 0) {
                   Serial.print(F("Server/Module Response after SEND OK: "));
                   Serial.println(responseBuffer);
                }

            } else if (strstr(responseBuffer, "SEND FAIL") != NULL) {
                 Serial.println(F("Data send failed (SEND FAIL)."));
                 sendSuccess = false;
            } else if (strstr(responseBuffer, "ERROR") != NULL) {
                Serial.println(F("Data send failed (ERROR)."));
                sendSuccess = false;
            } else {
                Serial.println(F("Data send status uncertain (No SEND OK/FAIL/ERROR)."));
                if(strstr(responseBuffer, "+CIPRXGET: 1")) {
                     Serial.println(F("(Server likely closed connection as requested)"));
                     sendSuccess = true; // Assume success if server closed after getting data
                } else {
                    Serial.print(F("Buffer content after CIPSEND: ")); Serial.println(responseBuffer); // Log buffer content
                    sendSuccess = false; // Otherwise assume failure
                }
            }
        } else {
            Serial.println(F("Did not receive '>' prompt after AT+CIPSEND."));
            Serial.print(F("Buffer content: ")); Serial.println(responseBuffer);
            sendSuccess = false;
        }
    } else {
         Serial.println(F("Skipping CIPSEND because connection failed."));
         sendSuccess = false;
    }

    // --- 3. Close Connection (or Shut Down) ---
    if (connectSuccess) {
         Serial.println(F("Closing TCP connection (AT+CIPCLOSE)..."));
         // Use CIPCLOSE=0 assuming single connection (CIPMUX=0)
         if (executeSimpleCommand("AT+CIPCLOSE=0", "CLOSE OK", 5000, 1)) {
             Serial.println(F("Connection closed successfully."));
         } else {
             Serial.println(F("CIPCLOSE failed or timed out. Might already be closed."));
             // Serial.println(F("Attempting CIPSHUT as fallback..."));
             // executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 1);
         }
    } else {
        Serial.println(F("Connection did not seem established, attempting CIPSHUT for clean state..."));
        executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 1);
    }

    delay(1000); // Small delay after closing/shutting

    overallSuccess = connectSuccess && sendSuccess;
    return overallSuccess;

} // End sendGpsToTraccar()


// ========================================================================
// HELPER FUNCTIONS (Largely unchanged)
// ========================================================================

/**
 * @brief Clears the SoftwareSerial receive buffer by reading all available chars.
 */
void clearSerialBuffer() {
    while(moduleSerial.available()) {
        moduleSerial.read();
    }
}

/**
 * @brief Reads from moduleSerial into global responseBuffer for a specified time.
 * Prints the collected response to the main Serial port for debugging.
 */
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
                if (c != '\0' ) {
                     responseBuffer[responseBufferPos++] = c;
                     responseBuffer[responseBufferPos] = '\0'; // Null-terminate
                }
            } else {
                moduleSerial.read(); // Buffer full, discard
            }
        }
        if (!moduleSerial.available()) {
           delay(5); // Small yield
        }
    }

    if (responseBufferPos > 0) {
        Serial.print(F("Received during wait: ["));
        for(uint16_t i=0; i<responseBufferPos; i++) {
            char c = responseBuffer[i];
            if (c == '\r') { /* Skip printing raw CR */ }
            else if (c == '\n') { Serial.print(F("<LF>")); }
            else if (isprint(c)) { Serial.print(c); }
            else { Serial.print('.'); }
        }
        Serial.println(F("]"));
    } else if (anythingReceived) {
         Serial.println(F("Received during wait: [Data received but buffer empty/discarded]"));
    }
}

/**
 * @brief Loops sending "AT" until "OK" is found in the response buffer.
 */
bool waitForInitialOK(int maxRetries) {
    for (int i = 0; i < maxRetries; i++) {
        Serial.print(F("Sending AT (Attempt ")); Serial.print(i + 1); Serial.print(F(")... "));
        moduleSerial.println("AT");
        readSerialResponse(1000);
        if (strstr(responseBuffer, "OK") != NULL) {
            Serial.println(F("OK received!"));
            return true;
        }
        Serial.println(F("No OK yet."));
        delay(500);
    }
    return false;
}


/**
 * @brief Sends a command, waits for a response, checks for expected substring.
 * NOTE: Best for commands with immediate final responses (OK/ERROR).
 * For commands with async responses (like CONNECT OK), check buffer afterwards.
 */
bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries) {
     for (int i=0; i < retries; i++) {
        Serial.print(F("Sending [Try ")); Serial.print(i+1); Serial.print(F("/") ); Serial.print(retries); Serial.print(F("]: ")); Serial.println(command);
        moduleSerial.println(command);
        readSerialResponse(timeoutMillis);

        if (strstr(responseBuffer, expectedResponse) != NULL) {
            Serial.println(F(">>> Expected response found."));
            return true;
        }
        if (strstr(responseBuffer, "ERROR") != NULL) {
            Serial.println(F(">>> ERROR response detected in buffer."));
        }

        Serial.println(F(">>> Expected response NOT found."));
        if (i < retries - 1) {
            Serial.println(F("    Retrying..."));
            delay(500);
        }
     }
     Serial.println(F(">>> Command failed after all retries."));
     return false;
}
