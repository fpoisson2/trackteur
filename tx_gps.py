import serial
import pynmea2
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct

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

# Fixed frequency configuration
FREQUENCY = 915000000  # 915 MHz
FXTAL = 32000000
FRF_FACTOR = 2**19

# Calculate frequency register values
frf = int((FREQUENCY * FRF_FACTOR) / FXTAL)
FRF_MSB = (frf >> 16) & 0xFF
FRF_MID = (frf >> 8) & 0xFF
FRF_LSB = frf & 0xFF

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

def cleanup():
    spi.close()
    GPIO.cleanup()

def spi_write(addr, value):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, value])
    GPIO.output(NSS, GPIO.HIGH)

def spi_read(addr):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr & 0x7F])
    result = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    return result

def reset_module():
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def set_frequency():
    spi_write(0x06, FRF_MSB)  # RegFrfMsb
    spi_write(0x07, FRF_MID)  # RegFrfMid
    spi_write(0x08, FRF_LSB)  # RegFrfLsb

def init_module():
    reset_module()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected! Check wiring and power.")
        cleanup()
        sys.exit(1)
    
    # Put module in Sleep mode with LoRa enabled
    spi_write(0x01, 0x80)
    time.sleep(0.1)
    
    # Map DIO0 to TxDone (bit 7:6 = 00)
    spi_write(0x40, 0x40)  # DIO0 = TxDone
    
    # Set fixed frequency (915 MHz)
    set_frequency()
    
    # RegModemConfig1: BW 125 kHz, CR 4/5, Implicit Header
    # Bits 7-4: Bandwidth (0111 = 125 kHz)
    # Bits 3-1: Coding Rate (001 = 4/5)
    # Bit 0: Implicit Header Mode (0 = Explicit Header Mode, 1 = Implicit Header Mode)
    # 0111 001 0 = 0x72 for explicit header, 0x73 for implicit header
    spi_write(0x1D, 0x73)
    
    # RegModemConfig2: SF12, CRC on
    # 1100 1 1 00 = 0xC4
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LDRO on, AGC on
    # 0000 1 1 00 = 0x0C
    spi_write(0x26, 0x0C)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Set PA configuration: +20 dBm with PA_BOOST
    spi_write(0x09, 0x8F)
    
    # Set FIFO TX base address and pointer
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)
    
    # Put module in standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def spi_tx(payload):
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
            lat = int(msg.latitude * 1_000_000)
            lon = int(msg.longitude * 1_000_000)
            alt = int(msg.altitude)
            timestamp = int(time.time())
            payload = struct.pack(">iiHI", lat, lon, alt, timestamp)
            
            print(f"TX: lat={lat/1_000_000}, lon={lon/1_000_000}, alt={alt}m, ts={timestamp}")
            spi_tx(payload)
            last_tx_time = time.time()
            print(f"Next transmission in {TX_INTERVAL} seconds")
        
        except pynmea2.ParseError as e:
            print(f"Parse error: {e}")
        except Exception as e:
            print(f"Error in parse_gps: {e}")
            init_module()

def read_gps():
    try:
        with serial.Serial(GPS_PORT, BAUD_RATE, timeout=1) as ser:
            print("Reading GPS data...")
            while True:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    parse_gps(line)
                time.sleep(0.01)
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        cleanup()
        sys.exit(1)

def main():
    init_module()
    try:
        read_gps()
    except KeyboardInterrupt:
        print("Interrupted by user. Exiting.")
        cleanup()
        sys.exit(0)

if __name__ == "__main__":
    main()