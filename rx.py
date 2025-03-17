#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct

# Pin definitions (BCM mode)
RESET = 17
NSS = 25
DIO0 = 4

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

# Frequency hopping channels (902.2 MHz to 927.8 MHz, 400 kHz spacing)
FREQ_START = 902200000
FREQ_STEP = 400000
FXTAL = 32000000
FRF_FACTOR = 2**19
FREQUENCY_STEP = FXTAL / FRF_FACTOR  # ~61.035 Hz
HOP_CHANNELS = []
for i in range(64):
    freq = FREQ_START + i * FREQ_STEP
    frf = int((freq * FRF_FACTOR) / FXTAL)
    msb = (frf >> 16) & 0xFF
    mid = (frf >> 8) & 0xFF
    lsb = frf & 0xFF
    HOP_CHANNELS.append((msb, mid, lsb))

current_channel = 0
current_frf = int((FREQ_START * FRF_FACTOR) / FXTAL)  # Initial Frf for channel 0

def spi_write(addr, val):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, val])
    GPIO.output(NSS, GPIO.HIGH)

def spi_read(addr):
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr & 0x7F])
    val = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    return val

def reset_module():
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def set_frequency(frf):
    msb = (frf >> 16) & 0xFF
    mid = (frf >> 8) & 0xFF
    lsb = frf & 0xFF
    spi_write(0x06, msb)
    spi_write(0x07, mid)
    spi_write(0x08, lsb)
    freq_mhz = (frf * FREQUENCY_STEP) / 1_000_000
    print(f"Set frequency to {freq_mhz:.3f} MHz (Frf: {hex(frf)})")

def init_lora():
    reset_module()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected!")
        cleanup()
        sys.exit(1)

    spi_write(0x01, 0x80)  # Sleep mode, LoRa
    time.sleep(0.1)

    spi_write(0x40, 0x40)  # DIO0 = FhssChangeChannel
    set_frequency(current_frf)  # Initial frequency (channel 0)

    spi_write(0x1D, 0x68)  # BW 62.5 kHz, CR 4/8, Implicit Header
    spi_write(0x1E, 0xC4)  # SF12, CRC on
    spi_write(0x26, 0x0C)  # LDRO on, AGC on
    spi_write(0x20, 0x00)  # Preamble MSB
    spi_write(0x21, 0x08)  # Preamble LSB
    spi_write(0x24, 5)     # Hop every 5 symbols (~327.7 ms)
    spi_write(0x0F, 0x00)  # FIFO RX base addr
    spi_write(0x0D, 0x00)  # FIFO addr ptr

    spi_write(0x01, 0x85)  # Continuous RX mode

def fhss_callback(channel):
    global current_channel, current_frf
    hop_channel = spi_read(0x1C)
    current_channel = hop_channel & 0x3F
    current_channel = (current_channel + 1) % len(HOP_CHANNELS)
    msb, mid, lsb = HOP_CHANNELS[current_channel]
    current_frf = (msb << 16) | (mid << 8) | lsb
    set_frequency(current_frf)
    spi_write(0x12, 0x04)  # Clear FhssChangeChannel interrupt
    print(f"FHSS interrupt: Switched to channel {current_channel}")

def get_frequency_error():
    # Read FEI registers (20-bit signed value)
    fei_msb = spi_read(0x1C) & 0x0F  # Only 4 LSBs are used
    fei_mid = spi_read(0x1D)
    fei_lsb = spi_read(0x1E)
    fei = (fei_msb << 16) | (fei_mid << 8) | fei_lsb
    
    # Sign-extend the 20-bit value
    if fei & 0x80000:  # If sign bit is set
        fei -= 1 << 20
    
    # Convert to Hz
    freq_error_hz = fei * FREQUENCY_STEP
    return freq_error_hz, fei

def receive_loop():
    GPIO.add_event_detect(DIO0, GPIO.RISING, callback=fhss_callback)
    print("Listening for LoRa packets with frequency hopping and correction...")
    while True:
        irq_flags = spi_read(0x12)
        if irq_flags & 0x40:  # RX Done
            print(f"IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
            spi_write(0x12, 0xFF)  # Clear IRQ flags
            
            # Measure frequency error
            freq_error_hz, fei = get_frequency_error()
            print(f"Frequency Error: {freq_error_hz:.1f} Hz (FEI: {fei})")
            
            # Adjust current frequency
            current_frf -= fei  # Subtract FEI to correct drift
            set_frequency(current_frf)
            
            # Read payload
            nb_bytes = spi_read(0x13)
            print(f"Packet size: {nb_bytes} bytes")
            current_addr = spi_read(0x10)
            spi_write(0x0D, current_addr)
            payload = bytearray()
            for _ in range(nb_bytes):
                payload.append(spi_read(0x00))

            print(f"Raw RX Payload ({len(payload)} bytes): {payload.hex()}")
            
            if len(payload) == 14:
                try:
                    lat, lon, alt, timestamp = struct.unpack(">iiHI", payload)
                    latitude = lat / 1_000_000.0
                    longitude = lon / 1_000_000.0
                    time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                    print(f"Received: Latitude={latitude}, Longitude={longitude}, Altitude={alt}m, Timestamp={time_str}")
                except struct.error as e:
                    print("Error decoding payload:", e)
            else:
                print(f"Unexpected payload size: {nb_bytes} bytes (expected 14)")
            
            if irq_flags & 0x20:  # CRC Error
                print("CRC error detected")
        
        time.sleep(0.01)  # Small delay to prevent CPU hogging

def cleanup():
    GPIO.remove_event_detect(DIO0)
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