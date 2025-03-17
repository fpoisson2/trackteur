import serial
import pynmea2
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import threading
import queue

# GPS Serial Port Configuration
GPS_PORT = "/dev/ttyAMA0"  # Confirmed UART for Dragino GPS HAT
BAUD_RATE = 9600           # Default baud rate for Dragino GPS

# --- Pin configuration (using BCM numbering) ---
RESET = 17   # Reset pin
NSS   = 25   # SPI Chip Select pin (manual control)
DIO0  = 4    # DIO0 pin; WiringPi pin 7 corresponds to BCM GPIO4

# Transmission interval (seconds)
TX_INTERVAL = 10

# Create a queue for storing GPS data
gps_queue = queue.Queue(maxsize=1)  # Only store the most recent data

# Setup GPIO in BCM mode
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(RESET, GPIO.OUT)
GPIO.setup(NSS, GPIO.OUT)
GPIO.setup(DIO0, GPIO.IN)

# --- SPI initialization ---
spi = spidev.SpiDev()
spi.open(0, 0)       # Bus 0, device 0
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

def init_module():
    reset_module()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected! Check wiring and power.")
        cleanup()
        sys.exit(1)
    
    # Put module in Sleep mode with LoRa enabled (RegOpMode)
    spi_write(0x01, 0x80)
    time.sleep(0.1)
    
    # Explicitly set DIO mapping: Map DIO0 to TX done.
    spi_write(0x40, 0x40)
    
    # Set frequency to 915 MHz (adjust if needed)
    frequency = 915000000
    frf = int(frequency / 61.03515625)
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb
    
    # Minimal modem configuration (example values)
    spi_write(0x1D, 0x72)  # RegModemConfig1: e.g., BW=125 kHz, CR=4/7, explicit header
    spi_write(0x1E, 0x74)  # RegModemConfig2: e.g., SF7, CRC on
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)  # RegPreambleMsb
    spi_write(0x21, 0x08)  # RegPreambleLsb
    
    # Set PA configuration (example value; ensure PA_BOOST is used if wired that way)
    spi_write(0x09, 0x8F)  # RegPaConfig
    
    # Set FIFO TX base address to 0 and reset FIFO pointer
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr

def spi_tx(payload):
    """Function to transmit binary payload using LoRa."""
    print(f"Raw TX Payload ({len(payload)} bytes): {payload.hex()}")  # Print raw hex data

    # Reset FIFO pointer
    spi_write(0x0D, 0x00)
    
    # Write binary payload into FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    spi_write(0x22, len(payload))  # Set payload length
    
    # Switch to TX mode: RegOpMode = 0x83 (LoRa TX mode)
    spi_write(0x01, 0x83)
    print(f"Transmitting {len(payload)} bytes.")

    start = time.time()
    while time.time() - start < 5:
        if GPIO.input(DIO0) == 1:
            print("TX done signal received!")
            break
        time.sleep(0.01)
    else:
        irq = spi_read(0x12)
        print("TX done timeout. IRQ flags:", hex(irq))

    # Return to standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def parse_gps(data):
    """Extracts latitude, longitude, altitude, and timestamp from GPS data."""
    if data.startswith("$GNGGA") or data.startswith("$GPGGA"):  # Look for valid GPS sentences
        try:
            msg = pynmea2.parse(data)
            
            # Convert to higher precision (6 decimal places)
            lat = int(msg.latitude * 1_000_000)   # Store latitude as int
            lon = int(msg.longitude * 1_000_000)  # Store longitude as int
            alt = int(msg.altitude)               # Store altitude in meters
            timestamp = int(time.time())          # Get current Unix timestamp (seconds)

            # Pack into a binary format: 4-byte lat, 4-byte lon, 2-byte alt, 4-byte timestamp
            payload = struct.pack(">iiHI", lat, lon, alt, timestamp)
            
            # Update the GPS data in the queue - replace old data if queue is full
            try:
                # Put without blocking, which will silently drop if queue is full
                gps_queue.put_nowait({
                    'lat': lat,
                    'lon': lon,
                    'alt': alt,
                    'timestamp': timestamp,
                    'payload': payload
                })
            except queue.Full:
                # Get the old item out
                gps_queue.get_nowait()
                # Put the new item in
                gps_queue.put({
                    'lat': lat,
                    'lon': lon,
                    'alt': alt,
                    'timestamp': timestamp,
                    'payload': payload
                })
            
            print(f"GPS: lat={lat/1_000_000}, lon={lon/1_000_000}, alt={alt}m, ts={timestamp}")

        except pynmea2.ParseError as e:
            print(f"Parse error: {e}")
        except Exception as e:
            print(f"Error in parse_gps: {e}")

def gps_thread_function():
    """Thread function to continuously read GPS data."""
    try:
        with serial.Serial(GPS_PORT, BAUD_RATE, timeout=1) as ser:
            print("GPS thread started. Reading GPS data...")
            while True:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    parse_gps(line)
                time.sleep(0.01)  # Small delay to prevent CPU hogging
    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except Exception as e:
        print(f"Error in GPS thread: {e}")

def tx_thread_function():
    """Thread function to transmit GPS data at regular intervals."""
    print("TX thread started. Will transmit every", TX_INTERVAL, "seconds")
    while True:
        try:
            # Check if we have GPS data to transmit
            if not gps_queue.empty():
                # Get the latest GPS data
                gps_data = gps_queue.get()
                
                # Transmit the data
                print(f"TX: lat={gps_data['lat']/1_000_000}, lon={gps_data['lon']/1_000_000}, alt={gps_data['alt']}m, ts={gps_data['timestamp']}")
                spi_tx(gps_data['payload'])
                
                # Mark the queue task as done
                gps_queue.task_done()
            
            # Wait for the next transmission interval
            time.sleep(TX_INTERVAL)
        except Exception as e:
            print(f"Error in TX thread: {e}")
            time.sleep(TX_INTERVAL)  # Keep the timing consistent even if there's an error

def main():
    init_module()
    
    # Create and start GPS thread
    gps_thread = threading.Thread(target=gps_thread_function, daemon=True)
    gps_thread.start()
    
    # Create and start TX thread
    tx_thread = threading.Thread(target=tx_thread_function, daemon=True)
    tx_thread.start()
    
    try:
        # Keep the main thread alive
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Interrupted by user. Exiting.")
        cleanup()
        sys.exit(0)

if __name__ == "__main__":
    main()