import serial
import pynmea2
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import argparse

# GPS Serial Port Configuration
GPS_PORT = "/dev/ttyAMA0"
BAUD_RATE = 9600

# Pin configuration (BCM numbering)
RESET = 17   # Reset pin
NSS   = 25   # SPI Chip Select pin
DIO0  = 4    # DIO0 pin for interrupts

# Global variables
last_tx_time = 0
TX_INTERVAL = 10  # Seconds between transmissions

# Default LoRa parameters (can be changed via command line)
FREQUENCY = 915000000  # 915 MHz default
SPREADING_FACTOR = 12  # SF12 default
BANDWIDTH = 125        # 125 kHz default
CODING_RATE = 1        # 4/5 default
PREAMBLE_LENGTH = 8    # 8 symbols default
IMPLICIT_HEADER = False # Explicit header by default
TX_POWER = 20          # 20 dBm default
SYNC_WORD = 0x12       # Default sync word

# FXTAL is fixed for the SX127x series
FXTAL = 32000000
FRF_FACTOR = 2**19

# Parse command-line arguments
def parse_arguments():
    parser = argparse.ArgumentParser(description='LoRa GPS transmitter with configurable parameters')
    
    # Radio configuration
    parser.add_argument('--freq', type=float, default=915.0,
                        help='Frequency in MHz (default: 915.0)')
    parser.add_argument('--sf', type=int, choices=range(7, 13), default=12,
                        help='Spreading Factor (7-12, default: 12)')
    parser.add_argument('--bw', type=float, choices=[62.5, 125, 250, 500], default=125,
                        help='Bandwidth in kHz (62.5, 125, 250, or 500, default: 125)')
    parser.add_argument('--cr', type=int, choices=range(1, 5), default=1,
                        help='Coding Rate (1=4/5, 2=4/6, 3=4/7, 4=4/8, default: 1)')
    parser.add_argument('--preamble', type=int, default=8,
                        help='Preamble length in symbols (default: 8)')
    parser.add_argument('--implicit', action='store_true',
                        help='Use implicit header mode instead of explicit')
    parser.add_argument('--power', type=int, choices=range(5, 21), default=20,
                        help='TX power in dBm (5-20, default: 20)')
    parser.add_argument('--sync-word', type=int, default=0x12,
                        help='Sync word value (default: 0x12)')
    parser.add_argument('--interval', type=int, default=10,
                        help='Transmission interval in seconds (default: 10)')
    parser.add_argument('--test', action='store_true',
                        help='Enable test mode: send simple test packets instead of GPS data')

    
    # GPS configuration
    parser.add_argument('--gps-port', type=str, default="/dev/ttyAMA0",
                        help='GPS serial port (default: /dev/ttyAMA0)')
    parser.add_argument('--baud-rate', type=int, default=9600,
                        help='GPS baud rate (default: 9600)')
    
    # Debugging options
    parser.add_argument('--verbose', '-v', action='count', default=0,
                        help='Increase verbosity (use -v, -vv, or -vvv)')
    
    return parser.parse_args()


# Setup GPIO in BCM mode
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(RESET, GPIO.OUT)
GPIO.setup(NSS, GPIO.OUT)
GPIO.setup(DIO0, GPIO.IN)

# SPI initialization
spi = spidev.SpiDev()
spi.open(0, 0)
spi.max_speed_hz = 5000000
spi.mode = 0b00

# Verbose mode for debugging
verbose_mode = 0


def send_test_packets():
    """Send simple test packets with increasing counter"""
    counter = 0
    print("Test mode enabled. Sending test packets...")
    
    while True:
        # Create a simple test payload with counter and timestamp
        timestamp = int(time.time())
        test_payload = struct.pack(">BI", counter, timestamp)
        
        print(f"Sending test packet #{counter} at {time.strftime('%H:%M:%S')}")
        spi_tx(test_payload)
        
        counter = (counter + 1) % 256  # Keep counter in byte range
        
        # Wait for the next transmission interval
        time.sleep(TX_INTERVAL)

def cleanup():
    if verbose_mode >= 1:
        print("Cleaning up...")
    spi.close()
    GPIO.cleanup()

def spi_write(addr, value):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, value])
    GPIO.output(NSS, GPIO.HIGH)
    
    if verbose_mode >= 3:
        print(f"SPI WRITE: [{addr | 0x80:02X}] <- {value:02X}")

def spi_read(addr):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr & 0x7F])
    result = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    
    if verbose_mode >= 3:
        print(f"SPI READ: [{addr & 0x7F:02X}] -> {result:02X}")
    
    return result

def reset_module():
    if verbose_mode >= 1:
        print("Resetting LoRa module...")
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def set_frequency():
    # Calculate frequency register values based on current FREQUENCY setting
    frf = int((FREQUENCY * FRF_FACTOR) / FXTAL)
    FRF_MSB = (frf >> 16) & 0xFF
    FRF_MID = (frf >> 8) & 0xFF
    FRF_LSB = frf & 0xFF
    
    if verbose_mode >= 1:
        print(f"Setting frequency to {FREQUENCY/1000000} MHz")
    
    spi_write(0x06, FRF_MSB)  # RegFrfMsb
    spi_write(0x07, FRF_MID)  # RegFrfMid
    spi_write(0x08, FRF_LSB)  # RegFrfLsb

def get_bandwidth_value():
    """Convert bandwidth in kHz to register value"""
    bw_values = {
        62.5: 0x60,  # Added 62.5 kHz option
        125: 0x70,
        250: 0x80,
        500: 0x90
    }
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

def get_power_config():
    # For PA_BOOST pin (used on most modules)
    if TX_POWER > 17:
        # High power mode with PA_BOOST (for +20 dBm)
        return 0x8F | ((min(TX_POWER, 20) - 5) & 0x0F)
    else:
        # Normal power mode with PA_BOOST
        return 0x80 | ((min(TX_POWER, 17) - 2) & 0x0F)

def should_enable_ldro():
    """Determine if Low Data Rate Optimization should be enabled based on SF and BW."""
    # For LoRa, LDRO should be enabled if symbol duration exceeds 16ms
    # Symbol duration (seconds) = 2^SF / BW (Hz)
    
    # Convert bandwidth from kHz to Hz
    bw_hz = BANDWIDTH * 1000
    
    # Calculate symbol duration in milliseconds
    symbol_duration_ms = (2**SPREADING_FACTOR) / bw_hz * 1000
    
    # Calculate threshold based on datasheet (enable if > 16ms)
    should_enable = symbol_duration_ms > 16.0
    
    if verbose_mode >= 1:
        print(f"Symbol duration: {symbol_duration_ms:.3f}ms (threshold: 16.0ms)")
        print(f"LDRO should be: {'ENABLED' if should_enable else 'DISABLED'}")
    
    return should_enable

def init_module():
    reset_module()
    version = spi_read(0x42)  # RegVersion
    
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected! Check wiring and power.")
        cleanup()
        sys.exit(1)
    
    # Put module in Sleep mode with LoRa enabled
    spi_write(0x01, 0x80)
    time.sleep(0.1)
    
    # Map DIO0 to TxDone (bit 7:6 = 01)
    spi_write(0x40, 0x40)  # DIO0 = TxDone
    
    # Set frequency
    set_frequency()

    # Set Sync Word
    spi_write(0x39, SYNC_WORD)
    
    # RegModemConfig1: BW, CR, and header mode
    modem_config1 = get_modem_confldro sx1276ig1()
    spi_write(0x1D, modem_config1)
    
    # RegModemConfig2: SF and CRC
    modem_config2 = get_spreading_factor_value()
    spi_write(0x1E, modem_config2)
    
    # RegModemConfig3: Set LDRO dynamically based on SF and BW
    ldro_enabled = should_enable_ldro()
    if ldro_enabled:
        modem_config3 = 0x0C  # LDRO enabled (0x08) + AGC enabled (0x04)
    else:
        modem_config3 = 0x04  # LDRO disabled (0x00) + AGC enabled (0x04)
    spi_write(0x26, modem_config3)
    
    # Set preamble length
    spi_write(0x20, (PREAMBLE_LENGTH >> 8) & 0xFF)  # MSB
    spi_write(0x21, PREAMBLE_LENGTH & 0xFF)         # LSB
    
    # Set power configuration
    power_config = get_power_config()
    spi_write(0x09, power_config)
    
    # Set FIFO TX base address and pointer
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)
    
    # Put module in standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)
    
    # Print configuration summary
    print(f"LoRa module initialized with:")
    print(f"  Frequency: {FREQUENCY/1000000} MHz")
    print(f"  Spreading Factor: SF{SPREADING_FACTOR}")
    print(f"  Bandwidth: {BANDWIDTH} kHz")
    print(f"  Coding Rate: 4/{4+CODING_RATE}")
    print(f"  Preamble Length: {PREAMBLE_LENGTH} symbols")
    print(f"  Header Mode: {'Implicit' if IMPLICIT_HEADER else 'Explicit'}")
    print(f"  TX Power: {TX_POWER} dBm")
    print(f"  TX Interval: {TX_INTERVAL} seconds")
    
    if verbose_mode >= 1:
        print(f"  ModemConfig1: 0x{spi_read(0x1D):02X}")
        print(f"  ModemConfig2: 0x{spi_read(0x1E):02X}")
        print(f"  ModemConfig3: 0x{spi_read(0x26):02X}")
        print(f"  Sync Word: 0x{spi_read(0x39):02X}")
        print(f"  Power Config: 0x{spi_read(0x09):02X}")

def spi_tx(payload):
    if verbose_mode >= 1:
        print(f"Raw TX Payload ({len(payload)} bytes): {payload.hex()}")

    # Reset FIFO pointer
    spi_write(0x0D, 0x00)
    
    # Write binary payload into FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    spi_write(0x22, len(payload))  # Set payload length
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Switch to TX mode
    spi_write(0x01, 0x83)
    print(f"Transmitting {len(payload)} bytes on {FREQUENCY/1000000} MHz.")

    # Wait for transmission to complete
    start = time.time()
    while time.time() - start < 5:  # Timeout after 5 seconds
        # Check for TX Done (bit 3 in RegIrqFlags)
        irq_flags = spi_read(0x12)
        if irq_flags & 0x08:  # TxDone flag
            print("Transmission complete!")
            break   
        
        time.sleep(0.01)
    
    if time.time() - start >= 5:
        print("TX timeout. IRQ flags:", hex(spi_read(0x12)))
    
    # Clear all IRQ flags
    spi_write(0x12, 0xFF)
    
    # Return to standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def parse_gps(data):
    global last_tx_time
    
    current_time = time.time()
    if last_tx_time > 0 and (current_time - last_tx_time) < TX_INTERVAL:
        return
    
    if data.startswith("$GNGGA") or data.startswith("$GPGGA"):
        try:
            msg = pynmea2.parse(data)
            
            # Only transmit if we have a valid GPS fix
            if msg.gps_qual > 0:  # 0 = no fix
                lat = int(msg.latitude * 1_000_000)
                lon = int(msg.longitude * 1_000_000)
                alt = int(float(msg.altitude) if msg.altitude else 0)
                timestamp = int(time.time())
                payload = struct.pack(">iiHI", lat, lon, alt, timestamp)
                
                print(f"TX: lat={msg.latitude}, lon={msg.longitude}, alt={alt}m, ts={timestamp}")
                spi_tx(payload)
                last_tx_time = time.time()
                print(f"Next transmission in {TX_INTERVAL} seconds")
            elif verbose_mode >= 1:
                print("Waiting for valid GPS fix...")
        
        except pynmea2.ParseError as e:
            if verbose_mode >= 2:
                print(f"Parse error: {e}")
        except Exception as e:
            print(f"Error in parse_gps: {e}")
            # Only reinitialize on serious errors
            if str(e).lower().find("spi") >= 0 or str(e).lower().find("gpio") >= 0:
                print("SPI or GPIO error detected, reinitializing module...")
                init_module()

def read_gps():
    try:
        with serial.Serial(GPS_PORT, BAUD_RATE, timeout=1) as ser:
            print(f"Reading GPS data from {GPS_PORT} at {BAUD_RATE} baud...")
            while True:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    if verbose_mode >= 3:
                        print(f"GPS: {line}")
                    parse_gps(line)
                time.sleep(0.01)
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        cleanup()
        sys.exit(1)

def main():
    global FREQUENCY, SPREADING_FACTOR, BANDWIDTH, CODING_RATE, PREAMBLE_LENGTH
    global IMPLICIT_HEADER, TX_POWER, TX_INTERVAL, GPS_PORT, BAUD_RATE, SYNC_WORD
    global verbose_mode
    
    # Parse command line arguments
    args = parse_arguments()
    
    # Apply settings from command line
    verbose_mode = args.verbose
    FREQUENCY = int(args.freq * 1000000)  # Convert MHz to Hz
    SPREADING_FACTOR = args.sf
    BANDWIDTH = args.bw
    CODING_RATE = args.cr
    PREAMBLE_LENGTH = args.preamble
    IMPLICIT_HEADER = args.implicit
    TX_POWER = args.power
    SYNC_WORD = args.sync_word
    TX_INTERVAL = args.interval
    GPS_PORT = args.gps_port
    BAUD_RATE = args.baud_rate
    
    # Print verbose mode
    if verbose_mode > 0:
        print(f"Verbose mode: level {verbose_mode}")
    
    # Initialize LoRa module
    init_module()
    
    try:
        if args.test:
            send_test_packets()  # Use test mode if requested
        else:
            read_gps()  # Normal GPS mode
    except KeyboardInterrupt:
        print("Interrupted by user. Exiting.")
        cleanup()
        sys.exit(0)

if __name__ == "__main__":
    main()