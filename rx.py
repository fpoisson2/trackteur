#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct

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

    # RegModemConfig1 (0x1D): BW + CR + Explicit/Implicit Header
    # BW = 0b0110 (62.5 kHz) [bits 7-4]
    # CR = 0b100 (4/8) [bits 3-1]
    # Explicit header = 0 [bit 0]
    # 0110 100 0 = 0x68
    spi_write(0x1D, 0x78)
    
    # RegModemConfig2 (0x1E): SF + other settings
    # SF = 12 (0b1100) [bits 7-4]
    # TxContinuousMode = 0 [bit 3]
    # RxPayloadCrcOn = 1 [bit 2]
    # SymbTimeout MSB = 00 [bits 1-0]
    # 1100 1 1 00 = 0xC4
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3 (0x26): Low Data Rate Optimization + others
    # Low Data Rate Optimization = 1 [bit 3]
    # AgcAutoOn = 1 [bit 2]
    # Reserved = 00000 (other bits)
    # 0000 1 1 00 = 0x0C
    spi_write(0x26, 0x0C)

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
            print(f"IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
            
            # Check if RX done flag is set (bit 6)
            if irq_flags & 0x40:
                # Clear IRQ flags
                spi_write(0x12, 0xFF)
                
                # Read payload length
                nb_bytes = spi_read(0x13)
                print(f"Packet size: {nb_bytes} bytes")
                
                # Read current FIFO address
                current_addr = spi_read(0x10)
                
                # Set FIFO address pointer
                spi_write(0x0D, current_addr)
                
                # Read payload
                payload = bytearray()
                for _ in range(nb_bytes):
                    payload.append(spi_read(0x00))

                print(f"Raw RX Payload ({len(payload)} bytes): {payload.hex()}")
                
                # If payload size is as expected (14 bytes), decode it
                if len(payload) == 14:
                    try:
                        # Unpack binary payload using big-endian format:
                        # >   : big-endian
                        # i   : 4-byte signed integer (latitude)
                        # i   : 4-byte signed integer (longitude)
                        # H   : 2-byte unsigned integer (altitude) - CAPITAL H for unsigned
                        # I   : 4-byte unsigned integer (timestamp)
                        lat, lon, alt, timestamp = struct.unpack(">iiHI", payload)
                        
                        # Convert back to float with 6 decimal precision
                        latitude = lat / 1_000_000.0
                        longitude = lon / 1_000_000.0
                        time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                        
                        print(f"Received: Latitude={latitude}, Longitude={longitude}, Altitude={alt}m, Timestamp={time_str}")
                    except struct.error as e:
                        print("Error decoding payload:", e)
                else:
                    # Fallback: print raw payload if size doesn't match
                    print(f"Unexpected payload size: {nb_bytes} bytes (expected 14)")
                    try:
                        message = ''.join(chr(b) for b in payload if 32 <= b <= 126)  # Convert printable ASCII only
                        print(f"Raw data: {payload.hex()}")
                        print(f"As text: {message}")
                    except Exception as e:
                        print(f"Error processing raw payload: {e}")
            
            # Also check for other IRQ flags for debugging
            if irq_flags & 0x20:  # PayloadCrcError
                print("CRC error detected")
                spi_write(0x12, 0xFF)  # Clear all IRQ flags
                
        time.sleep(0.01)  # Small delay to prevent CPU hogging

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
