# Trackteur GPS Tracker (LilyGo A7670G)

This document provides essential information about the Trackteur GPS Tracker project, focusing on compilation, uploading, and key configurations.

## Project Overview

The Trackteur GPS Tracker project is designed for the LilyGo A7670G board to send GPS data to a Traccar server and optionally back up data to an SD card. It utilizes the `TinyGsm` library for modem communication and `TinyGPSPlus` for GPS data parsing.

## Compilation and Upload using Arduino CLI

### Prerequisites

Ensure you have `arduino-cli` installed and configured with the `esp32` core.

### Compiling the Code

To compile the sketch, navigate to the project root directory and use the following command:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 ./code/TraccarGPS
```

-   `--fqbn esp32:esp32:esp32`: Specifies the Fully Qualified Board Name for a generic ESP32 Dev Module. This board definition was found to be compatible with the LilyGo A7670G for this project.
-   `./code/TraccarGPS`: Path to the main sketch directory.

### Uploading the Code

After successful compilation, upload the compiled sketch to your LilyGo A7670G board. Ensure your board is connected and identified as `/dev/ttyACM0` (or your equivalent serial port).

```bash
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32 ./code/TraccarGPS
```

-   `-p /dev/ttyACM0`: Specifies the serial port to which your board is connected. Adjust this if your port is different.
-   `--fqbn esp32:esp32:esp32`: Same as above, for the generic ESP32 Dev Module.
-   `./code/TraccarGPS`: Path to the main sketch directory.

## Key Configurations

-   **Traccar Server URL**: The current Traccar server endpoint is configured to `https://serveur1e.trackteur.cc`. This can be changed in `code/TraccarGPS/config.h`.
-   **GPS Initialization**: The code now includes specific PMTK commands sent to the GPS module during initialization to ensure it wakes up and performs a hot start for faster satellite fixes:
    -   Wake command: `$PMTK010,001*2E`
    -   Hot start command: `$PMTK101*32`
-   **Embedded Git Repository**: The `code/temp_libs/LilyGO-T-A76XX` directory is an embedded Git repository, which contains the LilyGo T-A76XX library. When cloning this project, you might need to handle this as a submodule or clone it separately.

For further details on hardware pins, network APN settings, and other configurations, refer to `code/TraccarGPS/config.h` and `code/TraccarGPS/utilities.h`.
