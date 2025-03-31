import spidev
import RPi.GPIO as GPIO
import time

# GPIO Pins (BCM numbering corrected!)
RESET = 17   # GPIO17 (WiringPi GPIO0)
NSS = 25     # GPIO25 (WiringPi GPIO6)
DIO0 = 4     # GPIO4 (WiringPi GPIO7)

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

GPIO.setup(NSS, GPIO.OUT, initial=GPIO.HIGH)
GPIO.setup(RESET, GPIO.OUT, initial=GPIO.HIGH)
GPIO.setup(DIO0, GPIO.IN)

# Reset SX1276 module properly
GPIO.output(RESET, GPIO.LOW)
time.sleep(0.2)
GPIO.output(RESET, GPIO.HIGH)
time.sleep(0.2)

# Initialize SPI (hardware SPI)
spi = spidev.SpiDev()
spi.open(0, 0)  # Bus 0, device 0 (CS managed manually with NSS)
spi.max_speed_hz = 5000000
spi.mode = 0b00

def spi_read(register):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([register & 0x7F])  # Read mode (MSB=0)
    result = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    return result

# Test: read SX1276 version (register 0x42 should be 0x12)
version = spi_read(0x42)
print(f"SX1276 Version: {hex(version)}")

spi.close()
GPIO.cleanup()
