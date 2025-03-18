#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys

# Pin definitions (BCM mode)
RESET = 17  # Reset pin
NSS = 25    # SPI Chip Select pin
DIO0 = 4    # DIO0 pin for TxDone interrupt

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

    # RegPaConfig: +20 dBm with PA_BOOST
    spi_write(0x09, 0x8F)  # PA_BOOST enabled, max power
    spi_write(0x0B, 0x3F)  # OCP off

    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header, CRC on
    spi_write(0x1D, 0xA3)  # BW=125 kHz (7:6=10), CR=4/5 (5:4=00), Header=explicit (3=1), CRC=on (2=1)

    # RegModemConfig2: SF12, Single shot, CRC on
    spi_write(0x1E, 0xC0)  # SF=12 (7:4=1100), Tx single shot (3=0), CRC=on (2=0)

    # RegModemConfig3: LowDataRateOptimize on, AGC on
    spi_write(0x26, 0x0C)  # LDRO=1 (3=1), AGC=1 (2=1)

    # Preamble length: 8 symbols
    spi_write(0x20, 0x00)  # MSB
    spi_write(0x21, 0x08)  # LSB

    # Set FIFO TX base address and pointer
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr

    # Map DIO0 to TxDone (bit 7:6 = 01 for TxDone)
    spi_write(0x40, 0x40)  # This should be 0x40, not 0x00 for TxDone

def transmit_message(message):
    payload = message.encode('ascii')
    if len(payload) > 255:  # LoRa packet max size
        payload = payload[:255]
    
    print(f"Transmitting: {message} ({len(payload)} bytes)")
    
    # Set FIFO pointer to TX base address
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr
    spi_write(0x0D, 0x00)  # Reset pointer to TX base
    
    # Write payload to FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    # Set payload length
    spi_write(0x22, len(payload))  # RegPayloadLength
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Start transmission
    spi_write(0x01, 0x83)  # LoRa mode, TX
    
    # Wait for transmission to complete
    start_time = time.time()
    while time.time() - start_time < 5:
        # Either poll IRQ register or check DIO0 pin
        irq_flags = spi_read(0x12)
        dio0_state = GPIO.input(DIO0)
        
        if irq_flags & 0x08 or dio0_state:  # TxDone flag or DIO0 high
            print(f"Transmission complete! IRQ flags: {bin(irq_flags)} (0x{irq_flags:02x}), DIO0: {dio0_state}")
            break
        time.sleep(0.01)
    
    if time.time() - start_time >= 5:
        print("TX timeout. IRQ flags:", hex(spi_read(0x12)))
    
    # Clear IRQ flags and return to standby
    spi_write(0x12, 0xFF)
    spi_write(0x01, 0x81)  # Back to standby

def cleanup():
    spi.close()
    GPIO.cleanup()

if __name__ == "__main__":
    try:
        init_lora()
        message = "Hello, SX1276!"
        counter = 0
        while True:
            transmit_message(f"{message} #{counter}")
            counter += 1
            time.sleep(5)  # Transmit every 5 seconds
    except KeyboardInterrupt:
        print("\nTerminating...")
        cleanup()
        sys.exit(0)