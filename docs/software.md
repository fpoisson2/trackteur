# Logiciel

## Prérequis

- Arduino IDE (>=1.8)
- Bibliothèques : TinyGPS++, SdFat, GSMStream (ou équivalent)

## Installation

```bash
arduino-cli compile --fqbn arduino:avr:nano code/code.ino
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano code/code.ino
```

## Configuration

- APN opérateur
- URL Traccar
- Config pins dans common.h