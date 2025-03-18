#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import argparse
import os
import math
from collections import deque

# Pin definitions (BCM mode)
RESET = 17  # Reset pin
NSS = 25    # SPI Chip Select pin
DIO0 = 4    # DIO0 pin (we'll use polling instead of relying on this interrupt)

# Frequency hopping configuration (matching transmitter)
FREQ_START = 902200000  # 902.2 MHz (US 915 band)
FREQ_STEP = 400000      # 400 kHz
FXTAL = 32000000        # 32 MHz crystal
FRF_FACTOR = 2**19

# RSSI Visualization config
RSSI_UPDATE_INTERVAL = 0.1  # Update RSSI every 0.1 seconds
RSSI_HISTORY_LENGTH = 60    # Store 60 RSSI readings for graph (6 seconds at 0.1s interval)
GRAPH_WIDTH = 60            # Width of the terminal graph
GRAPH_HEIGHT = 10           # Height of the terminal graph

# Generate frequency hopping channels
HOP_CHANNELS = []
for i in range(8):  # Using 8 channels to match transmitter
    freq = FREQ_START + i * FREQ_STEP
    frf = int((freq * FRF_FACTOR) / FXTAL)
    msb = (frf >> 16) & 0xFF
    mid = (frf >> 8) & 0xFF
    lsb = frf & 0xFF
    HOP_CHANNELS.append((msb, mid, lsb))

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
    print("Resetting module...")
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def set_frequency(channel_idx=0):
    """Set frequency using hop channel index or standard 915MHz"""
    if channel_idx >= 0 and channel_idx < len(HOP_CHANNELS):
        msb, mid, lsb = HOP_CHANNELS[channel_idx]
        spi_write(0x06, msb)  # RegFrfMsb
        spi_write(0x07, mid)  # RegFrfMid
        spi_write(0x08, lsb)  # RegFrfLsb
        freq_mhz = (FREQ_START + channel_idx * FREQ_STEP) / 1000000
        print(f"Frequency set to channel {channel_idx}: {freq_mhz:.3f} MHz")
    else:
        # Standard 915 MHz (center of US band)
        frf = int((915000000 * FRF_FACTOR) / FXTAL)
        spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
        spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
        spi_write(0x08, frf & 0xFF)          # RegFrfLsb
        print("Frequency set to 915.000 MHz")

def print_registers():
    """Print key register values for debugging"""
    print("\n=== SX1276 Register Values ===")
    regs = {
        "RegOpMode (0x01)": 0x01,
        "RegLna (0x0C)": 0x0C,
        "RegFifoAddrPtr (0x0D)": 0x0D,
        "RegFifoRxBaseAddr (0x0F)": 0x0F,
        "RegFifoRxCurrentAddr (0x10)": 0x10,
        "RegIrqFlags (0x12)": 0x12,
        "RegRxNbBytes (0x13)": 0x13,
        "RegPktSnrValue (0x19)": 0x19,
        "RegPktRssiValue (0x1A)": 0x1A,
        "RegRssiValue (0x1B)": 0x1B,
        "RegModemConfig1 (0x1D)": 0x1D,
        "RegModemConfig2 (0x1E)": 0x1E,
        "RegSymbTimeout (0x1F)": 0x1F,
        "RegPreamble (0x20-21)": 0x20,
        "RegPayloadLength (0x22)": 0x22,
        "RegHopPeriod (0x24)": 0x24,
        "RegModemConfig3 (0x26)": 0x26,
        "RegDioMapping1 (0x40)": 0x40,
        "RegVersion (0x42)": 0x42
    }
    
    for name, addr in regs.items():
        if name == "RegPreamble (0x20-21)":
            msb = spi_read(0x20)
            lsb = spi_read(0x21)
            print(f"{name}: 0x{msb:02X}{lsb:02X}")
        else:
            val = spi_read(addr)
            if addr in [0x01, 0x12, 0x1D, 0x1E, 0x26, 0x40]:  # Show binary for important registers
                print(f"{name}: 0x{val:02X} (0b{val:08b})")
            else:
                print(f"{name}: 0x{val:02X}")
    print("=============================\n")

def init_lora_simple(implicit=False, payload_len=26):
    """Initialize LoRa with SF7 for higher data rate"""
    reset_module()
    
    # Check module is responding
    version = spi_read(0x42)
    print(f"SX1276 Version: 0x{version:02X}")
    if version != 0x12:
        print("ERROR: Module not detected! Check connections.")
        return False
    
    # Set Sleep mode first (required to change to LoRa mode)
    spi_write(0x01, 0x00)  # FSK/OOK mode, sleep
    time.sleep(0.1)
    
    # Set LoRa mode, sleep
    spi_write(0x01, 0x80)
    time.sleep(0.1)
    
    # Set standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)
    
    # Set frequency 
    set_frequency()
    
    # RegLna: Max gain, boost on
    spi_write(0x0C, 0x23)  # LNA gain set by AGC, boost on
    
    # Configure modem - Simple Mode (SF7)
    if implicit:
        # RegModemConfig1: BW=125kHz, CR=4/5, Implicit header
        spi_write(0x1D, 0x74)
        # Set expected payload length (needed for implicit header mode)
        spi_write(0x22, payload_len)
    else:
        # RegModemConfig1: BW=125kHz, CR=4/5, Explicit header
        spi_write(0x1D, 0x72)
    
    # RegModemConfig2: SF7, normal mode, CRC on
    spi_write(0x1E, 0x74)
    
    # RegModemConfig3: LNA auto gain, No low data rate optimization
    spi_write(0x26, 0x04)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Set FIFO RX base address and pointer
    spi_write(0x0F, 0x00)  # RegFifoRxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr
    
    # Map DIO0 to RxDone (00 in bits 7:6 = 0x00)
    spi_write(0x40, 0x00)
    
    # Unmask RX interrupts
    spi_write(0x11, 0x00)
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Set to RX continuous mode
    spi_write(0x01, 0x85)
    
    header_mode = "Implicit" if implicit else "Explicit"
    print(f"LoRa initialized with SIMPLE settings (SF7, 125kHz BW, {header_mode} header)")
    return True

def init_lora_full(implicit=True, payload_len=26):
    """Initialize LoRa with SF12 for maximum range"""
    reset_module()
    
    # Check module is responding
    version = spi_read(0x42)
    print(f"SX1276 Version: 0x{version:02X}")
    if version != 0x12:
        print("ERROR: Module not detected! Check connections.")
        return False
    
    # Set Sleep mode first (required to change to LoRa mode)
    spi_write(0x01, 0x00)  # FSK/OOK mode, sleep
    time.sleep(0.1)
    
    # Set LoRa mode, sleep
    spi_write(0x01, 0x80)
    time.sleep(0.1)
    
    # Set standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)
    
    # Set frequency
    set_frequency()
    
    # RegLna: Max gain, boost on
    spi_write(0x0C, 0xC3)  # LNA gain G4, boost on for HF
    
    # Configure modem - Full Mode (SF12)
    if implicit:
        # RegModemConfig1: BW=125kHz, CR=4/8, Implicit header
        spi_write(0x1D, 0x74)
        # Set expected payload length (needed for implicit header mode)
        spi_write(0x22, payload_len)
    else:
        # RegModemConfig1: BW=125kHz, CR=4/5, Explicit header
        spi_write(0x1D, 0x72)
    
    # RegModemConfig2: SF12, CRC on, RX continuous
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LNA auto gain, Low data rate optimization (needed for SF11/12)
    spi_write(0x26, 0x0C)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Set FIFO RX base address and pointer
    spi_write(0x0F, 0x00)  # RegFifoRxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr
    
    # Map DIO0 to RxDone (00 in bits 7:6)
    spi_write(0x40, 0x00)
    
    # Unmask RX interrupts
    spi_write(0x11, 0x00)
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Set to RX continuous mode
    spi_write(0x01, 0x85)
    
    header_mode = "Implicit" if implicit else "Explicit"
    print(f"LoRa initialized with FULL settings (SF12, 125kHz BW, {header_mode} header)")
    return True

def init_lora_fhss(payload_len=26):
    """Initialize LoRa with frequency hopping like the GPS tracker"""
    reset_module()
    
    # Check module is responding
    version = spi_read(0x42)
    print(f"SX1276 Version: 0x{version:02X}")
    if version != 0x12:
        print("ERROR: Module not detected! Check connections.")
        return False
    
    # Set Sleep mode first (required to change to LoRa mode)
    spi_write(0x01, 0x00)  # FSK/OOK mode, sleep
    time.sleep(0.1)
    
    # Set LoRa mode, sleep
    spi_write(0x01, 0x80)
    time.sleep(0.1)
    
    # Set standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)
    
    # Set initial frequency to channel 0
    set_frequency(0)
    
    # Map DIO0 to FhssChangeChannel
    spi_write(0x40, 0x00)  # DIO0 = RxDone in Rx mode (00 in bits 7:6)
    
    # Configure modem - Exactly like GPS tracker but for RX
    # RegModemConfig1: BW=125kHz, CR=4/5, Implicit header
    spi_write(0x1D, 0x74)
    
    # Set expected payload length (needed for implicit header mode)
    spi_write(0x22, payload_len)
    
    # RegModemConfig2: SF12, RX continuous mode, CRC on
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LNA auto gain, Low data rate optimization
    spi_write(0x26, 0x0C)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Enable frequency hopping: Hop every 5 symbols to match transmitter
    spi_write(0x24, 5)  # RegHopPeriod
    
    # RegLna: Max gain, boost on
    spi_write(0x0C, 0xC3)  # LNA gain G4, boost on for HF
    
    # Set FIFO RX base address and pointer
    spi_write(0x0F, 0x00)  # RegFifoRxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr
    
    # Unmask RX interrupts
    spi_write(0x11, 0x00)
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Set to RX continuous mode
    spi_write(0x01, 0x85)
    
    print("LoRa initialized with FHSS settings (SF12, 125kHz BW, Implicit header, Freq. Hopping)")
    return True

def clear_terminal():
    """Clear the terminal screen"""
    os.system('cls' if os.name == 'nt' else 'clear')

def draw_rssi_graph(rssi_history, min_rssi=70, max_rssi=140):
    """Draw an ASCII graph of RSSI values in the terminal"""
    # Check if we have data
    if not rssi_history:
        return "No RSSI data yet"
    
    rssi_values = list(rssi_history)
    
    # Calculate the range of values
    graph_range = max_rssi - min_rssi
    if graph_range <= 0:
        graph_range = 1  # Avoid division by zero
    
    # Create graph
    graph_str = []
    graph_str.append(f"RSSI Graph (dBm) - Last {len(rssi_values)} readings - Min: -{min(rssi_values)} Max: -{max(rssi_values)}")
    graph_str.append(f"-{min_rssi} dBm " + "─" * (GRAPH_WIDTH - 20) + f" -{max_rssi} dBm")
    
    # Create rows
    for h in range(GRAPH_HEIGHT):
        row = ""
        threshold = min_rssi + (graph_range * (GRAPH_HEIGHT - h - 1) / GRAPH_HEIGHT)
        
        for val in rssi_values[-GRAPH_WIDTH:]:
            if val >= threshold:
                row += "█"
            else:
                row += " "
        
        # Add y-axis labels every few rows
        if h % 2 == 0:
            row_label = f"-{int(min_rssi + (graph_range * (GRAPH_HEIGHT - h - 1) / GRAPH_HEIGHT))} dBm"
            row = f"{row_label:>10} │{row}"
        else:
            row = " " * 10 + "│" + row
            
        graph_str.append(row)
    
    # Add x-axis
    graph_str.append(" " * 10 + "└" + "─" * min(len(rssi_values), GRAPH_WIDTH))
    
    return "\n".join(graph_str)

def receive_packets(max_time=None, use_fhss=False, expect_implicit=True, payload_len=26, visualize_rssi=True):
    """Listen for incoming packets with timeout option and RSSI visualization"""
    start_time = time.time()
    packet_count = 0
    current_channel = 0
    
    # Initialize RSSI tracking
    rssi_history = deque(maxlen=RSSI_HISTORY_LENGTH)
    last_rssi_update = 0
    
    print(f"Listening for incoming LoRa packets...")
    if max_time:
        print(f"Will stop after {max_time} seconds")
    
    # Ensure we're in RX continuous mode
    current_mode = spi_read(0x01)
    if (current_mode & 0x07) != 0x05:  # Check if not in RX mode
        print(f"Warning: Not in RX mode. Current mode: 0x{current_mode:02X}")
        spi_write(0x01, 0x85)  # Set to RX continuous mode
        time.sleep(0.1)
    
    # Last status print time to avoid spamming
    last_status_time = 0
    
    try:
        while True:
            # Check if we've exceeded the maximum time (if specified)
            current_time = time.time()
            if max_time and (current_time - start_time > max_time):
                print(f"Receive timeout after {max_time} seconds")
                break
            
            # Handle RSSI visualization
            if visualize_rssi and (current_time - last_rssi_update >= RSSI_UPDATE_INTERVAL):
                rssi = spi_read(0x1B)  # Current RSSI
                rssi_history.append(rssi)
                last_rssi_update = current_time
                
                # Draw RSSI graph
                if len(rssi_history) > 1:  # Only draw if we have at least 2 readings
                    clear_terminal()
                    print(draw_rssi_graph(rssi_history))
                    print(f"\nRSSI: -{rssi} dBm | Time: {int(current_time - start_time)}s | Packets: {packet_count}")
                    print("Press Ctrl+C to exit")
            
            # Print status every 5 seconds if not visualizing RSSI
            elif not visualize_rssi and current_time - last_status_time >= 5:
                irq_flags = spi_read(0x12)
                rssi = spi_read(0x1B)  # Current RSSI
                print(f"Still listening... [Time: {int(current_time - start_time)}s, "
                      f"Packets: {packet_count}, RSSI: -{rssi} dBm, "
                      f"IRQ: 0x{irq_flags:02X}, DIO0: {GPIO.input(DIO0)}]")
                last_status_time = current_time
            
            # Handle frequency hopping
            if use_fhss and GPIO.input(DIO0) == 1:
                current_channel = (current_channel + 1) % len(HOP_CHANNELS)
                set_frequency(current_channel)
                print(f"Frequency hopped to channel {current_channel}")
                spi_write(0x12, 0x04)  # Clear FhssChangeChannel flag
            
            # Read IRQ flags directly
            irq_flags = spi_read(0x12)
            
            # Check for RxDone (bit 6) and ValidHeader (bit 4) flags if applicable
            rx_done = irq_flags & 0x40
            timeout = irq_flags & 0x80
            
            if timeout:
                print("RX Timeout detected")
                spi_write(0x12, 0x80)  # Clear timeout flag
            
            if rx_done:  # RxDone flag is set
                # Temporarily pause visualization to show packet info
                if visualize_rssi:
                    clear_terminal()
                
                print(f"\n=== PACKET DETECTED ===")
                dio0_state = GPIO.input(DIO0)
                print(f"DIO0: {dio0_state}, IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
                
                # Check for CRC error (bit 5)
                if irq_flags & 0x20:
                    print("CRC error detected! Packet invalid.")
                    spi_write(0x12, 0xFF)  # Clear flags
                    continue
                
                # Read payload length
                if expect_implicit:
                    # In implicit mode, length is fixed to what we set in RegPayloadLength
                    nb_bytes = spi_read(0x22)
                else:
                    # In explicit mode, length is in RegRxNbBytes
                    nb_bytes = spi_read(0x13)
                
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
                
                # Log RSSI and SNR
                rssi_value = spi_read(0x1A)  # RegPktRssiValue
                raw_snr = spi_read(0x19)     # RegPktSnrValue (signed, divide by 4)
                snr_value = raw_snr if raw_snr < 128 else raw_snr - 256  # Convert to signed value
                
                print(f"RSSI: -{rssi_value} dBm, SNR: {snr_value / 4} dB")
                print("=== END OF PACKET ===\n")
                
                # Clear IRQ flags
                spi_write(0x12, 0xFF)
                
                # Count packet
                packet_count += 1
                
                # If visualizing RSSI, wait for user to acknowledge the packet
                if visualize_rssi:
                    input("Press Enter to continue RSSI visualization...")
                
                # Update last status time to avoid immediate status after packet
                last_status_time = time.time()
            
            # Small delay to prevent CPU hogging
            time.sleep(0.01)
        
    except KeyboardInterrupt:
        print("\nReceive loop terminated by user")
    finally:
        print(f"Receive statistics: Duration: {time.time() - start_time:.1f}s, Packets received: {packet_count}")
        return packet_count

def cleanup():
    spi.close()
    GPIO.cleanup()
    print("Resources released.")

def main():
    parser = argparse.ArgumentParser(description='LoRa SX1276 Receiver')
    parser.add_argument('--mode', choices=['simple', 'full', 'fhss'], default='full',
                        help='Receive mode: simple (SF7), full (SF12), fhss (frequency hopping)')
    parser.add_argument('--header', choices=['implicit', 'explicit'], default='implicit',
                        help='Header mode: implicit or explicit (must match transmitter)')
    parser.add_argument('--payload-length', type=int, default=26,
                        help='Expected payload length for implicit header mode (default: 26)')
    parser.add_argument('--time', type=float, default=0,
                        help='Time to listen in seconds (default: 0, continuous)')
    parser.add_argument('--registers', action='store_true',
                        help='Print register values before starting')
    parser.add_argument('--no-graph', action='store_true',
                        help='Disable RSSI graph visualization')
    
    args = parser.parse_args()
    
    # Convert header mode to bool for implicit
    implicit_header = (args.header == 'implicit')
    
    try:
        # Initialize based on selected mode
        if args.mode == 'simple':
            success = init_lora_simple(implicit=implicit_header, payload_len=args.payload_length)
        elif args.mode == 'fhss':
            success = init_lora_fhss(payload_len=args.payload_length)
        else:  # full mode
            success = init_lora_full(implicit=implicit_header, payload_len=args.payload_length)
        
        if not success:
            print("Failed to initialize LoRa module. Exiting.")
            cleanup()
            sys.exit(1)
        
        # Print register values if requested
        if args.registers:
            print_registers()
        
        # Set listen time (0 means continuous)
        listen_time = args.time if args.time > 0 else None
        
        # Start receiving
        receive_packets(
            max_time=listen_time,
            use_fhss=(args.mode == 'fhss'),
            expect_implicit=implicit_header,
            payload_len=args.payload_length,
            visualize_rssi=not args.no_graph
        )
        
    except KeyboardInterrupt:
        print("\nProgram interrupted by user.")
    finally:
        cleanup()

if __name__ == "__main__":
    main()