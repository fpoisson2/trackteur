#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct
import requests
import threading

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

HOP_CHANNELS = []
for i in range(64):
    freq = FREQ_START + i * FREQ_STEP
    frf = int((freq * FRF_FACTOR) / FXTAL)
    msb = (frf >> 16) & 0xFF
    mid = (frf >> 8) & 0xFF
    lsb = frf & 0xFF
    HOP_CHANNELS.append((msb, mid, lsb))

current_channel = 0
last_message_id = 0  # Track the last message ID to avoid duplicates

# Traccar server configuration
TRACCAR_URL = "http://trackteur.ve2fpd.com:5055"
DEVICE_ID = "212901"

# Flag to indicate if transmitting (for thread safety)
transmitting = False

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

def setup_rx_mode():
    # Set FIFO address pointer to RX base address
    spi_write(0x0D, spi_read(0x0F))
    
    # Set DIO0 mapping for RxDone (in lora mode)
    spi_write(0x40, 0x00)
    
    # Clear all IRQ flags
    spi_write(0x12, 0xFF)
    
    # Set to continuous RX mode
    spi_write(0x01, 0x85)

def setup_tx_mode():
    global transmitting
    transmitting = True
    
    # Set FIFO address pointer to TX base address
    spi_write(0x0D, spi_read(0x0E))
    
    # Set DIO0 mapping for TxDone
    spi_write(0x40, 0x40)  # For frequency hopping
    
    # Clear all IRQ flags
    spi_write(0x12, 0xFF)
    
    # Go to standby mode first
    spi_write(0x01, 0x81)
    time.sleep(0.1)

def send_ack(message_id):
    global transmitting, current_channel
    
    if transmitting:
        print("Already transmitting, cannot send ACK")
        return False
    
    # Create ACK payload: [0xAA, message_id, checksum]
    checksum = (0xAA + message_id) & 0xFF
    ack_payload = bytes([0xAA, message_id, checksum])
    
    print(f"Sending ACK for message ID {message_id}: {ack_payload.hex()}")
    
    # Setup for TX mode
    setup_tx_mode()
    
    # Reset FIFO pointer
    spi_write(0x0D, 0x00)
    
    # Write ACK payload into FIFO
    for byte in ack_payload:
        spi_write(0x00, byte)
    
    spi_write(0x22, len(ack_payload))  # Set payload length
    
    # Set initial channel
    current_channel = 0
    set_frequency(current_channel)
    
    # Switch to TX mode
    spi_write(0x01, 0x83)
    print("Transmitting ACK with frequency hopping")
    
    # Handle frequency hopping during transmission
    start = time.time()
    while time.time() - start < 3:  # Timeout after 3 seconds
        if GPIO.input(DIO0) == 1:
            irq_flags = spi_read(0x12)
            
            # Check for FhssChangeChannel (bit 2)
            if irq_flags & 0x04:
                hop_channel = spi_read(0x1C)
                current_channel = hop_channel & 0x3F
                print(f"FHSS interrupt: Current channel {current_channel}")
                
                # Update to next channel
                current_channel = (current_channel + 1) % len(HOP_CHANNELS)
                set_frequency(current_channel)
                
                # Clear FhssChangeChannel interrupt
                spi_write(0x12, 0x04)
            
            # Check for TX Done (bit 3)
            if irq_flags & 0x08:
                print("ACK transmission complete!")
                break
        
        time.sleep(0.01)
    
    if time.time() - start >= 3:
        print("ACK TX timeout. IRQ flags:", hex(spi_read(0x12)))
    
    # Clear all IRQ flags
    spi_write(0x12, 0xFF)
    
    # Return to RX mode
    setup_rx_mode()
    transmitting = False
    
    return True

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

    # Set FIFO RX/TX base address
    spi_write(0x0F, 0x00)  # RX base address
    spi_write(0x0E, 0x00)  # TX base address

    # Setup RX mode
    setup_rx_mode()
    print("LoRa receiver initialized and listening with frequency hopping")

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

def process_data_payload(payload):
    """Process a data payload (type 0x01) and send ACK"""
    global last_message_id
    
    if len(payload) < 2:
        print("Payload too short to contain message ID")
        return
        
    message_type = payload[0]
    message_id = payload[1]
    data = payload[2:]
    
    # Check if this is a new message or a duplicate
    if message_id == last_message_id:
        print(f"Duplicate message ID {message_id} received, sending ACK again")
        send_ack_thread = threading.Thread(target=send_ack, args=(message_id,))
        send_ack_thread.daemon = True
        send_ack_thread.start()
        return
        
    last_message_id = message_id
    
    # Process data based on message type
    if message_type == 0x01:  # GPS data type
        if len(data) == 14:  # Expected length for GPS data
            try:
                lat, lon, alt, timestamp = struct.unpack(">iiHI", data)
                latitude = lat / 1_000_000.0
                longitude = lon / 1_000_000.0
                time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp))
                print(f"Received GPS data: Latitude={latitude}, Longitude={longitude}, Altitude={alt}m, Timestamp={time_str}")
                
                # Send data to Traccar
                traccar_thread = threading.Thread(target=send_to_traccar, 
                                               args=(latitude, longitude, alt, timestamp))
                traccar_thread.daemon = True
                traccar_thread.start()
                
                # Send ACK in a separate thread to avoid blocking reception
                send_ack_thread = threading.Thread(target=send_ack, args=(message_id,))
                send_ack_thread.daemon = True
                send_ack_thread.start()
                
            except struct.error as e:
                print("Error decoding GPS payload:", e)
        else:
            print(f"Unexpected payload size: {len(data)} bytes (expected 14)")
    else:
        print(f"Unknown message type: {message_type}")

def receive_loop():
    global current_channel, transmitting
    print("Listening for incoming LoRa packets with frequency hopping...")
    setup_rx_mode()
    
    while True:
        # Skip checking if we're currently transmitting an ACK
        if transmitting:
            time.sleep(0.01)
            continue
            
        # Check IRQ flags
        irq_flags = spi_read(0x12)
        
        # Check for FhssChangeChannel interrupt (bit 2)
        if GPIO.input(DIO0) and (irq_flags & 0x04):
            hop_channel = spi_read(0x1C)
            current_channel = hop_channel & 0x3F
            print(f"FHSS interrupt: Current channel {current_channel}")
            
            # Update to next channel
            current_channel = (current_channel + 1) % len(HOP_CHANNELS)
            set_frequency(current_channel)
            
            # Clear FhssChangeChannel interrupt
            spi_write(0x12, 0x04)
        
        # Check for RX Done (bit 6)
        if irq_flags & 0x40:
            print(f"IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
            
            # Check for CRC error
            if irq_flags & 0x20:
                print("CRC error detected, discarding packet")
                spi_write(0x12, 0xFF)  # Clear IRQ flags
                continue
                
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
            
            # Clear IRQ flags
            spi_write(0x12, 0xFF)
            
            # Check if this is an ACK message (starts with 0xAA)
            if len(payload) > 0 and payload[0] == 0xAA:
                print("Received ACK message, ignoring")
            # Check if this is a data message (type 0x01)
            elif len(payload) > 0 and payload[0] == 0x01:
                process_data_payload(payload)
            else:
                print("Unknown message format")
                try:
                    message = ''.join(chr(b) for b in payload if 32 <= b <= 126)
                    print(f"Raw data: {payload.hex()}")
                    print(f"As text: {message}")
                except Exception as e:
                    print(f"Error processing raw payload: {e}")
        
        time.sleep(0.01)  # Small delay to prevent CPU hogging

def cleanup():
    spi.close()
    GPIO.cleanup()
    print("Cleanup complete")

if __name__ == "__main__":
    try:
        init_lora()
        receive_loop()
    except KeyboardInterrupt:
        print("\nTerminating...")
        cleanup()
        sys.exit(0)