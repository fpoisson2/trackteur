#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys

# Pin definitions (BCM mode)
RESET = 17  # Reset pin
NSS = 25    # SPI Chip Select pin
DIO0 = 4    # DIO0 pin for RxDone interrupt

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

def spi_write(addr, value):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, value])
    GPIO.output(NSS, GPIO.HIGH)

def spi_read(addr):
    GPIO.output(NSS, GPIO.LOW)
    val = spi.xfer2([addr & 0x7F, 0x00])[1]  # Read with dummy byte
    GPIO.output(NSS, GPIO.HIGH)
    return val

def reset_module():
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def init_lora():
    reset_module()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    # Set to Sleep mode with LoRa enabled
    spi_write(0x01, 0x80)  # Sleep mode first to reset
    time.sleep(0.1)
    spi_write(0x01, 0x81)  # LoRa mode, standby
    time.sleep(0.1)

    # Set frequency to 915 MHz
    frf = int(915000000 / 61.03515625)  # Fstep = 32e6 / 2^19 ≈ 61.035 Hz
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb = 0xE4
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid = 0x24
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb = 0x00

    # RegLna: Max gain, boost on
    spi_write(0x0C, 0xC3)  # LNA gain G4, boost on for HF

    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header, CRC on
    spi_write(0x1D, 0xA3)  # BW=125 kHz (7:6=10), CR=4/5 (5:4=00), Header=explicit (3=1), CRC=on (2=1)

    # RegModemConfig2: SF12, Continuous mode, CRC on
    spi_write(0x1E, 0xC4)  # SF=12 (7:4=1100), Rx continuous (3=1), CRC=on (2=1)

    # RegModemConfig3: LowDataRateOptimize on, AGC on
    spi_write(0x26, 0x0C)  # LDRO=1 (3=1), AGC=1 (2=1)

    # Preamble length: 8 symbols
    spi_write(0x20, 0x00)  # MSB
    spi_write(0x21, 0x08)  # LSB

    # Set FIFO RX base address and pointer
    spi_write(0x0F, 0x00)  # RegFifoRxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr

    # Map DIO0 to RxDone (bit 7:6 = 01)
    spi_write(0x40, 0x40)

    # Set to Continuous RX mode
    spi_write(0x01, 0x85)

def receive_loop():
    print("Listening for incoming LoRa packets...")
    while True:
        irq_flags = spi_read(0x12)
        
        # Check for RxDone (bit 6)
        if irq_flags & 0x40:  # RxDone flag is set
            dio0_state = GPIO.input(DIO0)
            print(f"Packet detected - DIO0: {dio0_state}, IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
            
            # Clear IRQ flags
            spi_write(0x12, 0xFF)
            
            # Read payload length
            nb_bytes = spi_read(0x13)  # RegRxNbBytes
            print(f"Packet size: {nb_bytes} bytes")
            
            # Read current FIFO address
            current_addr = spi_read(0x10)  # RegFifoRxCurrentAddr
            print(f"FIFO address: {current_addr}")
            
            # Set FIFO address pointer
            spi_write(0x0D, current_addr)
            
            # Read payload
            payload = bytearray()
            for _ in range(nb_bytes):
                payload.append(spi_read(0x00))
            
            # Convert payload to string if possible, otherwise show raw bytes
            try:
                message = payload.decode('ascii')
                print(f"Received: {message} ({len(payload)} bytes)")
            except UnicodeDecodeError:
                print(f"Received raw bytes: {payload.hex()} ({len(payload)} bytes)")
            
            # Check for CRC error
            if irq_flags & 0x20:
                print("CRC error detected")
            
            # Log RSSI and SNR
            rssi = spi_read(0x1A)  # RegRssiValue
            snr = spi_read(0x19)   # RegPktSnrValue (signed, divide by 4)
            snr_value = snr if snr < 128 else snr - 256  # Convert to signed value
            print(f"RSSI: -{rssi} dBm, SNR: {snr_value / 4} dB")
            print("-" * 40)  # Separator for readability
        
        time.sleep(0.01)  # Small delay to prevent CPU hogging

def cleanup():
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