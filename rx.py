#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import requests  # Import the requests library for HTTP communication

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

# Fixed frequency configuration
FREQUENCY = 915000000  # 915 MHz
FXTAL = 32000000
FRF_FACTOR = 2**19

# Calculate frequency register values
frf = int((FREQUENCY * FRF_FACTOR) / FXTAL)
FRF_MSB = (frf >> 16) & 0xFF
FRF_MID = (frf >> 8) & 0xFF
FRF_LSB = frf & 0xFF

# Traccar server configuration
TRACCAR_URL = "http://trackteur.ve2fpd.com:5055"  # Adjust port if needed
DEVICE_ID = "212901"  # Replace with your device's unique ID registered in Traccar

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

def set_frequency():
    spi_write(0x06, FRF_MSB)  # RegFrfMsb
    spi_write(0x07, FRF_MID)  # RegFrfMid
    spi_write(0x08, FRF_LSB)  # RegFrfLsb

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

    # Set fixed frequency (915 MHz)
    set_frequency()

    # RegModemConfig1: BW 125 kHz, CR 4/8, Implicit Header
    spi_write(0x1D, 0x78)
    
    # RegModemConfig2: SF12, CRC on
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LDRO on, AGC on
    spi_write(0x26, 0x0C)

    # Preamble length: 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)

    # Map DIO0 to RxDone (bit 7:6 = 00)
    spi_write(0x40, 0x00)

    # Set FIFO RX base addr and pointer
    spi_write(0x0F, 0x00)
    spi_write(0x0D, 0x00)

    # Continuous RX mode
    spi_write(0x01, 0x85)
    
    print(f"LoRa module initialized. Listening on {FREQUENCY/1000000} MHz")

def send_to_traccar(latitude, longitude, altitude, timestamp):
    """Send GPS data to Traccar server using the OSMAnd protocol."""
    params = {
        "id": DEVICE_ID,
        "lat": latitude,
        "lon": longitude,
        "altitude": altitude,
        "timestamp": timestamp
    }
    try:
        response = requests.get(TRACCAR_URL, params=params, timeout=10)
        print(f"Traccar response status: {response.status_code}")
        print(f"Traccar response content: {response.text}")
        if response.status_code == 200:
            print("Data successfully sent to Traccar server")
        else:
            print(f"Failed to send data to Traccar. Status code: {response.status_code}")
    except requests.RequestException as e:
        print(f"Error sending data to Traccar: {e}")

def receive_loop():
    print(f"Listening for incoming LoRa packets on {FREQUENCY/1000000} MHz...")
    while True:
        irq_flags = spi_read(0x12)
        
        # Check for RX Done (bit 6)
        if irq_flags & 0x40:
            print(f"IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
            
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
            
            # Decode payload if 14 bytes
            if len(payload) == 14:
                try:
                    lat, lon, alt, timestamp = struct.unpack(">iiHI", payload)
                    latitude = lat / 1_000_000.0
                    longitude = lon / 1_000_000.0
                    time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                    print(f"Received: Latitude={latitude}, Longitude={longitude}, Altitude={alt}m, Timestamp={time_str}")
                    
                    # Send data to Traccar
                    send_to_traccar(latitude, longitude, alt, timestamp)
                except struct.error as e:
                    print("Error decoding payload:", e)
            else:
                print(f"Unexpected payload size: {nb_bytes} bytes (expected 14)")
                try:
                    message = ''.join(chr(b) for b in payload if 32 <= b <= 126)
                    print(f"Raw data: {payload.hex()}")
                    print(f"As text: {message}")
                except Exception as e:
                    print(f"Error processing raw payload: {e}")
            
            # Check for CRC error
            if irq_flags & 0x20:
                print("CRC error detected")
        
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