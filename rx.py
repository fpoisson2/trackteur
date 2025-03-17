#rx.py
#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys

# Pin definitions (BCM mode)
RESET = 17
NSS = 25
DIO0 = 4

# GPIO setup
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(RESET, GPIO.OUT)
GPIO.setup(NSS, GPIO.OUT)
GPIO.setup(DIO0, GPIO.IN)

# SPI setup
spi = spidev.SpiDev()
spi.open(0, 0)
spi.max_speed_hz = 5000000
spi.mode = 0b00

def spi_write(addr, val):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, val])
    GPIO.output(NSS, GPIO.HIGH)

def spi_read(addr):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr & 0x7F])
    val = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    return val

def reset_module():
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def init_lora():
    reset_module()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    spi_write(0x01, 0x80)  # LoRa sleep mode
    time.sleep(0.1)

    frequency = 915000000
    frf = int(frequency / 61.03515625)
    spi_write(0x06, (frf >> 16) & 0xFF)
    spi_write(0x07, (frf >> 8) & 0xFF)
    spi_write(0x08, frf & 0xFF)

    spi_write(0x1D, 0x62)  # BW=125kHz, CR=4/7
    spi_write(0x1E, 0x74)  # SF7, CRC on

    spi_write(0x20, 0x00)  # Preamble length MSB
    spi_write(0x21, 0x08)  # Preamble length LSB

    spi_write(0x40, 0x00)  # DIO0 mapped to RX done

    spi_write(0x0F, 0x00)  # FIFO RX base addr
    spi_write(0x0D, 0x00)  # FIFO addr ptr

    spi_write(0x01, 0x85)  # Continuous RX mode

def receive_loop():
    print("Listening for incoming LoRa packets...")
    while True:
        if GPIO.input(DIO0):
            irq_flags = spi_read(0x12)
            if irq_flags & 0x40:
                spi_write(0x12, 0xFF)
                nb_bytes = spi_read(0x13)
                current_addr = spi_read(0x10)
                spi_write(0x0D, current_addr)

                payload = []
                for _ in range(nb_bytes):
                    payload.append(spi_read(0x00))

                message = ''.join(chr(b) for b in payload)
                print(f"Received ({nb_bytes} bytes): {message}")

        time.sleep(0.01)

def cleanup():
    spi.close()
    GPIO.cleanup()

if __name__ == "__main__":
    try:
        init_lora()
        receive_loop()
    except KeyboardInterrupt:
        print("\nTerminating...")
        cleanup()
        sys.exit(0)

