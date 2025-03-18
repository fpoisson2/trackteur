#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import requests
import argparse
import logging
from datetime import datetime

# Parse command-line arguments
def parse_arguments():
    parser = argparse.ArgumentParser(description='LoRa receiver for GPS data with Traccar integration')
    
    # Radio configuration
    parser.add_argument('--freq', type=float, default=915.0,
                        help='Frequency in MHz (default: 915.0)')
    parser.add_argument('--bw', type=int, choices=[125, 250, 500], default=125,
                        help='Bandwidth in kHz (default: 125)')
    parser.add_argument('--sf', type=int, choices=range(7, 13), default=12,
                        help='Spreading Factor (7-12, default: 12)')
    parser.add_argument('--cr', type=int, choices=range(1, 5), default=1,
                        help='Coding Rate (1=4/5, 2=4/6, 3=4/7, 4=4/8, default: 1)')
    parser.add_argument('--preamble', type=int, default=8,
                        help='Preamble length in symbols (default: 8)')
    parser.add_argument('--implicit', action='store_true',
                        help='Use implicit header mode')
    
    # Traccar configuration
    parser.add_argument('--traccar', type=str, default="http://trackteur.ve2fpd.com:5055",
                        help='Traccar server URL (default: http://trackteur.ve2fpd.com:5055)')
    parser.add_argument('--device-id', type=str, default="212901",
                        help='Device ID for Traccar (default: 212901)')
    
    # Debugging options
    parser.add_argument('--verbose', '-v', action='count', default=0,
                        help='Increase verbosity (can be used multiple times)')
    parser.add_argument('--no-traccar', action='store_true',
                        help='Disable sending data to Traccar server')
    parser.add_argument('--hex-dump', action='store_true',
                        help='Print hexdump of all SPI transactions')
    
    # GPIO pin configuration
    parser.add_argument('--reset-pin', type=int, default=17,
                        help='GPIO pin for RESET (default: 17)')
    parser.add_argument('--nss-pin', type=int, default=25,
                        help='GPIO pin for NSS/CS (default: 25)')
    parser.add_argument('--dio0-pin', type=int, default=4,
                        help='GPIO pin for DIO0 (default: 4)')
    
    return parser.parse_args()

# Global variables
args = parse_arguments()

# Configure logging based on verbosity
log_levels = {
    0: logging.WARNING,
    1: logging.INFO,
    2: logging.DEBUG
}
log_level = log_levels.get(args.verbose, logging.DEBUG)
logging.basicConfig(
    level=log_level,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# Pin definitions (BCM mode)
RESET = args.reset_pin
NSS = args.nss_pin
DIO0 = args.dio0_pin

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

# Frequency configuration
FREQUENCY = int(args.freq * 1000000)  # Convert MHz to Hz
FXTAL = 32000000
FRF_FACTOR = 2**19

# Calculate frequency register values
frf = int((FREQUENCY * FRF_FACTOR) / FXTAL)
FRF_MSB = (frf >> 16) & 0xFF
FRF_MID = (frf >> 8) & 0xFF
FRF_LSB = frf & 0xFF

# Traccar server configuration
TRACCAR_URL = args.traccar
DEVICE_ID = args.device_id

# Register addresses (for better readability)
REG_FIFO = 0x00
REG_OP_MODE = 0x01
REG_FRF_MSB = 0x06
REG_FRF_MID = 0x07
REG_FRF_LSB = 0x08
REG_IRQ_FLAGS = 0x12
REG_RX_NB_BYTES = 0x13
REG_FIFO_ADDR_PTR = 0x0D
REG_FIFO_RX_BASE_ADDR = 0x0F
REG_FIFO_RX_CURRENT_ADDR = 0x10
REG_MODEM_CONFIG1 = 0x1D
REG_MODEM_CONFIG2 = 0x1E
REG_PREAMBLE_MSB = 0x20
REG_PREAMBLE_LSB = 0x21
REG_MODEM_CONFIG3 = 0x26
REG_DIO_MAPPING1 = 0x40
REG_VERSION = 0x42

def spi_write(addr, val):
    if args.hex_dump:
        logger.debug(f"SPI WRITE: [{addr | 0x80:02X}] <- {val:02X}")
    
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, val])
    GPIO.output(NSS, GPIO.HIGH)

def spi_read(addr):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr & 0x7F])
    val = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    
    if args.hex_dump:
        logger.debug(f"SPI READ: [{addr & 0x7F:02X}] -> {val:02X}")
    
    return val

def reset_module():
    logger.info("Resetting LoRa module...")
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def set_frequency():
    logger.info(f"Setting frequency to {FREQUENCY/1000000} MHz (FRF: 0x{FRF_MSB:02X}{FRF_MID:02X}{FRF_LSB:02X})")
    spi_write(REG_FRF_MSB, FRF_MSB)
    spi_write(REG_FRF_MID, FRF_MID)
    spi_write(REG_FRF_LSB, FRF_LSB)

def get_bandwidth_setting():
    bw_values = {125: 0x70, 250: 0x80, 500: 0x90}
    return bw_values.get(args.bw, 0x70)  # Default to 125kHz

def get_coding_rate_setting():
    cr_values = {1: 0x02, 2: 0x04, 3: 0x06, 4: 0x08}
    return cr_values.get(args.cr, 0x02)  # Default to 4/5

def get_spreading_factor_setting():
    return (args.sf << 4) | 0x04  # 0x04 sets CRC on

def build_modem_config1():
    # Combine bandwidth and coding rate settings
    config = get_bandwidth_setting() | get_coding_rate_setting()
    
    # Add implicit header bit if needed
    if args.implicit:
        config |= 0x01
    
    return config

def init_lora():
    reset_module()
    version = spi_read(REG_VERSION)
    
    logger.info(f"SX1276 Version: 0x{version:02X}")
    if version != 0x12:
        logger.error("Module not detected or wrong version, check connections")
        cleanup()
        sys.exit(1)

    # Enter sleep mode
    logger.info("Setting module to LoRa sleep mode")
    spi_write(REG_OP_MODE, 0x80)
    time.sleep(0.1)

    # Set frequency
    set_frequency()

    # Configure modem - Bandwidth, Coding Rate, Header Mode
    modem_config1 = build_modem_config1()
    logger.info(f"Setting ModemConfig1: 0x{modem_config1:02X}")
    spi_write(REG_MODEM_CONFIG1, modem_config1)
    
    # Configure modem - Spreading Factor, CRC
    modem_config2 = get_spreading_factor_setting()
    logger.info(f"Setting ModemConfig2: 0x{modem_config2:02X}")
    spi_write(REG_MODEM_CONFIG2, modem_config2)
    
    # Configure modem - LDRO, AGC
    logger.info("Setting ModemConfig3: 0x0C (LDRO and AGC enabled)")
    spi_write(REG_MODEM_CONFIG3, 0x0C)

    # Set preamble length
    logger.info(f"Setting preamble length to {args.preamble} symbols")
    spi_write(REG_PREAMBLE_MSB, (args.preamble >> 8) & 0xFF)
    spi_write(REG_PREAMBLE_LSB, args.preamble & 0xFF)

    # Map DIO0 to RxDone (bit 7:6 = 00)
    logger.info("Setting DIO0 to RxDone")
    spi_write(REG_DIO_MAPPING1, 0x00)

    # Set FIFO RX base address and pointer
    logger.info("Setting FIFO RX base address to 0x00")
    spi_write(REG_FIFO_RX_BASE_ADDR, 0x00)
    spi_write(REG_FIFO_ADDR_PTR, 0x00)

    # Enter continuous RX mode
    logger.info("Entering continuous RX mode")
    spi_write(REG_OP_MODE, 0x85)
    
    # Print configuration summary
    logger.info(f"LoRa module initialized with:")
    logger.info(f"  Frequency: {FREQUENCY/1000000} MHz")
    logger.info(f"  Bandwidth: {args.bw} kHz")
    logger.info(f"  Spreading Factor: SF{args.sf}")
    logger.info(f"  Coding Rate: 4/{4+args.cr}")
    logger.info(f"  Preamble Length: {args.preamble} symbols")
    logger.info(f"  Header Mode: {'Implicit' if args.implicit else 'Explicit'}")
    
    if not args.no_traccar:
        logger.info(f"Traccar server: {TRACCAR_URL}")
        logger.info(f"Device ID: {DEVICE_ID}")
    else:
        logger.info("Traccar integration disabled")

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
    if args.no_traccar:
        logger.info("Traccar integration disabled, not sending data")
        return
        
    params = {
        "id": DEVICE_ID,
        "lat": latitude,
        "lon": longitude,
        "altitude": altitude,
        "timestamp": timestamp
    }
    
    logger.info(f"Sending data to Traccar: {params}")
    
    try:
        response = requests.get(TRACCAR_URL, params=params, timeout=10)
        logger.info(f"Traccar response status: {response.status_code}")
        logger.debug(f"Traccar response content: {response.text}")
        
        if response.status_code == 200:
            logger.info("Data successfully sent to Traccar server")
        else:
            logger.warning(f"Failed to send data to Traccar. Status code: {response.status_code}")
    except requests.RequestException as e:
        logger.error(f"Error sending data to Traccar: {e}")

def receive_loop():
    packet_count = 0
    last_status_time = time.time()
    
    logger.info(f"Listening for incoming LoRa packets on {FREQUENCY/1000000} MHz...")
    
    while True:
        current_time = time.time()
        
        # Print status every minute if no packets received
        if current_time - last_status_time > 60:
            logger.info(f"Still listening... ({packet_count} packets received so far)")
            last_status_time = current_time
        
        # Check IRQ flags
        irq_flags = spi_read(REG_IRQ_FLAGS)
        
        # Check for RX Done (bit 6)
        if irq_flags & 0x40:
            last_status_time = current_time
            packet_count += 1
            rx_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            
            logger.info(f"[{rx_time}] Packet #{packet_count} received!")
            logger.debug(f"IRQ Flags: 0b{irq_flags:08b} (0x{irq_flags:02X})")
            
            # Clear IRQ flags
            spi_write(REG_IRQ_FLAGS, 0xFF)
            
            # Read payload length
            nb_bytes = spi_read(REG_RX_NB_BYTES)
            logger.info(f"Packet size: {nb_bytes} bytes")
            
            # Read current FIFO address
            current_addr = spi_read(REG_FIFO_RX_CURRENT_ADDR)
            logger.debug(f"FIFO RX current address: 0x{current_addr:02X}")
            
            # Set FIFO address pointer
            spi_write(REG_FIFO_ADDR_PTR, current_addr)
            
            # Read payload
            payload = bytearray()
            for _ in range(nb_bytes):
                payload.append(spi_read(REG_FIFO))

            logger.info(f"Raw RX Payload ({len(payload)} bytes): {payload.hex()}")
            if args.verbose > 1:
                logger.debug("Hexdump of payload:")
                logger.debug(format_hex_dump(payload))
            
            # Check RSSI and SNR
            if args.verbose > 0:
                # Read after packet reception
                rssi = spi_read(0x1A) - 137  # Convert to dBm (see datasheet)
                snr = spi_read(0x19)
                # Convert SNR to decimal value (signed 8-bit)
                if snr > 127:
                    snr = (snr - 256) / 4
                else:
                    snr = snr / 4
                
                logger.info(f"Signal quality: RSSI={rssi} dBm, SNR={snr:.1f} dB")
            
            # Decode payload if 14 bytes (expected format)
            if len(payload) == 14:
                try:
                    lat, lon, alt, timestamp = struct.unpack(">iiHI", payload)
                    latitude = lat / 1_000_000.0
                    longitude = lon / 1_000_000.0
                    time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                    
                    logger.info(f"Decoded GPS: Lat={latitude:.6f}, Lon={longitude:.6f}, Alt={alt}m, Time={time_str}")
                    
                    # Send data to Traccar
                    send_to_traccar(latitude, longitude, alt, timestamp)
                except struct.error as e:
                    logger.error(f"Error decoding payload: {e}")
            else:
                logger.warning(f"Unexpected payload size: {nb_bytes} bytes (expected 14)")
                try:
                    message = ''.join(chr(b) for b in payload if 32 <= b <= 126)
                    logger.info(f"Raw data: {payload.hex()}")
                    logger.info(f"As text: {message}")
                except Exception as e:
                    logger.error(f"Error processing raw payload: {e}")
            
            # Check for CRC error
            if irq_flags & 0x20:
                logger.warning("CRC error detected in packet")
        
        # Small delay to prevent CPU hogging
        time.sleep(0.01)

def cleanup():
    logger.info("Cleaning up...")
    spi.close()
    GPIO.cleanup()
    logger.info("Shutdown complete.")

def display_intro():
    print("\n" + "="*80)
    print(" LoRa GPS Receiver and Traccar Integration")
    print(" Enhanced version with command-line parameters")
    print("="*80)
    
    if args.verbose == 0:
        print("\nTip: Use -v or --verbose for more detailed output")

if __name__ == "__main__":
    try:
        display_intro()
        init_lora()
        receive_loop()
    except KeyboardInterrupt:
        logger.info("\nReceived keyboard interrupt. Terminating...")
        cleanup()
        sys.exit(0)
    except Exception as e:
        logger.exception(f"Unhandled exception: {e}")
        cleanup()
        sys.exit(1)