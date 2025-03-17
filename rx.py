#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import requests  # HTTP communication

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
FREQ_START = 902200000  # 902.2 MHz
FREQ_STEP = 400000      # 400 kHz
FXTAL = 32000000
FRF_FACTOR = 2**19

# Define fixed ACK channel
ACK_CHANNEL = 0  # Using channel 0 (902.2 MHz) for ACK

HOP_CHANNELS = []
for i in range(64):
    freq = FREQ_START + i * FREQ_STEP
    frf = int((freq * FRF_FACTOR) / FXTAL)
    msb = (frf >> 16) & 0xFF
    mid = (frf >> 8) & 0xFF
    lsb = frf & 0xFF
    HOP_CHANNELS.append((msb, mid, lsb))

current_channel = 0

# Traccar server configuration
TRACCAR_URL = "http://trackteur.ve2fpd.com:5055"  # Adjust port if needed
DEVICE_ID = "212901"  # Replace with your device's unique ID registered in Traccar

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

def set_frequency(channel_idx):
    global HOP_CHANNELS
    msb, mid, lsb = HOP_CHANNELS[channel_idx]
    freq_hz = (((msb << 16) | (mid << 8) | lsb) * FXTAL) / FRF_FACTOR
    print(f"Setting frequency to {freq_hz/1000000:.3f} MHz (channel {channel_idx})")
    spi_write(0x06, msb)  # RegFrfMsb
    spi_write(0x07, mid)  # RegFrfMid
    spi_write(0x08, lsb)  # RegFrfLsb

def debug_registers():
    """Print important register values for debugging."""
    print("--- DEBUG REGISTERS ---")
    print(f"RegOpMode (0x01): 0b{spi_read(0x01):08b}")
    print(f"RegDioMapping1 (0x40): 0b{spi_read(0x40):08b}")
    print(f"RegIrqFlags (0x12): 0b{spi_read(0x12):08b}")
    print(f"RegFifoRxBaseAddr (0x0F): 0x{spi_read(0x0F):02x}")
    print(f"RegFifoAddrPtr (0x0D): 0x{spi_read(0x0D):02x}")
    print(f"RegModemConfig1 (0x1D): 0b{spi_read(0x1D):08b}")
    print(f"RegModemConfig2 (0x1E): 0b{spi_read(0x1E):08b}")
    print(f"RegModemConfig3 (0x26): 0b{spi_read(0x26):08b}")
    print(f"RegSymbTimeoutLsb (0x1F): {spi_read(0x1F)}")
    print(f"RegHopPeriod (0x24): {spi_read(0x24)}")
    print("----------------------")

def init_lora():
    reset_module()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    spi_write(0x01, 0x80)  # LoRa sleep mode
    time.sleep(0.1)

    # Set initial frequency to channel 0 (902.2 MHz)
    set_frequency(0)

    # RegModemConfig1: BW 125 kHz, CR 4/8, Explicit Header
    spi_write(0x1D, 0x78)
    
    # RegModemConfig2: SF12, CRC on
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LDRO on, AGC on
    spi_write(0x26, 0x0C)

    # Preamble length: 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)

    # Enable frequency hopping: Hop every 5 symbols (~327.7 ms at 62.5 kHz)
    spi_write(0x24, 5)  # RegHopPeriod

    # Map DIO0 to FhssChangeChannel (bit 7:6 = 01)
    spi_write(0x40, 0x40)

    # Set FIFO RX base addr and pointer
    spi_write(0x0F, 0x00)
    spi_write(0x0D, 0x00)

    # Continuous RX mode
    spi_write(0x01, 0x85)
    
    debug_registers()

def send_to_traccar(latitude, longitude, altitude, timestamp):
    """Send GPS data to Traccar server using the OSMAnd protocol."""
    params = {
        "id": DEVICE_ID,
        "lat": latitude,
        "lon": longitude,
        "altitude": altitude,
        "timestamp": timestamp
    }
    try:
        response = requests.get(TRACCAR_URL, params=params, timeout=10)
        print(f"Traccar response status: {response.status_code}")
        print(f"Traccar response content: {response.text}")
        if response.status_code == 200:
            print("Data successfully sent to Traccar server")
        else:
            print(f"Failed to send data to Traccar. Status code: {response.status_code}")
    except requests.RequestException as e:
        print(f"Error sending data to Traccar: {e}")

def send_ack():
    ack_payload = b"ACK"
    print("Preparing to send ACK...")
    time.sleep(5)
    
    spi_write(0x01, 0x81)  # LoRa standby mode
    time.sleep(0.01)
    
    spi_write(0x24, 0)     # Disable frequency hopping
    set_frequency(ACK_CHANNEL)
    print(f"Set frequency to {FREQ_START/1000000} MHz for ACK transmission")
    
    spi_write(0x40, 0x40)  # DIO0 = TxDone
    time.sleep(0.01)
    
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr
    
    for byte in ack_payload:
        spi_write(0x00, byte)
    spi_write(0x22, len(ack_payload))
    
    spi_write(0x12, 0xFF)  # Clear IRQ flags
    print(f"Pre-TX: RegOpMode={hex(spi_read(0x01))}, DIO0={GPIO.input(DIO0)}, IRQ={hex(spi_read(0x12))}")
    
    spi_write(0x01, 0x83)  # TX mode
    print("Sending ACK...")
    
    start = time.time()
    timeout = 5
    while time.time() - start < timeout:
        dio0_state = GPIO.input(DIO0)
        irq_flags = spi_read(0x12)
        if dio0_state == 1:
            print(f"DIO0 high detected! IRQ flags: {hex(irq_flags)}")
            if irq_flags & 0x08:  # TxDone
                print("ACK sent successfully!")
                break
        time.sleep(0.01)
    else:
        print("ACK transmission timed out!")

def receive_loop():
    global current_channel
    print("Listening for incoming LoRa packets with frequency hopping...")
    while True:
        irq_flags = spi_read(0x12)
        
        # Check for FhssChangeChannel interrupt (bit 2)
        if GPIO.input(DIO0) and (irq_flags & 0x04):
            hop_channel = spi_read(0x1C)
            current_channel = hop_channel & 0x3F
            print(f"FHSS interrupt: Current channel {current_channel}")
            set_frequency(current_channel)
            spi_write(0x12, 0x04)  # Clear FhssChangeChannel interrupt flag
        
        # Check for RX Done (bit 6)
        if irq_flags & 0x40:
            print(f"IRQ Flags: 0b{irq_flags:08b} (0x{irq_flags:02x})")
            if irq_flags & 0x20:
                print("CRC error detected, no ACK sent.")
                spi_write(0x12, 0xFF)
                continue
            
            nb_bytes = spi_read(0x13)
            print(f"Packet size: {nb_bytes} bytes")
            
            current_addr = spi_read(0x10)
            spi_write(0x0D, current_addr)
            
            payload = bytearray()
            for _ in range(nb_bytes):
                payload.append(spi_read(0x00))
            print(f"Raw RX Payload ({len(payload)} bytes): {payload.hex()}")
            
            spi_write(0x12, 0xFF)  # Clear IRQ flags
            
            if len(payload) == 14:  # Expected GPS packet size
                try:
                    lat, lon, alt, timestamp = struct.unpack(">iiHI", payload)
                    latitude = lat / 1_000_000.0
                    longitude = lon / 1_000_000.0
                    time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                    print(f"Received: Latitude={latitude}, Longitude={longitude}, Altitude={alt}m, Timestamp={time_str}")
                    
                    # Send GPS data to Traccar
                    send_to_traccar(latitude, longitude, alt, timestamp)
                    
                    # Send ACK on fixed channel
                    send_ack()
                except struct.error as e:
                    print("Error decoding payload:", e)
            else:
                print(f"Unexpected payload size: {nb_bytes} bytes (expected 14), no ACK sent.")
                try:
                    message = ''.join(chr(b) for b in payload if 32 <= b <= 126)
                    print(f"Raw data: {payload.hex()}")
                    print(f"As text: {message}")
                except Exception as e:
                    print(f"Error processing raw payload: {e}")
        
        time.sleep(0.01)  # Prevent CPU hogging

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