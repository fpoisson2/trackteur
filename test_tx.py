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

def read_registers():
    """Read and print key registers for debugging"""
    print("Register values:")
    print(f"RegOpMode (0x01): 0x{spi_read(0x01):02X}")
    print(f"RegPaConfig (0x09): 0x{spi_read(0x09):02X}")
    print(f"RegPaRamp (0x0A): 0x{spi_read(0x0A):02X}")
    print(f"RegOcp (0x0B): 0x{spi_read(0x0B):02X}")
    print(f"RegFifoTxBaseAddr (0x0E): 0x{spi_read(0x0E):02X}")
    print(f"RegFifoAddrPtr (0x0D): 0x{spi_read(0x0D):02X}")
    print(f"RegIrqFlags (0x12): 0x{spi_read(0x12):02X}")
    print(f"RegDioMapping1 (0x40): 0x{spi_read(0x40):02X}")
    print(f"RegDioMapping2 (0x41): 0x{spi_read(0x41):02X}")
    print(f"RegPaDac (0x4D): 0x{spi_read(0x4D):02X}")

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

    # Set to Sleep mode
    spi_write(0x01, 0x00)  # FSK/OOK mode, sleep
    time.sleep(0.1)
    
    # Switch to LoRa mode (can only be done in sleep mode)
    spi_write(0x01, 0x80)  # LoRa mode, sleep
    time.sleep(0.1)
    
    # Set to standby mode
    spi_write(0x01, 0x81)  # LoRa mode, standby
    time.sleep(0.1)

    # Set frequency to 915 MHz
    frf = int(915000000 / 61.03515625)  # Fstep = 32e6 / 2^19 ≈ 61.035 Hz
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb

    # Configure PA_BOOST
    # RegPaConfig: Enable PA_BOOST, set output power to 17dBm (14+3)
    spi_write(0x09, 0x8F)  # 1000 1111 - PA_BOOST pin, max power (15dBm) + 2dB
    
    # Set PA ramp-up time 40us
    spi_write(0x0A, 0x08)  # RegPaRamp: 40us
    
    # Turn off Over Current Protection
    spi_write(0x0B, 0x20)  # RegOcp: OCP disable
    
    # Enable high power mode (for +20dBm output)
    spi_write(0x4D, 0x87)  # RegPaDac: 0x87 for +20dBm

    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header, CRC on
    spi_write(0x1D, 0xA3)  # BW=125 kHz, CR=4/5, Explicit header, CRC on

    # RegModemConfig2: SF12, Continuous mode, CRC on
    spi_write(0x1E, 0xC4)  # SF=12, Rx continuous mode, CRC on

    # RegModemConfig3: LowDataRateOptimize on, AGC on
    spi_write(0x26, 0x0C)  # LowDataRateOptimize on, AGC on
    
    # Set symbol timeout (only the LSB; the MSB remains at its default)
    spi_write(0x1F, 0xFF)  # RegSymbTimeout LSB

    # Set preamble length: 8 symbols
    spi_write(0x20, 0x00)  # Preamble length MSB (0x0008)
    spi_write(0x21, 0x08)  # Preamble length LSB

    # Set FIFO TX base address
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr = 0
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr = 0

    # Map DIO0 to TxDone (01 in bits 7:6)
    spi_write(0x40, 0x40)  # RegDioMapping1: TxDone on DIO0

    # Clear IRQ flags
    spi_write(0x12, 0xFF)  # RegIrqFlags: Clear all IRQ flags
    
    print("LoRa module initialized")
    read_registers()

def transmit_message(message):
    payload = message.encode('ascii')
    if len(payload) > 255:  # LoRa packet max size
        payload = payload[:255]
    
    print(f"Transmitting: {message} ({len(payload)} bytes)")
    
    # Ensure the module is in standby mode
    spi_write(0x01, 0x81)  # LoRa mode, standby
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Reset FIFO pointer to the TX base address
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr = RegFifoTxBaseAddr
    
    # Write payload to FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    # Set payload length
    spi_write(0x22, len(payload))  # RegPayloadLength
    
    # Start transmission
    spi_write(0x01, 0x83)  # LoRa mode, TX
    
    print("Transmission started - waiting for completion")
    print(f"Initial IRQ flags: 0x{spi_read(0x12):02X}")
    
    # Wait for transmission to complete
    start_time = time.time()
    while time.time() - start_time < 5:
        irq_flags = spi_read(0x12)
        dio0_state = GPIO.input(DIO0)
        
        if irq_flags & 0x08:  # TxDone flag detected
            print(f"Transmission complete! IRQ flags: 0x{irq_flags:02X}, DIO0: {dio0_state}")
            break
        
        time.sleep(0.1)
        # Optionally, print periodic status
        if int((time.time() - start_time) * 10) % 10 == 0:
            print(f"Waiting... IRQ flags: 0x{irq_flags:02X}, DIO0: {dio0_state}")
    
    if time.time() - start_time >= 5:
        print(f"TX timeout. IRQ flags: 0x{spi_read(0x12):02X}")
        print(f"OpMode: 0x{spi_read(0x01):02X}")  # Check the current mode
    
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
