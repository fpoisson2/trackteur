#include <SoftwareSerial.h>

// --- Pin Definitions ---
const int powerPin = 2; // Pin to turn the module ON (set HIGH)
const int swRxPin = 3;  // Software Serial RX pin (connect to module's TX)
const int swTxPin = 4;  // Software Serial TX pin (connect to module's RX) <- NOTE: Changed from D2

// --- Software Serial Object ---
// Initialize SoftwareSerial library for communication with the module
SoftwareSerial moduleSerial(swRxPin, swTxPin);

// --- Baud Rate ---
const long moduleBaudRate = 9600;

// --- State Variable ---
bool moduleReady = false; // Flag to track if module responded with "OK"

void setup() {
  // Initialize hardware serial for debugging output (to Arduino IDE Serial Monitor)
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Arduino Nano Initialized.");

  // Configure the power pin as an output
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW); // Start with module OFF
  Serial.println("Module power pin configured (D2).");

  // Initialize Software Serial for the module
  moduleSerial.begin(moduleBaudRate);
  Serial.print("Software Serial initialized on Pins RX:");
  Serial.print(swRxPin);
  Serial.print(", TX:");
  Serial.print(swTxPin);
  Serial.print(" at ");
  Serial.print(moduleBaudRate);
  Serial.println(" baud.");

  // --- Power On Sequence ---
  Serial.println("Turning module ON (Setting D2 HIGH)...");
  digitalWrite(powerPin, HIGH); // Turn the module ON
  Serial.println("Waiting 2 seconds for module to boot...");
  delay(2000); // Wait 2 seconds
  Serial.println("Module boot wait complete.");
}

void loop() {
  // Only try to communicate if the module isn't marked as ready yet
  if (!moduleReady) {
    Serial.println("Sending 'AT' command to module...");
    moduleSerial.println("AT"); // Send AT command followed by newline

    // Wait a moment for a response
    delay(500); // Adjust this delay if needed based on module response time

    // Check for response
    String response = "";
    while (moduleSerial.available()) {
      char c = moduleSerial.read();
      response += c;
      // Small delay to allow buffer to fill slightly, helps prevent partial reads
      delay(2); 
    }

    // Trim whitespace from response (like leading/trailing \r\n)
    response.trim(); 

    if (response.length() > 0) {
        Serial.print("Received from module: '");
        Serial.print(response);
        Serial.println("'");
    }

    // Check if the response contains "OK"
    // Using indexOf is often more reliable than exact match due to potential extra characters (\r\n)
    if (response.indexOf("OK") != -1) {
      Serial.println(">>> Module responded with OK! <<<");
      moduleReady = true; // Set the flag so we stop sending AT
    } else if (response.length() > 0) {
      Serial.println("Module did not respond with OK yet. Retrying...");
    } else {
        Serial.println("No response received from module. Retrying...");
    }
    
    // Wait before sending the next AT command if not ready
    if (!moduleReady) {
        delay(1000); // Wait 1 second before retrying
    }

  } else {
    // Module is ready, do nothing further or add other logic here
    Serial.println("Module is ready. Holding state.");
    // Prevent spamming the serial monitor
    delay(5000); 
  }
}
