import spidev
import RPi.GPIO as GPIO
import time
import sys

# --- Pin configuration (using BCM numbering) ---
RESET = 17   # Reset pin
NSS   = 25   # SPI Chip Select pin (manual control)
DIO0  = 4    # DIO0 pin; WiringPi pin 7 corresponds to BCM GPIO4

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
    
    # Explicitly set DIO mapping: Map DIO0 to RX done.
    spi_write(0x40, 0x00)
    
    # Set frequency to 915 MHz (adjust if needed)
    frequency = 915000000
    frf = int(frequency / 61.03515625)
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb
    
    # Minimal modem configuration (example values)
    spi_write(0x1D, 0x78)  # RegModemConfig1: e.g., BW=125 kHz, CR=4/7, explicit header
    spi_write(0x1E, 0xC4)  # RegModemConfig2: e.g., SF7, CRC on
    spi_write(0x26, 0x0C)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)  # RegPreambleMsb
    spi_write(0x21, 0x08)  # RegPreambleLsb
    
    # Set PA configuration (example value; ensure PA_BOOST is used if wired that way)
    spi_write(0x09, 0x8F)  # RegPaConfig
    
    # Set FIFO RX base address to 0 and reset FIFO pointer
    spi_write(0x0E, 0x00)  # RegFifoRxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr

def spi_rx():
    """Function to receive data using LoRa."""
    # Set module to RX continuous mode (RegOpMode = 0x85)
    spi_write(0x01, 0x85)
    print("Listening for incoming data...")
    
    while True:  # Keep listening indefinitely
        if GPIO.input(DIO0) == 1:
            print("RX done signal detected!")
            # Read number of received bytes from RegRxNbBytes (0x13)
            nb_bytes = spi_read(0x13)
            print(f"Payload length: {nb_bytes} bytes")
            payload = []
            for i in range(nb_bytes):
                payload.append(spi_read(0x00))
            received_data = ''.join(chr(b) for b in payload)
            print(f"Received payload: {received_data}")
        
        time.sleep(0.1)  # Sleep to avoid busy loop

def main():
    init_module()
    try:
        spi_rx()  # Start listening for incoming LoRa packets
    except KeyboardInterrupt:
        print("Interrupted by user. Exiting.")
        cleanup()
        sys.exit(0)

if __name__ == "__main__":
    main()