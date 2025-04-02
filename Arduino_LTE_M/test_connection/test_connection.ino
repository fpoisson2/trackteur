#include <SoftwareSerial.h>

// --- Pin Definitions ---
const int powerPin = 2; // Pin to turn the module ON (set HIGH)
const int swRxPin = 3;  // Software Serial RX pin (connect to module's TX)
const int swTxPin = 4;  // Software Serial TX pin (connect to module's RX)

// --- Software Serial Object ---
SoftwareSerial moduleSerial(swRxPin, swTxPin);

// --- Baud Rate ---
const long moduleBaudRate = 9600;

// --- Network Configuration ---
const char* APN = "em"; // Emnify APN (or your APN)

// --- Response Buffer ---
#define RESPONSE_BUFFER_SIZE 200
char responseBuffer[RESPONSE_BUFFER_SIZE];
uint16_t responseBufferPos = 0;

// --- Function Declarations ---
bool waitForInitialOK(int maxRetries);
bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries);
void clearSerialBuffer();
void readSerialResponse(unsigned long waitMillis);


// ========================================================================
// SETUP - Performs the entire sequence
// ========================================================================
void setup() {
  bool success = true;

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
  Serial.println(F("Waiting 2 seconds for module boot..."));
  delay(2000);
  Serial.println(F("Module boot wait complete."));

  // --- Wait for Initial AT -> OK ---
  Serial.println(F("Attempting initial communication..."));
  if (!waitForInitialOK(15)) { // Try up to 15 times
    Serial.println(F("FATAL: Module did not respond with OK initially. Halting."));
    success = false;
    while (true) { delay(1000); } // Halt execution
  }
  Serial.println(F("Initial communication successful."));

  // --- STEP 1: Configure Network Settings ---
  if (success) {
    Serial.println(F("\n=== STEP 1: Configure Network Settings ==="));
    executeSimpleCommand("AT+CIPSHUT", "OK", 5000, 3); // Allow "SHUT OK" too? Check response manually for now.
    success &= executeSimpleCommand("AT+CFUN=0", "OK", 5000, 3);
    success &= executeSimpleCommand("AT+CNMP=38", "OK", 2000, 3);
    success &= executeSimpleCommand("AT+CMNB=1", "OK", 2000, 3);
  }
  if (success) {
     Serial.println(F("Turning radio ON (CFUN=1) and rebooting..."));
     success &= executeSimpleCommand("AT+CFUN=1,1", "OK", 10000, 1); // Just send command, module resets
  }

  // --- Wait and Re-check Communication Post-Reset ---
  if (success) {
     const unsigned long rebootWaitMillis = 25000; // 25 seconds wait
     Serial.print(F("Waiting ")); Serial.print(rebootWaitMillis / 1000); Serial.println(F("s for module reboot/network search..."));
     delay(rebootWaitMillis);

     Serial.println(F("Attempting communication after reset..."));
     if (!waitForInitialOK(15)) { // Use the same reliable check
        Serial.println(F("ERROR: Module unresponsive after CFUN=1,1 reset."));
        success = false;
     } else {
        Serial.println(F("Communication re-established after reset."));
     }
  }

  // --- STEP 2: Check Network Registration ---
  if (success) {
      Serial.println(F("\n=== STEP 2: Check Network Registration ==="));
      bool registered = false;
      const int maxRegRetries = 15; // More retries might be needed
      for (int i = 0; i < maxRegRetries; i++) {
          Serial.print(F("Registration check attempt ")); Serial.print(i + 1); Serial.print(F("/")); Serial.println(maxRegRetries);
          executeSimpleCommand("AT+CPIN?", "READY", 2000, 1); // Check SIM is READY
          executeSimpleCommand("AT+CSQ", "+CSQ", 2000, 1);     // Check signal quality (just expect the URC)

    // Check CREG/CEREG - Improved Check
          bool creg_ok = false;
          executeSimpleCommand("AT+CREG?", "+CREG:", 2000, 1); // Just send command and read response
          if (strstr(responseBuffer, "+CREG:") != NULL && (strstr(responseBuffer, ",1") != NULL || strstr(responseBuffer, ",5") != NULL)) {
               Serial.println(F("DEBUG: CREG Registered (,1 or ,5 found)"));
               creg_ok = true;
          } else {
               Serial.println(F("DEBUG: CREG Not Registered (,1 or ,5 not found)"));
          }
    
    
          bool cereg_ok = false;
          executeSimpleCommand("AT+CEREG?", "+CEREG:", 2000, 1); // Just send command and read response
           if (strstr(responseBuffer, "+CEREG:") != NULL && (strstr(responseBuffer, ",1") != NULL || strstr(responseBuffer, ",5") != NULL)) {
               Serial.println(F("DEBUG: CEREG Registered (,1 or ,5 found)"));
               cereg_ok = true;
           } else {
                Serial.println(F("DEBUG: CEREG Not Registered (,1 or ,5 not found)"));
           }
    
    
           if (creg_ok || cereg_ok) { // Accept if registered on either CREG or CEREG
                Serial.println(F("Registered on network."));
                registered = true;
                break; // Exit loop
           } else {
                Serial.println(F("Not registered yet..."));
                delay(5000); // Wait longer between registration checks
           }
          if (creg_ok || cereg_ok) { // Accept if registered on either CREG or CEREG (adjust if specific tech needed)
             Serial.println(F("Registered on network."));
             registered = true;
             break; // Exit loop
          } else {
             Serial.println(F("Not registered yet..."));
             delay(5000); // Wait longer between registration checks
          }
      }
      if (!registered) {
          Serial.println(F("ERROR: Failed to register on the network."));
          success = false;
      }
  }

  // --- STEP 3: Setup APN and Activate PDP Context ---
  if (success) {
      Serial.println(F("\n=== STEP 3: Setup APN and Activate PDP Context ==="));
      char cmdBuffer[100];
      snprintf(cmdBuffer, sizeof(cmdBuffer), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
      success &= executeSimpleCommand(cmdBuffer, "OK", 2000, 3);
  }
  if (success) {
     success &= executeSimpleCommand("AT+CGATT=1", "OK", 5000, 3);
  }
  if (success) {
     // Use CNACT - might need CGACT on other modules
     char cmdBuffer[100];
     snprintf(cmdBuffer, sizeof(cmdBuffer), "AT+CNACT=1,\"%s\"", APN);
     success &= executeSimpleCommand(cmdBuffer, "OK", 15000, 1); // Activation might take time
  }
  if (success) {
     // Verify activation
     success &= executeSimpleCommand("AT+CNACT?", "+CNACT: 1,", 5000, 3);
     if (!success) {
        Serial.println(F("Warning: Could not verify PDP context via AT+CNACT?, but proceeding."));
        success = true; // Override failure for now if verification fails
     }
  }

  // --- End of Sequence ---
   if (success) {
      Serial.println(F("\n=== CONFIGURATION AND CONNECTION SETUP COMPLETE (Simplified) ==="));
   } else {
       Serial.println(F("\n--- ERRORS ENCOUNTERED DURING SETUP ---"));
   }
   Serial.println(F("Setup complete. Entering idle loop."));

} // End setup()


// ========================================================================
// LOOP - Does nothing after setup
// ========================================================================
void loop() {
  // Nothing here. The work is done in setup().
  delay(10000); // Prevent busy-looping excessively
}

// ========================================================================
// HELPER FUNCTIONS
// ========================================================================

/**
 * @brief Clears the SoftwareSerial receive buffer by reading all available chars.
 */
void clearSerialBuffer() {
    // Serial.print(F("Clearing serial RX buffer...")); // Debug
    while(moduleSerial.available()) {
        moduleSerial.read();
    }
    // Serial.println(F("Done.")); // Debug
}

/**
 * @brief Reads from moduleSerial into global responseBuffer for a specified time.
 */
void readSerialResponse(unsigned long waitMillis) {
    unsigned long start = millis();
    // Clear global buffer before reading new response
    memset(responseBuffer, 0, RESPONSE_BUFFER_SIZE);
    responseBufferPos = 0;

    while (millis() - start < waitMillis) {
        while (moduleSerial.available()) {
           if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
               char c = moduleSerial.read();
               responseBuffer[responseBufferPos++] = c;
               responseBuffer[responseBufferPos] = '\0';
           } else {
               moduleSerial.read(); // Discard if buffer full
           }
        }
        delay(5); // Small delay to prevent busy wait
    }

    // Print received data after wait is over
    if (responseBufferPos > 0) {
       Serial.print(F("Received during wait: ["));
       for(uint16_t i=0; i<responseBufferPos; i++) {
          char c = responseBuffer[i];
          if (isprint(c)) { Serial.print(c); }
          else if (c == '\r') { Serial.print(F("<CR>")); }
          else if (c == '\n') { Serial.print(F("<LF>")); }
          else { Serial.print('.'); }
       }
       Serial.println(F("]"));
    } else {
       Serial.println(F("Received during wait: [Nothing]"));
    }
}


/**
 * @brief Loops sending "AT" until "OK" is found, mimicking simple code.
 * @param maxRetries Max number of AT commands to send.
 * @return true if OK received, false otherwise.
 */
bool waitForInitialOK(int maxRetries) {
    for (int i = 0; i < maxRetries; i++) {
        Serial.print(F("Sending AT (Attempt ")); Serial.print(i + 1); Serial.print(F(")... "));

        // 1. Send command (no buffer clear before)
        moduleSerial.println("AT");

        // 2. Wait and read response into global buffer
        readSerialResponse(1000); // Wait 1 second reading response

        // 3. Check global buffer for "OK"
        if (strstr(responseBuffer, "OK") != NULL) {
            Serial.println(F("OK received!"));
            return true; // Success
        }

        Serial.println(F("No OK yet."));
        delay(500); // Wait before next retry
    }
    return false; // Failed after retries
}


/**
 * @brief Sends a command, waits for a response, checks for expected substring. Very simple.
 * @param command The AT command to send.
 * @param expectedResponse The substring expected in a successful response.
 * @param timeoutMillis How long to wait for the response after sending.
 * @param retries How many times to retry sending the command if expected response not found.
 * @return true if expected response found within any retry, false otherwise.
 */
bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries) {
     for (int i=0; i < retries; i++) {
        // Optionally clear hardware buffer before sending? Let's try WITHOUT first.
        // clearSerialBuffer();

        Serial.print(F("Sending [Try ")); Serial.print(i+1); Serial.print(F("]: ")); Serial.println(command);
        moduleSerial.println(command);

        // Wait and read response
        readSerialResponse(timeoutMillis);

        // Check for expected response
        if (strstr(responseBuffer, expectedResponse) != NULL) {
           Serial.println(F(">>> Expected response found."));
           return true; // Success
        }

        // Check for error (optional, basic)
        if (strstr(responseBuffer, "ERROR") != NULL) {
           Serial.println(F(">>> ERROR response detected."));
           // return false; // Fail immediately on error? Or allow retry? Let's allow retry for now.
        }

        Serial.println(F(">>> Expected response NOT found."));
        if (i < retries - 1) {
           Serial.println(F("    Retrying..."));
           delay(500); // Wait a bit before retrying
        }
     }
     Serial.println(F(">>> Command failed after all retries."));
     return false; // Failed after all retries
}
