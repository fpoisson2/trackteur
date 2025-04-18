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

# Frequency hopping configuration
FREQ_START = 902200000  # 902.2 MHz
FREQ_STEP = 400000      # 400 kHz
FXTAL = 32000000
FRF_FACTOR = 2**19

HOP_CHANNELS = []
for i in range(64):
    freq = FREQ_START + i * FREQ_STEP
    frf = int((freq * FRF_FACTOR) / FXTAL)
    msb = (frf >> 16) & 0xFF
    mid = (frf >> 8) & 0xFF
    lsb = frf & 0xFF
    HOP_CHANNELS.append((msb, mid, lsb))

# Replace the HOP_CHANNELS list in the main code with this generated list
current_channel = 0

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

def set_frequency(channel_idx):
    global HOP_CHANNELS
    msb, mid, lsb = HOP_CHANNELS[channel_idx]
    spi_write(0x06, msb)  # RegFrfMsb
    spi_write(0x07, mid)  # RegFrfMid
    spi_write(0x08, lsb)  # RegFrfLsb

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
    
    # Map DIO0 to FhssChangeChannel (bit 7:6 = 01)
    spi_write(0x40, 0x40)  # DIO0 = FhssChangeChannel
    
    # Set initial frequency to channel 0 (902.2 MHz)
    set_frequency(0)
    
    # RegModemConfig1: BW 125 kHz, CR 4/8, Implicit Header
    # 0110 100 0 = 0x68
    spi_write(0x1D, 0x78)
    
    # RegModemConfig2: SF12, CRC on
    # 1100 1 1 00 = 0xC4
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LDRO on, AGC on
    # 0000 1 1 00 = 0x0C
    spi_write(0x26, 0x0C)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Enable frequency hopping: Hop every 5 symbols (~327.7 ms at 62.5 kHz)
    spi_write(0x24, 5)  # RegHopPeriod
    
    # Set PA configuration: +20 dBm with PA_BOOST
    spi_write(0x09, 0x8F)
    
    # Set FIFO TX base address and pointer
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)
    
    # Put module in standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def spi_tx(payload):
    global current_channel
    print(f"Raw TX Payload ({len(payload)} bytes): {payload.hex()}")

    # Reset FIFO pointer
    spi_write(0x0D, 0x00)
    
    # Write binary payload into FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    spi_write(0x22, len(payload))  # Set payload length
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Set initial channel
    current_channel = 0
    set_frequency(current_channel)
    
    # Switch to TX mode
    spi_write(0x01, 0x83)
    print(f"Transmitting {len(payload)} bytes with frequency hopping.")

    # Give a moment for transmission to start
    time.sleep(0.1)
    
    # Remap DIO0 to TxDone (bit 7:6 = 00)
    spi_write(0x40, 0x00)

    # Handle frequency hopping during transmission
    start = time.time()
    while time.time() - start < 5:  # Timeout after 5 seconds
        if GPIO.input(DIO0) == 1:  # FhssChangeChannel interrupt
            # Read current channel and IRQ flags
            hop_channel = spi_read(0x1C)
            current_channel = hop_channel & 0x3F
            print(f"FHSS interrupt: Current channel {current_channel}")
            
            # Update to next channel
            current_channel = (current_channel + 1) % len(HOP_CHANNELS)
            set_frequency(current_channel)
            
            # Clear FhssChangeChannel interrupt
            spi_write(0x12, 0x04)
        
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