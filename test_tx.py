#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys

# Pin definitions (BCM mode)
RESET = 17  # Reset pin
NSS = 25    # SPI Chip Select pin
DIO0 = 4    # DIO0 pin for TxDone interrupt

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

def spi_write(addr, value):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, value])
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

    # Set to Sleep mode with LoRa enabled
    spi_write(0x01, 0x80)
    time.sleep(0.1)

    # Set frequency to 902.2 MHz (fixed)
    spi_write(0x06, 0x6C)  # RegFrfMsb
    spi_write(0x07, 0x19)  # RegFrfMid
    spi_write(0x08, 0x9A)  # RegFrfLsb

    # RegModemConfig1: BW 125 kHz, CR 4/8, Implicit Header
    spi_write(0x1D, 0x78)
    
    # RegModemConfig2: SF12, CRC on
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LDRO on, AGC on
    spi_write(0x26, 0x0C)

    # Preamble length: 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)

    # Set PA configuration: +20 dBm with PA_BOOST
    spi_write(0x09, 0x8F)

    # Set FIFO TX base address and pointer
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)

    # Map DIO0 to TxDone (bit 7:6 = 00)
    spi_write(0x40, 0x00)

    # Set to Standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def transmit_message(message):
    payload = message.encode('ascii')  # Convert string to bytes
    print(f"Transmitting: {message} ({len(payload)} bytes)")

    # Reset FIFO pointer
    spi_write(0x0D, 0x00)
    
    # Write payload to FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    # Set payload length
    spi_write(0x22, len(payload))
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Switch to TX mode
    spi_write(0x01, 0x83)
    
    # Wait for TxDone interrupt (timeout after 5 seconds)
    start_time = time.time()
    while time.time() - start_time < 5:
        if GPIO.input(DIO0) == 1:  # TxDone interrupt
            print("Transmission complete!")
            break
        time.sleep(0.01)
    
    if time.time() - start_time >= 5:
        print("TX timeout. IRQ flags:", hex(spi_read(0x12)))
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Return to Standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def cleanup():
    spi.close()
    GPIO.cleanup()

if __name__ == "__main__":
    try:
        init_lora()
        message = "Hello, World!"
        while True:
            transmit_message(message)
            time.sleep(5)  # Transmit every 5 seconds
    except KeyboardInterrupt:
        print("\nTerminating...")
        cleanup()
        sys.exit(0)