#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import requests  # Import the requests library for HTTP communication

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

# Frequency hopping configuration 
FREQ_START = 902200000  # 902.2 MHz
FREQ_STEP = 200000      # 200 kHz (reduced from 400 kHz to fit more channels)
FXTAL = 32000000
FRF_FACTOR = 2**19

HOP_CHANNELS = []
for i in range(128):  # Changed from 64 to 128 channels
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
    spi_write(0x06, msb)  # RegFrfMsb
    spi_write(0x07, mid)  # RegFrfMid
    spi_write(0x08, lsb)  # RegFrfLsb

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

    # RegModemConfig1: BW 125 kHz, CR 4/8, Implicit Header
    spi_write(0x1D, 0x70)
    
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

def receive_loop():
    global current_channel
    hop_count = 0
    last_hop_time = time.time()
    last_status_time = time.time()
    print("Listening for incoming LoRa packets with frequency hopping...")
    
    while True:
        current_time = time.time()
        
        # Print status every 10 seconds for debugging
        if current_time - last_status_time > 10:
            irq_flags = spi_read(0x12)
            reg_op_mode = spi_read(0x01)
            reg_hop_channel = spi_read(0x1C)
            print(f"[{time.strftime('%H:%M:%S')}] Status - OpMode: 0x{reg_op_mode:02X}, " +
                  f"IRQ: 0x{irq_flags:02X}, HopChannel: 0x{reg_hop_channel:02X}")
            last_status_time = current_time
        
        # Check if DIO0 is high, indicating an interrupt
        if GPIO.input(DIO0) == 1:
            irq_flags = spi_read(0x12)
            print(f"[{time.strftime('%H:%M:%S')}] DIO0 interrupt detected! IRQ flags: 0x{irq_flags:02X}")
            
            # Check all possible flags
            if irq_flags & 0x80: print("  - CadDetected flag set")
            if irq_flags & 0x40: print("  - FhssChangeChannel flag set")
            if irq_flags & 0x20: print("  - CadDone flag set")
            if irq_flags & 0x10: print("  - TxTimeout flag set")
            if irq_flags & 0x08: print("  - ValidHeader flag set")
            if irq_flags & 0x04: print("  - PayloadCrcError flag set")
            if irq_flags & 0x02: print("  - RxDone flag set")
            if irq_flags & 0x01: print("  - TxDone flag set")
            
            # Specific handling for FhssChangeChannel
            if irq_flags & 0x40:  # This is the actual FhssChangeChannel bit
                hop_count += 1
                hop_interval = current_time - last_hop_time
                last_hop_time = current_time
                
                reg_hop_channel = spi_read(0x1C)
                current_channel = reg_hop_channel & 0x3F
                
                print(f"FHSS Hop #{hop_count}: Channel {current_channel}, " +
                      f"Interval: {hop_interval:.3f}s")
                
                # Update to next channel
                next_channel = (current_channel + 1) % len(HOP_CHANNELS)
                set_frequency(next_channel)
                print(f"Setting next channel to {next_channel}, " +
                      f"Freq: {(FREQ_START + next_channel * FREQ_STEP)/1000000:.3f} MHz")
                
                # Clear FhssChangeChannel interrupt
                spi_write(0x12, 0x40)
            
            # Check for RxDone
            if irq_flags & 0x02:  # RxDone flag
                print(f"Packet received at channel {current_channel}")
                
                # Clear IRQ flags
                spi_write(0x12, 0xFF)
                
                # Read payload length
                nb_bytes = spi_read(0x13)
                print(f"Packet size: {nb_bytes} bytes")
                
                # Read current FIFO address
                current_addr = spi_read(0x10)
                
                # Set FIFO address pointer
                spi_write(0x0D, current_addr)
                
                # Read payload
                payload = bytearray()
                for _ in range(nb_bytes):
                    payload.append(spi_read(0x00))

                print(f"Raw RX Payload ({len(payload)} bytes): {payload.hex()}")
                
                # Decode payload if 14 bytes
                if len(payload) == 14:
                    try:
                        lat, lon, alt, timestamp = struct.unpack(">iiHI", payload)
                        latitude = lat / 1_000_000.0
                        longitude = lon / 1_000_000.0
                        time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                        print(f"Received: Latitude={latitude}, Longitude={longitude}, Altitude={alt}m, Timestamp={time_str}")
                        
                        # Send data to Traccar
                        send_to_traccar(latitude, longitude, alt, timestamp)
                    except struct.error as e:
                        print("Error decoding payload:", e)
                else:
                    print(f"Unexpected payload size: {nb_bytes} bytes (expected 14)")
                    try:
                        message = ''.join(chr(b) for b in payload if 32 <= b <= 126)
                        print(f"Raw data: {payload.hex()}")
                        print(f"As text: {message}")
                    except Exception as e:
                        print(f"Error processing raw payload: {e}")
                
                # Check for CRC error
                if irq_flags & 0x04:
                    print("CRC error detected")
            
            # Clear all IRQ flags after processing
            spi_write(0x12, 0xFF)
        
        # Polling for RxDone as a backup method (in case interrupt is missed)
        irq_flags = spi_read(0x12)
        if irq_flags & 0x02:  # RxDone flag
            print(f"RxDone detected via polling! IRQ: 0x{irq_flags:02X}")
            
            # Handle packet reception (same code as above)
            # Clear IRQ flags
            spi_write(0x12, 0xFF)
            
            # Read payload length
            nb_bytes = spi_read(0x13)
            print(f"Packet size: {nb_bytes} bytes")
            
            # Read current FIFO address
            current_addr = spi_read(0x10)
            
            # Set FIFO address pointer
            spi_write(0x0D, current_addr)
            
            # Read payload
            payload = bytearray()
            for _ in range(nb_bytes):
                payload.append(spi_read(0x00))

            print(f"Raw RX Payload ({len(payload)} bytes): {payload.hex()}")
            
            # Decode payload if 14 bytes
            if len(payload) == 14:
                try:
                    lat, lon, alt, timestamp = struct.unpack(">iiHI", payload)
                    latitude = lat / 1_000_000.0
                    longitude = lon / 1_000_000.0
                    time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                    print(f"Received: Latitude={latitude}, Longitude={longitude}, Altitude={alt}m, Timestamp={time_str}")
                    
                    # Send data to Traccar
                    send_to_traccar(latitude, longitude, alt, timestamp)
                except struct.error as e:
                    print("Error decoding payload:", e)
            else:
                print(f"Unexpected payload size: {nb_bytes} bytes (expected 14)")
                try:
                    message = ''.join(chr(b) for b in payload if 32 <= b <= 126)
                    print(f"Raw data: {payload.hex()}")
                    print(f"As text: {message}")
                except Exception as e:
                    print(f"Error processing raw payload: {e}")
            
            # Check for CRC error
            if irq_flags & 0x04:
                print("CRC error detected")
        
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