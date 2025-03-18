#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import requests
import argparse
import logging

# Pin definitions (BCM mode) - Keep original defaults
RESET = 17
NSS = 25
DIO0 = 4

# Fixed frequency configuration - Keep original defaults
FREQUENCY = 915000000  # 915 MHz
FXTAL = 32000000
FRF_FACTOR = 2**19

# Traccar server configuration - Keep original defaults
TRACCAR_URL = "http://trackteur.ve2fpd.com:5055"
DEVICE_ID = "212901"

# Parse command-line arguments (optional functionality)
def parse_arguments():
    parser = argparse.ArgumentParser(description='LoRa receiver for GPS data with Traccar integration')
    
    # Radio configuration
    parser.add_argument('--freq', type=float,
                        help='Frequency in MHz (default: 915.0)')
    parser.add_argument('--device-id', type=str,
                        help='Device ID for Traccar (default: 212901)')
    parser.add_argument('--traccar-url', type=str,
                        help='Traccar server URL (default: http://trackteur.ve2fpd.com:5055)')
    
    # Debugging options
    parser.add_argument('--verbose', '-v', action='count', default=0,
                        help='Increase verbosity (use -v, -vv, or -vvv)')
    parser.add_argument('--no-traccar', action='store_true',
                        help='Disable sending data to Traccar server')
    
    args = parser.parse_args()
    
    # Only return args if any were explicitly provided
    # This ensures we use the original hardcoded values when no args are given
    if len(sys.argv) > 1:
        return args
    else:
        return None

# Configure based on command line or use defaults
args = parse_arguments()
verbose_mode = 0
if args:
    verbose_mode = args.verbose
    if args.freq:
        FREQUENCY = int(args.freq * 1000000)  # Convert MHz to Hz
    if args.device_id:
        DEVICE_ID = args.device_id
    if args.traccar_url:
        TRACCAR_URL = args.traccar_url

# Configure stdout formatting based on verbosity
if verbose_mode > 0:
    print(f"Verbose mode: level {verbose_mode}")

# Calculate frequency register values
frf = int((FREQUENCY * FRF_FACTOR) / FXTAL)
FRF_MSB = (frf >> 16) & 0xFF
FRF_MID = (frf >> 8) & 0xFF
FRF_LSB = frf & 0xFF

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
    if verbose_mode >= 3:
        print(f"SPI WRITE: [{addr | 0x80:02X}] <- {val:02X}")
    
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, val])
    GPIO.output(NSS, GPIO.HIGH)

def spi_read(addr):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr & 0x7F])
    val = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    
    if verbose_mode >= 3:
        print(f"SPI READ: [{addr & 0x7F:02X}] -> {val:02X}")
    
    return val

def reset_module():
    if verbose_mode >= 1:
        print("Resetting LoRa module...")
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def set_frequency():
    if verbose_mode >= 1:
        print(f"Setting frequency to {FREQUENCY/1000000} MHz")
    spi_write(0x06, FRF_MSB)  # RegFrfMsb
    spi_write(0x07, FRF_MID)  # RegFrfMid
    spi_write(0x08, FRF_LSB)  # RegFrfLsb

def init_lora():
    reset_module()
    version = spi_read(0x42)  # RegVersion
    
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    # LoRa sleep mode
    spi_write(0x01, 0x80)
    time.sleep(0.1)

    # Set fixed frequency (915 MHz by default)
    set_frequency()

    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header
    # 0111 001 0 = 0x72 for explicit header
    spi_write(0x1D, 0x72)
    
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
    
    if verbose_mode >= 1:
        print(f"ModemConfig1: 0x{spi_read(0x1D):02X}")
        print(f"ModemConfig2: 0x{spi_read(0x1E):02X}")
        print(f"ModemConfig3: 0x{spi_read(0x26):02X}")
    
    print(f"LoRa module initialized. Listening on {FREQUENCY/1000000} MHz")
    
    if verbose_mode >= 1:
        print(f"Traccar URL: {TRACCAR_URL}")
        print(f"Device ID: {DEVICE_ID}")

def format_hex_dump(data):
    """Format a byte array as a hexdump with ASCII representation"""
    result = []
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_values = ' '.join(f'{b:02X}' for b in chunk)
        ascii_values = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
        result.append(f"{i:04X}: {hex_values:<48} | {ascii_values}")
    return '\n'.join(result)

def send_to_traccar(latitude, longitude, altitude, timestamp):
    """Send GPS data to Traccar server using the OSMAnd protocol."""
    if args and args.no_traccar:
        if verbose_mode >= 1:
            print("Traccar integration disabled, not sending data")
        return
        
    params = {
        "id": DEVICE_ID,
        "lat": latitude,
        "lon": longitude,
        "altitude": altitude,
        "timestamp": timestamp
    }
    
    if verbose_mode >= 1:
        print(f"Sending data to Traccar: {params}")
    
    try:
        response = requests.get(TRACCAR_URL, params=params, timeout=10)
        print(f"Traccar response status: {response.status_code}")
        
        if verbose_mode >= 2:
            print(f"Traccar response content: {response.text}")
        
        if response.status_code == 200:
            print("Data successfully sent to Traccar server")
        else:
            print(f"Failed to send data to Traccar. Status code: {response.status_code}")
    except requests.RequestException as e:
        print(f"Error sending data to Traccar: {e}")

def receive_loop():
    packet_count = 0
    last_status_time = time.time()
    
    print(f"Listening for incoming LoRa packets on {FREQUENCY/1000000} MHz...")
    
    while True:
        current_time = time.time()
        
        # Print status every minute if no packets received and verbose mode
        if verbose_mode >= 1 and current_time - last_status_time > 60:
            print(f"Still listening... ({packet_count} packets received so far)")
            last_status_time = current_time
        
        # Check IRQ flags
        irq_flags = spi_read(0x12)
        
        # Check for RX Done (bit 6)
        if irq_flags & 0x40:
            last_status_time = current_time
            packet_count += 1
            
            print(f"Packet #{packet_count} received!")
            if verbose_mode >= 2:
                print(f"IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
            
            # Clear IRQ flags
            spi_write(0x12, 0xFF)
            
            # Read payload length
            nb_bytes = spi_read(0x13)
            print(f"Packet size: {nb_bytes} bytes")
            
            # Read current FIFO address
            current_addr = spi_read(0x10)
            if verbose_mode >= 2:
                print(f"FIFO RX current address: 0x{current_addr:02X}")
            
            # Set FIFO address pointer
            spi_write(0x0D, current_addr)
            
            # Read payload
            payload = bytearray()
            for _ in range(nb_bytes):
                payload.append(spi_read(0x00))

            print(f"Raw RX Payload ({len(payload)} bytes): {payload.hex()}")
            if verbose_mode >= 2:
                print("Hexdump of payload:")
                print(format_hex_dump(payload))
            
            # Check RSSI and SNR
            if verbose_mode >= 1:
                # Read after packet reception
                rssi = spi_read(0x1A) - 137  # Convert to dBm (see datasheet)
                snr = spi_read(0x19)
                # Convert SNR to decimal value (signed 8-bit)
                if snr > 127:
                    snr = (snr - 256) / 4
                else:
                    snr = snr / 4
                
                print(f"Signal quality: RSSI={rssi} dBm, SNR={snr:.1f} dB")
            
            # Decode payload if 14 bytes (expected format)
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
        
        # Small delay to prevent CPU hogging
        time.sleep(0.01)

def cleanup():
    if verbose_mode >= 1:
        print("Cleaning up...")
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
    except Exception as e:
        print(f"Unhandled exception: {e}")
        cleanup()
        sys.exit(1)