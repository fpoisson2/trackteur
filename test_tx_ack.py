#!/usr/bin/env python3
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
    
    # Explicitly set DIO mapping: Map DIO0 to TX done.
    spi_write(0x40, 0x40)  # DIO0 mapped to TxDone (bits 7-6 = 01)
    
    # Set frequency to 915 MHz (matching RX script)
    frequency = 915000000
    frf = int(frequency / 61.03515625)
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb
    
    # Modem configuration (matching RX script)
    spi_write(0x1D, 0x78)  # RegModemConfig1: BW=125 kHz, CR=4/7, explicit header
    spi_write(0x1E, 0xC4)  # RegModemConfig2: SF7, CRC on
    spi_write(0x26, 0x0C)  # RegModemConfig3: LDRO on, AGC on
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)  # RegPreambleMsb
    spi_write(0x21, 0x08)  # RegPreambleLsb
    
    # Set PA configuration (example value; adjust if needed)
    spi_write(0x09, 0x8F)  # RegPaConfig: +20 dBm with PA_BOOST
    
    # Set FIFO TX base address to 0 and reset FIFO pointer
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr

def send_ack():
    """Function to send an ACK message."""
    payload = "ACK"  # ACK message to send
    print("\n[Sending ACK]")
    
    # Reset FIFO pointer
    spi_write(0x0D, 0x00)
    
    # Write payload bytes into FIFO
    for char in payload:
        spi_write(0x00, ord(char))
    
    # Set payload length
    spi_write(0x22, len(payload))
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Switch to TX mode: RegOpMode = 0x83 (LoRa TX mode)
    spi_write(0x01, 0x83)
    print("Transmitting ACK...")
    
    # Wait for TX done signal (DIO0 high)
    start = time.time()
    while time.time() - start < 5:
        if GPIO.input(DIO0) == 1:
            print("ACK sent successfully!")
            break
        time.sleep(0.01)
    else:
        irq = spi_read(0x12)
        print("TX done timeout. IRQ flags:", hex(irq))
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Return to standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def main():
    init_module()
    # Remove other test functions, focus on sending ACK periodically
    while True:
        send_ack()
        time.sleep(5)  # Send ACK every 5 seconds

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Interrupted by user. Exiting.")
        cleanup()
        sys.exit(0)