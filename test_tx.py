#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import argparse

# Pin definitions (BCM mode)
RESET = 17  # Reset pin
NSS = 25    # SPI Chip Select pin
DIO0 = 4    # DIO0 pin (we'll use polling instead of relying on this interrupt)

# Frequency hopping configuration (optional)
FREQ_START = 902200000  # 902.2 MHz (US 915 band)
FREQ_STEP = 400000      # 400 kHz
FXTAL = 32000000        # 32 MHz crystal
FRF_FACTOR = 2**19

# Generate frequency hopping channels
HOP_CHANNELS = []
for i in range(8):  # Using 8 channels for simplicity
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
        "RegPaConfig (0x09)": 0x09,
        "RegOcp (0x0B)": 0x0B,
        "RegLna (0x0C)": 0x0C,
        "RegFifoAddrPtr (0x0D)": 0x0D,
        "RegFifoTxBaseAddr (0x0E)": 0x0E,
        "RegIrqFlags (0x12)": 0x12,
        "RegModemConfig1 (0x1D)": 0x1D,
        "RegModemConfig2 (0x1E)": 0x1E,
        "RegSymbTimeout (0x1F)": 0x1F,
        "RegPreamble (0x20-21)": 0x20,
        "RegPayloadLength (0x22)": 0x22,
        "RegHopPeriod (0x24)": 0x24,
        "RegModemConfig3 (0x26)": 0x26,
        "RegDioMapping1 (0x40)": 0x40,
        "RegVersion (0x42)": 0x42,
        "RegPaDac (0x4D)": 0x4D
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

def init_lora_simple(implicit=False):
    """Initialize LoRa with SF7 for higher data rate and lower power"""
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
    
    # Configure modem - Simple Mode (SF7)
    if implicit:
        # RegModemConfig1: BW=125kHz, CR=4/5, Implicit header
        # BW (bits 7-4): 0111 (125kHz)
        # CR (bits 3-1): 001 (4/5)
        # Implicit header (bit 0): 1
        spi_write(0x1D, 0x72 | 0x01)  # 0x73
    else:
        # RegModemConfig1: BW=125kHz, CR=4/5, Explicit header
        # BW (bits 7-4): 0111 (125kHz)
        # CR (bits 3-1): 001 (4/5)
        # Explicit header (bit 0): 0
        spi_write(0x1D, 0x72)  # Already 0x72
    
    # RegModemConfig2: SF7, normal mode, CRC on
    spi_write(0x1E, 0x74)
    
    # RegModemConfig3: LNA auto gain, No low data rate optimization
    spi_write(0x26, 0x04)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Set moderate power level with PA_BOOST
    spi_write(0x09, 0x8C)  # PA_BOOST, 14 dBm
    
    # Disable over current protection
    spi_write(0x0B, 0x20)  # Disable OCP

    # Set TX FIFO base address
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)  # Reset FIFO pointer
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    header_mode = "Implicit" if implicit else "Explicit"
    print(f"LoRa initialized with SIMPLE settings (SF7, 125kHz BW, {header_mode} header, 14dBm)")
    return True

def init_lora_full(implicit=True):
    """Initialize LoRa with SF12 for maximum range and high power"""
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
    
    # Configure modem - Full Mode (SF12)
    if implicit:
        # RegModemConfig1: BW=125kHz, CR=4/5, Implicit header
        # BW (bits 7-4): 0111 (125kHz)
        # CR (bits 3-1): 001 (4/5)
        # Implicit header (bit 0): 1
        spi_write(0x1D, 0x72 | 0x01)  # 0x73
        print("Setting implicit header mode (RegModemConfig1 = 0x73)")
    else:
        # RegModemConfig1: BW=125kHz, CR=4/5, Explicit header
        # BW (bits 7-4): 0111 (125kHz)
        # CR (bits 3-1): 001 (4/5)
        # Explicit header (bit 0): 0
        spi_write(0x1D, 0x72)  # Already 0x72
        print("Setting explicit header mode (RegModemConfig1 = 0x72)")
    
    # Verify the value was written correctly
    mc1_val = spi_read(0x1D)
    print(f"Verified RegModemConfig1 = 0x{mc1_val:02X} (expected: 0x{0x73 if implicit else 0x72:02X})")
    
    # RegModemConfig2: SF12, normal mode, CRC on
    # SF (bits 7-4): 1100 (SF12)
    # TxContinuousMode (bit 3): 0 (normal mode)
    # RxPayloadCrcOn (bit 2): 1 (CRC enabled)
    # SymbTimeout (bits 1-0): 00 (used with bits from RegSymbTimeout)
    spi_write(0x1E, 0xC4)
    print(f"Setting SF12, normal mode, CRC on (RegModemConfig2 = 0xC4)")
    
    # RegModemConfig3: LNA auto gain, Low data rate optimization (needed for SF11/12)
    # Reserved (bits 7-4): 0000
    # LowDataRateOptimize (bit 3): 1 (enabled - essential for SF11/SF12)
    # AgcAutoOn (bit 2): 1 (LNA gain set by AGC)
    spi_write(0x26, 0x0C)
    print(f"Setting LNA auto gain, Low data rate optimization (RegModemConfig3 = 0x0C)")
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Set maximum power level with PA_BOOST
    spi_write(0x09, 0x8F)  # PA_BOOST, 17 dBm (15+2)
    
    # Set PA ramp-up time 40us
    spi_write(0x0A, 0x08)  # RegPaRamp: 40us
    
    # Disable over current protection
    spi_write(0x0B, 0x20)  # Disable OCP
    
    # Enable high power mode for +20dBm
    spi_write(0x4D, 0x87)  # RegPaDac: high power mode
    
    # Set TX FIFO base address
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)  # Reset FIFO pointer
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    header_mode = "Implicit" if implicit else "Explicit"
    print(f"LoRa initialized with FULL settings (SF12, 125kHz BW, {header_mode} header, 20dBm)")
    return True

def init_lora_fhss(implicit=True):
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
    spi_write(0x40, 0x40)  # DIO0 = FhssChangeChannel (bit 7:6 = 01)
    
    # Configure modem - Exactly like GPS tracker
    if implicit:
        # RegModemConfig1: BW=125kHz, CR=4/5, Implicit header
        # BW (bits 7-4): 0111 (125kHz)
        # CR (bits 3-1): 001 (4/5)
        # Implicit header (bit 0): 1
        spi_write(0x1D, 0x72 | 0x01)  # 0x73
    else:
        # RegModemConfig1: BW=125kHz, CR=4/5, Explicit header
        # BW (bits 7-4): 0111 (125kHz)
        # CR (bits 3-1): 001 (4/5)
        # Explicit header (bit 0): 0
        spi_write(0x1D, 0x72)  # Already 0x72
    
    # RegModemConfig2: SF12, normal mode, CRC on
    spi_write(0x1E, 0xC4)
    
    # RegModemConfig3: LNA auto gain, Low data rate optimization 
    spi_write(0x26, 0x0C)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Enable frequency hopping: Hop every 5 symbols (~327.7 ms at 62.5 kHz)
    spi_write(0x24, 5)  # RegHopPeriod
    
    # Set maximum power level with PA_BOOST
    spi_write(0x09, 0x8F)  # PA_BOOST, 17 dBm
    
    # Disable over current protection
    spi_write(0x0B, 0x20)  # Disable OCP
    
    # Enable high power mode for +20dBm
    spi_write(0x4D, 0x87)  # RegPaDac: high power mode
    
    # Set TX FIFO base address
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)  # Reset FIFO pointer
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    header_mode = "Implicit" if implicit else "Explicit"
    print(f"LoRa initialized with FHSS settings (SF12, 125kHz BW, {header_mode} header, 20dBm, Freq. Hopping)")
    return True

def transmit(payload, use_fhss=False):
    """Transmit a message and wait for completion using polling"""
    if isinstance(payload, str):
        payload = payload.encode('ascii')
    
    # Limit payload size (255 bytes max for LoRa)
    if len(payload) > 255:
        payload = payload[:255]
        print(f"WARNING: Payload truncated to 255 bytes")
    
    print(f"Transmitting payload: {len(payload)} bytes")
    if len(payload) < 32:  # Only print content for small payloads
        try:
            print(f"Content: {payload.decode('ascii')}")
        except:
            print(f"Content (hex): {payload.hex()}")
    
    # 1. Put in standby mode
    spi_write(0x01, 0x81)
    
    # 2. Reset FIFO pointer
    spi_write(0x0D, 0x00)
    
    # 3. Write payload to FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    # 4. Set payload length
    spi_write(0x22, len(payload))
    
    # 5. Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # 6. Start transmission
    spi_write(0x01, 0x83)  # LoRa mode, TX
    
    print("Transmission started...")
    print(f"Initial IRQ flags: 0x{spi_read(0x12):02X}")
    
    # 7. Wait for transmission completion
    start_time = time.time()
    current_channel = 0
    
    while time.time() - start_time < 10:  # 10 second timeout
        # Check IRQ flags for completion
        irq_flags = spi_read(0x12)
        
        # Check mode (might have left TX)
        op_mode = spi_read(0x01)
        tx_mode = (op_mode & 0x07) == 0x03
        
        # Handle frequency hopping if enabled
        if use_fhss and GPIO.input(DIO0) == 1:
            current_channel = (current_channel + 1) % len(HOP_CHANNELS)
            set_frequency(current_channel)
            print(f"Frequency hopped to channel {current_channel}")
            spi_write(0x12, 0x04)  # Clear FhssChangeChannel flag
        
        # Check for TxDone flag
        if irq_flags & 0x08:
            print(f"Transmission complete! Time: {time.time() - start_time:.2f}s")
            print(f"Final IRQ flags: 0x{irq_flags:02X}")
            break
        
        # Check if not in TX mode anymore
        if not tx_mode:
            print(f"Device left TX mode. OpMode: 0x{op_mode:02X}")
            break
        
        # Periodic status update
        if int((time.time() - start_time) * 2) % 2 == 0:
            print(f"TX in progress... IRQ flags: 0x{irq_flags:02X}, OpMode: 0x{op_mode:02X}")
            time.sleep(0.5)  # Avoid too many messages
            
    # Check if timed out
    if time.time() - start_time >= 10:
        print("TX timeout after 10 seconds")
    
    # 8. Clear IRQ flags and return to standby
    spi_write(0x12, 0xFF)
    spi_write(0x01, 0x81)
    
    # 9. Print final register status
    print_registers()

def cleanup():
    spi.close()
    GPIO.cleanup()
    print("Resources released.")

def main():
    parser = argparse.ArgumentParser(description='LoRa SX1276 Transmitter')
    parser.add_argument('--mode', choices=['simple', 'full', 'fhss'], default='full',
                        help='Transmission mode: simple (SF7), full (SF12), fhss (frequency hopping)')
    parser.add_argument('--header', choices=['implicit', 'explicit'], default='implicit',
                        help='Header mode: implicit or explicit (must match receiver)')
    parser.add_argument('--message', type=str, default='Hello!',
                        help='Message to transmit (default: Hello from LoRa SX1276!)')
    parser.add_argument('--count', type=int, default=3,
                        help='Number of transmissions (default: 3)')
    parser.add_argument('--interval', type=float, default=5.0,
                        help='Interval between transmissions in seconds (default: 5.0)')
    parser.add_argument('--registers', action='store_true',
                        help='Print register values before starting')
    
    args = parser.parse_args()
    
    # Convert header mode to bool for implicit
    implicit_header = (args.header == 'implicit')
    
    try:
        # Initialize based on selected mode
        if args.mode == 'simple':
            success = init_lora_simple(implicit=implicit_header)
        elif args.mode == 'fhss':
            success = init_lora_fhss(implicit=implicit_header)
        else:  # full mode
            success = init_lora_full(implicit=implicit_header)
        
        if not success:
            print("Failed to initialize LoRa module. Exiting.")
            cleanup()
            sys.exit(1)
        
        # Print register values if requested
        if args.registers:
            print_registers()
        
        # Perform transmissions
        for i in range(args.count):
            print(f"\n=== Transmission {i+1}/{args.count} ===")
            msg = f"{args.message} #{i+1}"
            transmit(msg, use_fhss=(args.mode == 'fhss'))
            
            if i < args.count - 1:
                print(f"Waiting {args.interval} seconds before next transmission...")
                time.sleep(args.interval)
        
        print("\nAll transmissions completed.")
        
    except KeyboardInterrupt:
        print("\nProgram interrupted by user.")
    finally:
        cleanup()

if __name__ == "__main__":
    main()