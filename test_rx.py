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
    
    # Verify module presence
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    # Basic LoRa configuration
    spi_write(0x01, 0x80)  # Sleep mode
    time.sleep(0.1)
    
    # Set frequency to 915 MHz
    frf = int((915000000 * 2**19) / 32000000)
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb
    
    # Basic modem config: BW 125 kHz, CR 4/5, SF 7
    spi_write(0x1D, 0x72)  # RegModemConfig1
    spi_write(0x1E, 0x74)  # RegModemConfig2
    
    # Set FIFO RX base addr and pointer
    spi_write(0x0F, 0x00)
    spi_write(0x0D, 0x00)
    
    # Continuous RX mode
    spi_write(0x01, 0x85)

def receive_loop():
    print("Listening for LoRa packets...")
    while True:
        # Check for RX Done (bit 6)
        irq_flags = spi_read(0x12)
        if irq_flags & 0x40:
            # Clear IRQ flags
            spi_write(0x12, 0xFF)
            
            # Get payload length
            nb_bytes = spi_read(0x13)
            
            # Get current FIFO address
            current_addr = spi_read(0x10)
            
            # Set FIFO pointer
            spi_write(0x0D, current_addr)
            
            # Read payload
            payload = bytearray()
            for _ in range(nb_bytes):
                payload.append(spi_read(0x00))
            
            # Print packet information
            print(f"\nNew packet received at {time.strftime('%H:%M:%S')}")
            print(f"Size: {nb_bytes} bytes")
            print(f"Raw bytes: {payload.hex()}")
            
            # Try to decode as text if possible
            try:
                text = ''.join(chr(b) for b in payload if 32 <= b <= 126)
                if text:
                    print(f"As text: {text}")
            except Exception:
                pass
            
            # Check for CRC error
            if irq_flags & 0x20:
                print("CRC error detected")
        
        time.sleep(0.01)  # Prevent CPU hogging

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