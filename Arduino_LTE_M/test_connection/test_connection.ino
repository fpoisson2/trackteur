#include <SoftwareSerial.h>
#include <stdio.h> // Include for snprintf
#include <string.h> // Include for strstr, memset

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

// --- TCP Test Targets ---
const char* TEST_PUBLIC_IP    = "1.1.1.1";       // Public test server (e.g., Cloudflare DNS)
const int   TEST_PUBLIC_PORT  = 80;              // Standard HTTP port
const char* TEST_PRIVATE_IP   = "10.82.185.13";  // Your private server IP
const int   TEST_PRIVATE_PORT = 5055;            // Your private server port

// --- Response Buffer ---
#define RESPONSE_BUFFER_SIZE 200
char responseBuffer[RESPONSE_BUFFER_SIZE];
uint16_t responseBufferPos = 0;

// --- Command Buffer ---
#define CMD_BUFFER_SIZE 100
char cmdBuffer[CMD_BUFFER_SIZE];

// --- Function Declarations ---
bool waitForInitialOK(int maxRetries);
bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries);
void clearSerialBuffer();
void readSerialResponse(unsigned long waitMillis);


// ========================================================================
// SETUP - Performs the entire sequence including TCP tests
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
    // Attempt CIPSHUT first in case a connection was previously open
    executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 2); // Don't fail if this returns ERROR

    success &= executeSimpleCommand("AT+CFUN=0", "OK", 5000, 3);
    success &= executeSimpleCommand("AT+CNMP=38", "OK", 2000, 3); // Prefer LTE
    success &= executeSimpleCommand("AT+CMNB=1", "OK", 2000, 3);  // Prefer NB-IoT
  }
  if (success) {
     Serial.println(F("Turning radio ON (CFUN=1) and rebooting..."));
     // Send command, module resets. We don't expect "OK" here immediately.
     moduleSerial.println("AT+CFUN=1,1");
     // We need to wait for reboot regardless of command success tracking here.
     // Let's reset 'success' state after reboot check.
  }

  // --- Wait and Re-check Communication Post-Reset ---
  // Always perform this wait and check after CFUN=1,1
  const unsigned long rebootWaitMillis = 25000; // 25 seconds wait
  Serial.print(F("Waiting ")); Serial.print(rebootWaitMillis / 1000); Serial.println(F("s for module reboot/network search..."));
  unsigned long startWait = millis();
  while(millis() - startWait < rebootWaitMillis) {
      readSerialResponse(100); // Read any incoming URCs during the wait
      delay(100);
  }


  Serial.println(F("Attempting communication after reset..."));
  if (!waitForInitialOK(15)) { // Use the same reliable check
      Serial.println(F("ERROR: Module unresponsive after CFUN=1,1 reset."));
      success = false; // Now set success based on post-reboot check
  } else {
      Serial.println(F("Communication re-established after reset."));
      success = true; // Communication OK after reset
  }


  // --- STEP 2: Check Network Registration ---
  if (success) {
     Serial.println(F("\n=== STEP 2: Check Network Registration ==="));
     bool registered = false;
     const int maxRegRetries = 15; // More retries might be needed
     for (int i = 0; i < maxRegRetries && success; i++) {
         Serial.print(F("Registration check attempt ")); Serial.print(i + 1); Serial.print(F("/")); Serial.println(maxRegRetries);

         // Check SIM status - crucial
         if (!executeSimpleCommand("AT+CPIN?", "READY", 3000, 1)) {
            Serial.println(F("ERROR: SIM not READY. Check SIM card."));
            success = false; // Cannot proceed without SIM
            break; // Exit registration loop
         }

         executeSimpleCommand("AT+CSQ", "+CSQ", 2000, 1);   // Check signal quality (print response)

         // Check CREG/CEREG - Improved Check
         bool creg_ok = false;
         executeSimpleCommand("AT+CREG?", "+CREG:", 2000, 1); // Just send command and read response buffer
         if (strstr(responseBuffer, "+CREG: ") != NULL && (strstr(responseBuffer, ",1") != NULL || strstr(responseBuffer, ",5") != NULL)) {
             Serial.println(F("DEBUG: CREG Registered (Home or Roaming)"));
             creg_ok = true;
         } else {
             Serial.println(F("DEBUG: CREG Not Registered or Unknown State"));
             // Optional: Print the actual response for debugging CREG state
             // Serial.print(F("CREG Response was: ")); Serial.println(responseBuffer);
         }


         bool cereg_ok = false;
         executeSimpleCommand("AT+CEREG?", "+CEREG:", 2000, 1); // Just send command and read response buffer
         if (strstr(responseBuffer, "+CEREG: ") != NULL && (strstr(responseBuffer, ",1") != NULL || strstr(responseBuffer, ",5") != NULL)) {
             Serial.println(F("DEBUG: CEREG Registered (Home or Roaming)"));
             cereg_ok = true;
         } else {
             Serial.println(F("DEBUG: CEREG Not Registered or Unknown State"));
             // Optional: Print the actual response for debugging CEREG state
             // Serial.print(F("CEREG Response was: ")); Serial.println(responseBuffer);
         }

         // Use CEREG for LTE/NB-IoT focus, but accept CREG as fallback
         if (cereg_ok || creg_ok) {
             Serial.println(F("Registered on network."));
             registered = true;
             break; // Exit loop
         } else {
             Serial.println(F("Not registered yet..."));
             delay(5000); // Wait longer between registration checks
         }
     } // end for loop (registration retries)

     if (!registered) {
         Serial.println(F("ERROR: Failed to register on the network after multiple attempts."));
         success = false;
     }
  } // end if(success) before registration check


  // --- STEP 3: Setup APN and Activate PDP Context ---
  if (success) {
     Serial.println(F("\n=== STEP 3: Setup APN and Activate PDP Context ==="));

     // Define PDP Context (CID=1) with the specified APN
     snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
     success &= executeSimpleCommand(cmdBuffer, "OK", 5000, 3);
  }
  if (success) {
      // Attach to GPRS/Packet Service
     success &= executeSimpleCommand("AT+CGATT=1", "OK", 10000, 3); // Attaching can take time
  }
  if (success) {
     // Activate PDP Context (CID=1) using CNACT (preferred for newer modules)
     // Note: Some older modules might require AT+CGACT=1,1
     // We will use the specific APN with CNACT=1,<apn> syntax if needed, but
     // often just activating the predefined context (CID=1) is enough if CGDCONT was set.
     // Let's try activating CID 1 directly first.
     Serial.println(F("Attempting PDP context activation (AT+CNACT=1)..."));
     success &= executeSimpleCommand("AT+CNACT=1", "OK", 30000, 2); // Activation can take significant time

     // If AT+CNACT=1 failed, maybe the module requires the APN in the command?
     if (!success) {
        Serial.println(F("AT+CNACT=1 failed, trying AT+CNACT=1,\"apn\"..."));
        snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CNACT=1,\"%s\"", APN);
        // Reset success flag before retrying this specific step
        success = executeSimpleCommand(cmdBuffer, "OK", 30000, 1);
     }
  }
  if (success) {
     // Verify PDP context activation status
     Serial.println(F("Verifying PDP context status (AT+CNACT?)..."));
     // Look for "+CNACT: 1," indicating CID 1 is active (state 1 = active)
     if (!executeSimpleCommand("AT+CNACT?", "+CNACT: 1,", 5000, 3)) {
         Serial.println(F("Warning: Could not verify active PDP context via AT+CNACT?. Checking IP..."));
         // As a fallback, try getting the IP address. If we get one, assume context is active.
         if (executeSimpleCommand("AT+CGPADDR=1", "+CGPADDR: 1,", 5000, 1)) {
             Serial.println(F("Got IP address via AT+CGPADDR=1. Assuming PDP context is active."));
             // Keep success = true
         } else {
             Serial.println(F("ERROR: Failed to verify PDP context activation via CNACT? or CGPADDR."));
             success = false;
         }
     } else {
         Serial.println(F("PDP Context active confirmed via AT+CNACT?."));
         // Optional: Print the CNACT response from buffer for IP info
         Serial.print(F("CNACT Response: ")); Serial.println(responseBuffer);
     }
  }

  // --- End of Core Setup Sequence ---
  if (success) {
     Serial.println(F("\n=== CORE CONFIGURATION AND CONNECTION COMPLETE ==="));
  } else {
      Serial.println(F("\n--- ERRORS ENCOUNTERED DURING CORE SETUP ---"));
      // Decide if you want to halt or proceed to TCP tests even if core setup failed
      // For now, we'll proceed to TCP tests but note the failure.
      Serial.println(F("Proceeding to TCP tests despite core setup issues (may fail)..."));
      // Optionally reset success = true here if you want TCP tests to run independently
      // success = true;
  }


// --- === NEW SECTION: TCP Connection Tests === ---

  // Only proceed with TCP tests if core setup (incl. PDP activation) was potentially successful
  if (success) {

      // --- STEP 4: Try CIP approach (SIM7000 specific diagnostics added) ---
      Serial.println(F("\n=== STEP 4: Try CIP TCP Connection (SIM7000 Diagnostics) ==="));

      // --- Check CIPMUX state ---
      Serial.println(F("-- Check Connection Mode (AT+CIPMUX?) --"));
      // Expect "+CIPMUX: 0" or "+CIPMUX: 1"
      if (executeSimpleCommand("AT+CIPMUX?", "+CIPMUX:", 3000, 1)) {
          // You can manually check the Serial Monitor output here to see if it's 0 or 1
          Serial.print(F("CIPMUX state response: ")); Serial.println(responseBuffer);
          // Let's assume CIPMUX=0 (single connection) for now unless proven otherwise
      } else {
          Serial.println(F("Could not determine CIPMUX state. Assuming 0 (Single Connection)."));
      }
      // Optional: Force single connection mode (if multi-mode is suspected and not desired)
      // Serial.println(F("-- Setting CIPMUX=0 (Single Connection Mode) --"));
      // executeSimpleCommand("AT+CIPMUX=0", "OK", 3000, 1);
      // delay(500);


      // --- Check IP Stack Status ---
      Serial.println(F("-- Check IP Stack Status (AT+CIPSTATUS) --"));
      // Common states after PDP active: "INITIAL", "IP STATUS", "PDP DEACT" (if context dropped)
      // We want "INITIAL" or "IP STATUS" ideally.
      // Let's just execute the command and print the response for diagnosis.
      // Use a generic expected response like "OK" or even just part of the command echo.
      if (executeSimpleCommand("AT+CIPSTATUS", "OK", 5000, 1)) {
          Serial.print(F("CIPSTATUS response: ")); Serial.println(responseBuffer);
          // Check if the response contains "INITIAL" or "IP STATUS"
          if (strstr(responseBuffer, "INITIAL") != NULL || strstr(responseBuffer, "IP STATUS") != NULL) {
              Serial.println(F("IP Stack state appears ready."));
          } else {
              Serial.println(F("WARNING: IP Stack state might not be ready for connection."));
              // You could choose to 'return' or set 'success = false' here if state is wrong.
          }
      } else {
          Serial.println(F("Failed to get CIPSTATUS. Proceeding anyway..."));
      }
      delay(1000); // Small delay after checking status

      // --- Attempt Connection (Assuming CIPMUX=0 Syntax) ---
      Serial.println(F("-- Attempt CIP START to PUBLIC IP/Port (Syntax for CIPMUX=0) --"));
      // NOTE: Changed command format - removed the "=1" part assuming CIPMUX=0
      snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CIPSTART=\"TCP\",\"%s\",%d", TEST_PUBLIC_IP, TEST_PUBLIC_PORT);

      bool cip_public_ok = false;
      // Expect "OK" first, then async "CONNECT OK". Some firmwares might give "CONNECT OK" directly.
      // Let's check for "OK" primarily, then check buffer for "CONNECT OK" / "ALREADY CONNECT"
      if (executeSimpleCommand(cmdBuffer, "OK", 20000, 1)) { // Check for initial OK
            // Wait a bit more for potential async "CONNECT OK" / "ALREADY CONNECT"
            readSerialResponse(5000); // Read for up to 5 more seconds
            if (strstr(responseBuffer, "CONNECT OK") != NULL) {
                Serial.println(F("Public CIP Result: CONNECT OK"));
                cip_public_ok = true;
            } else if (strstr(responseBuffer, "ALREADY CONNECT") != NULL) {
                Serial.println(F("Public CIP Result: ALREADY CONNECT"));
                cip_public_ok = true; // Treat as success
            } else if (strstr(responseBuffer, "ERROR") != NULL) {
                 Serial.println(F("Public CIP Result: ERROR response after OK")); // e.g., DNS fail?
            } else {
                Serial.println(F("Public CIP Result: OK received, but no CONNECT OK/ALREADY CONNECT/ERROR followed. Check manually."));
                // Sometimes OK is the only success indicator if server immediately closes or data arrives fast.
                // Consider this potentially successful depending on application.
                // cip_public_ok = true; // Uncomment if OK alone is sufficient indication
            }
      } else {
          // Check if the error happened directly (like before)
          if (strstr(responseBuffer, "ERROR") != NULL) {
              Serial.println(F("Public CIP Result: Direct ERROR response (Command likely wrong syntax/state)"));
          } else if (strstr(responseBuffer, "CONNECT FAIL") != NULL) {
               Serial.println(F("Public CIP Result: CONNECT FAIL response")); // Explicit failure URC
          }
          else {
              Serial.println(F("Public CIP Result: No OK or recognizable response (Timeout?)"));
          }
      }

      // Close the connection using CIPCLOSE or CIPSHUT
      // CIPCLOSE is better if you might want other connections open (if CIPMUX=1)
      // CIPSHUT closes everything and resets the IP stack state, good for testing.
      Serial.println(F("-- Attempting CIPSHUT --"));
      if (executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 1)) {
          Serial.println(F("CIPSHUT successful."));
      } else {
          Serial.println(F("CIPSHUT failed or not needed."));
      }
      delay(1000); // Pause after shutting down


      // --- Re-check status before next attempt ---
      Serial.println(F("-- Re-Check IP Stack Status (AT+CIPSTATUS) --"));
       if (executeSimpleCommand("AT+CIPSTATUS", "OK", 5000, 1)) {
          Serial.print(F("CIPSTATUS response: ")); Serial.println(responseBuffer);
          // Should likely be "INITIAL" or "IP DEINITIAL" after SHUT OK
       } else {
           Serial.println(F("Failed to get CIPSTATUS."));
       }
       delay(500);

      // --- Attempt Connection to Private IP (Assuming CIPMUX=0 Syntax) ---
      Serial.println(F("\n-- Attempt CIP START to PRIVATE IP/Port (Syntax for CIPMUX=0) --"));
      snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CIPSTART=\"TCP\",\"%s\",%d", TEST_PRIVATE_IP, TEST_PRIVATE_PORT);
      bool cip_private_ok = false;
       if (executeSimpleCommand(cmdBuffer, "OK", 20000, 1)) { // Check for initial OK
            readSerialResponse(5000);
            if (strstr(responseBuffer, "CONNECT OK") != NULL) {
                Serial.println(F("Private CIP Result: CONNECT OK"));
                cip_private_ok = true;
            } else if (strstr(responseBuffer, "ALREADY CONNECT") != NULL) {
                Serial.println(F("Private CIP Result: ALREADY CONNECT"));
                cip_private_ok = true;
            } else if (strstr(responseBuffer, "ERROR") != NULL) {
                 Serial.println(F("Private CIP Result: ERROR response after OK"));
            } else {
                Serial.println(F("Private CIP Result: OK received, but no CONNECT OK/ALREADY CONNECT/ERROR followed. Check manually."));
                 // cip_private_ok = true; // Uncomment if OK alone is sufficient indication
            }
      } else {
          if (strstr(responseBuffer, "ERROR") != NULL) {
              Serial.println(F("Private CIP Result: Direct ERROR response"));
          } else if (strstr(responseBuffer, "CONNECT FAIL") != NULL) {
               Serial.println(F("Private CIP Result: CONNECT FAIL response"));
          } else {
              Serial.println(F("Private CIP Result: No OK or recognizable response (Timeout?)"));
          }
      }

      // Close again
      Serial.println(F("-- Attempting CIPSHUT --"));
       if (executeSimpleCommand("AT+CIPSHUT", "SHUT OK", 5000, 1)) {
           Serial.println(F("CIPSHUT successful."));
       } else {
           Serial.println(F("CIPSHUT failed or not needed."));
       }
       delay(1000);

      // --- STEP 5: CSOC Test ---
      // Since CIP commands *should* work on SIM7000, let's focus on fixing that first.
      // You can leave the CSOC test code as is for now, or comment it out temporarily.
      // If CIP still fails after diagnostics, CSOC might be an alternative, but getting
      // ERROR on both suggests a more fundamental issue we need to solve first.
      Serial.println(F("\n=== STEP 5: Try CSOC TCP Connection (Skipping detailed diagnostics for now) ==="));
      // ... (keep existing CSOC code or comment out) ...
      // For now, let's assume it will likely fail similarly if CIP fails fundamentally.
       const int csoc_socket_id = 0; // Use socket 0 for simplicity
       Serial.println(F("-- Attempt CSOC Create Socket --"));
       bool csoc_socket_created = executeSimpleCommand("AT+CSOC=1,1,1", "OK", 5000, 1);
       // ... rest of CSOC logic ...
        if (!csoc_socket_created) {
            readSerialResponse(1000);
            if (strstr(responseBuffer, "+CSOC:") != NULL) {
                Serial.println(F("CSOC Create: Got +CSOC: URC (assuming success)"));
                csoc_socket_created = true;
            } else if (strstr(responseBuffer,"ERROR") != NULL) {
                 Serial.println(F("CSOC Create: Failed (ERROR response)"));
            }
             else {
                 Serial.println(F("CSOC Create: Failed (No OK or +CSOC: URC)"));
            }
        } else {
             Serial.println(F("CSOC Create: OK Received."));
        }

       if (csoc_socket_created) {
            // ... CSOCON / CSOCL logic ...
            Serial.println(F("-- CSOC Connect/Close steps would follow here --"));
            // Make sure to close the socket if created
             Serial.println(F("-- Attempt CSOC Close Socket (Cleanup) --"));
             snprintf(cmdBuffer, CMD_BUFFER_SIZE, "AT+CSOCL=%d", csoc_socket_id);
             executeSimpleCommand(cmdBuffer, "OK", 5000, 1); // Attempt close regardless
        } else {
            Serial.println(F("Skipping CSOC Connect tests because initial socket creation failed."));
        }


      Serial.println(F("\n=== TCP Connection Tests Finished ==="));

  } else {
       Serial.println(F("\n--- Skipping TCP Connection Tests due to errors in core setup ---"));
  }

  // --- Final Status ---
  Serial.println(F("\nSetup sequence complete. Entering idle loop."));
} // End setup()


// ========================================================================
// LOOP - Does nothing after setup
// ========================================================================
void loop() {
  // Read and print any unexpected URCs from the module during idle
  readSerialResponse(100);
  if(responseBufferPos > 0) {
      Serial.print(F("Idle URC?: "));
      Serial.println(responseBuffer);
  }
  delay(10000); // Prevent busy-looping excessively
}

// ========================================================================
// HELPER FUNCTIONS (Keep your existing helper functions as they are)
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
 * Prints the collected response to the main Serial port for debugging.
 */
void readSerialResponse(unsigned long waitMillis) {
    unsigned long start = millis();
    // Clear global buffer before reading new response
    memset(responseBuffer, 0, RESPONSE_BUFFER_SIZE);
    responseBufferPos = 0;
    bool anythingReceived = false;

    while (millis() - start < waitMillis) {
        while (moduleSerial.available()) {
            anythingReceived = true;
            if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
                char c = moduleSerial.read();
                // Filter out null bytes which can terminate strings early
                if (c != '\0') {
                    responseBuffer[responseBufferPos++] = c;
                    responseBuffer[responseBufferPos] = '\0'; // Null-terminate
                }
            } else {
                // Buffer full, discard character but acknowledge receipt
                moduleSerial.read();
            }
        }
        // Don't delay excessively within the reading loop if data is actively coming in
        // Only delay if nothing was available in the last check
        if (!moduleSerial.available()) {
           delay(5); // Small delay to yield if buffer is empty
        }
    }

    // Print received data after wait is over, only if something was actually received
    if (responseBufferPos > 0) {
        Serial.print(F("Received during wait: ["));
        for(uint16_t i=0; i<responseBufferPos; i++) {
            char c = responseBuffer[i];
             // Make CR/LF visible, print others if printable, else '.'
            if (c == '\r') { Serial.print(F("<CR>")); }
            else if (c == '\n') { Serial.print(F("<LF>")); }
            else if (isprint(c)) { Serial.print(c); }
            else { Serial.print('.'); }
        }
        Serial.println(F("]"));
    } else if (anythingReceived) {
        // We received chars but they might have been discarded (e.g., buffer full or null bytes)
         Serial.println(F("Received during wait: [Data received but buffer empty/discarded]"));
    }
    // No need to print "Nothing" if truly nothing came in, reduces log noise.
    // else {
    //    Serial.println(F("Received during wait: [Nothing]"));
    // }
}


/**
 * @brief Loops sending "AT" until "OK" is found in the response buffer.
 * @param maxRetries Max number of AT commands to send.
 * @return true if OK received, false otherwise.
 */
bool waitForInitialOK(int maxRetries) {
    for (int i = 0; i < maxRetries; i++) {
        Serial.print(F("Sending AT (Attempt ")); Serial.print(i + 1); Serial.print(F(")... "));

        // 1. Clear potential leftover URCs before sending command
        // clearSerialBuffer(); // Optional: Might clear useful URCs if module sends them spontaneously
        // Let's rely on readSerialResponse clearing the buffer *before reading*

        // 2. Send command
        moduleSerial.println("AT");

        // 3. Wait and read response into global buffer
        readSerialResponse(1000); // Wait 1 second reading response

        // 4. Check global buffer for "OK" surrounded by CR/LF for robustness
        // Simple check first:
        if (strstr(responseBuffer, "OK") != NULL) {
        // More robust check (optional):
        // if (strstr(responseBuffer, "\r\nOK\r\n") != NULL) {
            Serial.println(F("OK received!"));
            return true; // Success
        }

        Serial.println(F("No OK yet."));
        delay(500); // Wait before next retry
    }
    return false; // Failed after retries
}


/**
 * @brief Sends a command, waits for a response, checks for expected substring.
 * @param command The AT command to send (C-string). MUST be null-terminated.
 * @param expectedResponse The substring expected in a successful response (C-string).
 * @param timeoutMillis How long to wait for the response AFTER sending.
 * @param retries How many times to retry sending the command if expected response not found.
 * @return true if expected response found within any retry, false otherwise.
 */
bool executeSimpleCommand(const char* command, const char* expectedResponse, unsigned long timeoutMillis, int retries) {
     for (int i=0; i < retries; i++) {
        // Clear serial buffer *before* sending to only capture response to *this* command
        // clearSerialBuffer(); // Disabled: readSerialResponse clears the buffer anyway

        Serial.print(F("Sending [Try ")); Serial.print(i+1); Serial.print(F("/") ); Serial.print(retries); Serial.print(F("]: ")); Serial.println(command);
        moduleSerial.println(command);

        // Wait and read response into the global responseBuffer
        readSerialResponse(timeoutMillis);

        // Check for expected response SUBSTRING anywhere in the buffer
        if (strstr(responseBuffer, expectedResponse) != NULL) {
            Serial.println(F(">>> Expected response found."));
            return true; // Success
        }

        // Check for "ERROR" response (basic check)
        if (strstr(responseBuffer, "ERROR") != NULL) {
            Serial.println(F(">>> ERROR response detected in buffer."));
            // Optional: Fail immediately on ERROR? Or allow retry? Allowing retry.
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
