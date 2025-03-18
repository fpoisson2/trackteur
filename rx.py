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
    parser.add_argument('--sf', type=int, choices=range(7, 13),
                        help='Spreading Factor (7-12, default: 12)')
    parser.add_argument('--bw', type=int, choices=[125, 250, 500],
                        help='Bandwidth in kHz (default: 125)')
    parser.add_argument('--cr', type=int, choices=range(1, 5),
                        help='Coding Rate (1=4/5, 2=4/6, 3=4/7, 4=4/8, default: 1)')
    parser.add_argument('--preamble', type=int,
                        help='Preamble length in symbols (default: 8)')
    parser.add_argument('--implicit', action='store_true',
                        help='Use implicit header mode instead of explicit')
    parser.add_argument('--sync-word', type=int,
                        help='Sync word value (default: 0x12)')
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
SPREADING_FACTOR = 12  # Default SF12
BANDWIDTH = 125        # Default 125 kHz
CODING_RATE = 1        # Default 4/5
PREAMBLE_LENGTH = 8    # Default 8 symbols
IMPLICIT_HEADER = False  # Default explicit header
SYNC_WORD = 0x12       # Default LoRa sync word

if args:
    verbose_mode = args.verbose
    if args.freq:
        FREQUENCY = int(args.freq * 1000000)  # Convert MHz to Hz
    if args.sf:
        SPREADING_FACTOR = args.sf
    if args.bw:
        BANDWIDTH = args.bw
    if args.cr:
        CODING_RATE = args.cr
    if args.preamble:
        PREAMBLE_LENGTH = args.preamble
    if args.implicit:
        IMPLICIT_HEADER = True
    if args.sync_word:
        SYNC_WORD = args.sync_word
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

def get_bandwidth_value():
    # Convert bandwidth in kHz to register value
    bw_values = {125: 0x70, 250: 0x80, 500: 0x90}
    return bw_values.get(BANDWIDTH, 0x70)  # Default to 125kHz

def get_coding_rate_value():
    # Convert coding rate index to register value
    cr_values = {1: 0x02, 2: 0x04, 3: 0x06, 4: 0x08}
    return cr_values.get(CODING_RATE, 0x02)  # Default to 4/5

def get_spreading_factor_value():
    # SF7 is 0x70, SF8 is 0x80, and so on
    return ((SPREADING_FACTOR & 0x0F) << 4) | 0x04  # 0x04 sets CRC on

def get_modem_config1():
    # Combine bandwidth and coding rate
    config = get_bandwidth_value() | get_coding_rate_value()
    
    # Add implicit header bit if needed
    if IMPLICIT_HEADER:
        config |= 0x01
        
    return config

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

    # Set fixed frequency
    set_frequency()

    # Set Sync Word (0x39 for LoRaWAN public, 0x12 default for private networks)
    spi_write(0x39, SYNC_WORD)

    # RegModemConfig1: BW, CR, and header mode
    modem_config1 = get_modem_config1()
    spi_write(0x1D, modem_config1)
    
    # RegModemConfig2: SF and CRC
    modem_config2 = get_spreading_factor_value()
    spi_write(0x1E, modem_config2)
    
    # RegModemConfig3: LDRO on, AGC on
    spi_write(0x26, 0x0C)

    # Preamble length
    spi_write(0x20, (PREAMBLE_LENGTH >> 8) & 0xFF)  # MSB
    spi_write(0x21, PREAMBLE_LENGTH & 0xFF)         # LSB

    # Map DIO0 to RxDone (bit 7:6 = 00)
    spi_write(0x40, 0x00)

    # Set FIFO RX base addr and pointer
    spi_write(0x0F, 0x00)
    spi_write(0x0D, 0x00)

    # Continuous RX mode
    spi_write(0x01, 0x85)
    
    # Print detailed configuration
    print(f"LoRa module initialized with:")
    print(f"  Frequency: {FREQUENCY/1000000} MHz")
    print(f"  Spreading Factor: SF{SPREADING_FACTOR}")
    print(f"  Bandwidth: {BANDWIDTH} kHz")
    print(f"  Coding Rate: 4/{4+CODING_RATE}")
    print(f"  Preamble Length: {PREAMBLE_LENGTH} symbols")
    print(f"  Header Mode: {'Implicit' if IMPLICIT_HEADER else 'Explicit'}")
    
    if verbose_mode >= 1:
        print(f"  ModemConfig1: 0x{spi_read(0x1D):02X}")
        print(f"  ModemConfig2: 0x{spi_read(0x1E):02X}")
        print(f"  ModemConfig3: 0x{spi_read(0x26):02X}")
        print(f"  Sync Word: 0x{spi_read(0x39):02X}")
        print(f"  Traccar URL: {TRACCAR_URL}")
        print(f"  Device ID: {DEVICE_ID}")
    
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
    last_verbose_print = time.time()
    
    print(f"Listening for incoming LoRa packets on {FREQUENCY/1000000} MHz...")
    
    while True:
        current_time = time.time()
        
        # Print status every minute if no packets received and verbose mode
        if verbose_mode >= 1 and current_time - last_status_time > 60:
            print(f"Still listening... ({packet_count} packets received so far)")
            last_status_time = current_time
        
        # Check IRQ flags
        irq_flags = spi_read(0x12)
        
        # Limit frequent debug output in high verbosity mode
        should_print_verbose = (current_time - last_verbose_print) > 0.1  # Max 10 prints per second
        
        # Check for RX Done (bit 6)
        if irq_flags & 0x40:
            last_status_time = current_time
            last_verbose_print = current_time
            packet_count += 1
            
            print(f"Packet #{packet_count} received!")
            if verbose_mode >= 2 and should_print_verbose:
                print(f"IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
            
            # Clear IRQ flags
            spi_write(0x12, 0xFF)
            
            # Read payload length
            nb_bytes = spi_read(0x13)
            print(f"Packet size: {nb_bytes} bytes")
            
            # Read current FIFO address
            current_addr = spi_read(0x10)
            if verbose_mode >= 2 and should_print_verbose:
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
        
        # Reduce CPU usage while maintaining responsiveness based on verbosity
        # Lower verbose mode = more responsive, higher verbose mode = less CPU usage
        sleep_time = 0.01 if verbose_mode < 2 else 0.005
        time.sleep(sleep_time)

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