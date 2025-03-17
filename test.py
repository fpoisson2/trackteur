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
    # For RegDioMapping1 (0x40), bits 7-6 control DIO0.
    # Setting them to '01' (i.e. value 0x40) makes DIO0 indicate TX done.
    spi_write(0x40, 0x40)
    
    # Set frequency to 915 MHz (adjust if needed)
    frequency = 915000000
    # Frequency step Fstep ≈ 32e6 / 2^19 ≈ 61.035 Hz
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

def test_register_rw():
    print("\n[Register R/W Test]")
    original = spi_read(0x0D)  # Read FIFO pointer register
    print(f"Original RegFifoAddrPtr (0x0D): {hex(original)}")
    test_val = 0x55
    spi_write(0x0D, test_val)
    read_back = spi_read(0x0D)
    print(f"Value after writing {hex(test_val)}: {hex(read_back)}")
    spi_write(0x0D, original)  # Restore original value
    if read_back == test_val:
        print("Register R/W test passed!")
    else:
        print("Register R/W test failed!")

def test_tx_mode():
    payload = "test_tx"
    print("\n[TX Mode Test]")
    # Reset FIFO pointer
    spi_write(0x0D, 0x00)
    # Write payload bytes into FIFO
    for char in payload:
        spi_write(0x00, ord(char))
    spi_write(0x22, len(payload))  # Set payload length
    
    # Switch to TX mode: RegOpMode = 0x83 (LoRa TX mode)
    spi_write(0x01, 0x83)
    print("Payload transmitted, waiting for TX done signal...")
    
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

def test_rx_mode():
    print("\n[RX Mode Test]")
    # Set module to RX continuous mode (RegOpMode = 0x85)
    spi_write(0x01, 0x85)
    start = time.time()
    received = False
    while time.time() - start < 5:
        if GPIO.input(DIO0) == 1:
            print("RX done signal detected!")
            # Read number of received bytes from RegRxNbBytes (0x13)
            nb_bytes = spi_read(0x13)
            print("Payload length:", nb_bytes)
            payload = []
            for i in range(nb_bytes):
                payload.append(spi_read(0x00))
            print("Received payload:", ''.join(chr(b) for b in payload))
            received = True
            break
        time.sleep(0.01)
    if not received:
        print("No packet received in RX mode.")
    # Return to standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def main():
    init_module()
    test_register_rw()
    test_tx_mode()
    test_rx_mode()
    cleanup()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Interrupted by user. Exiting.")
        cleanup()
        sys.exit(0)
