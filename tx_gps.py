#!/usr/bin/env python3
import serial
import pynmea2
import spidev
import RPi.GPIO as GPIO
import time
import sys
import struct

# GPS Serial Port Configuration
GPS_PORT = "/dev/ttyAMA0"
BAUD_RATE = 9600

# Pin configuration (BCM numbering)
RESET = 17   # Reset pin
NSS   = 25   # SPI Chip Select pin
DIO0  = 4    # DIO0 pin for interrupts

# Global variables
last_tx_time = 0
TX_INTERVAL = 10  # Seconds between transmissions

# Debug flag - set to True for detailed debugging
DEBUG = True

# Frequency hopping configuration
FREQ_START = 902200000  # 902.2 MHz
FREQ_STEP = 200000      # 200 kHz (reduced step to fit more channels)
FXTAL = 32000000
FRF_FACTOR = 2**19

# Create channel list - identical between TX and RX
HOP_CHANNELS = []
for i in range(128):  # 128 channels
    freq = FREQ_START + i * FREQ_STEP
    frf = int((freq * FRF_FACTOR) / FXTAL)
    msb = (frf >> 16) & 0xFF
    mid = (frf >> 8) & 0xFF
    lsb = frf & 0xFF
    HOP_CHANNELS.append((msb, mid, lsb))

current_channel = 0

# Setup GPIO in BCM mode
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(RESET, GPIO.OUT)
GPIO.setup(NSS, GPIO.OUT)
GPIO.setup(DIO0, GPIO.IN)

# SPI initialization
spi = spidev.SpiDev()
spi.open(0, 0)
spi.max_speed_hz = 5000000
spi.mode = 0b00

def cleanup():
    """Clean up GPIO and SPI before exiting"""
    spi.close()
    GPIO.cleanup()
    print("Cleanup completed")

def spi_write(addr, value):
    """Write a byte to a register"""
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr | 0x80, value])
    GPIO.output(NSS, GPIO.HIGH)

def spi_read(addr):
    """Read a byte from a register"""
    GPIO.output(NSS, GPIO.LOW)
    spi.xfer2([addr & 0x7F])
    result = spi.xfer2([0x00])[0]
    GPIO.output(NSS, GPIO.HIGH)
    return result

def read_register_block(start_addr, count):
    """Read a block of registers and return as a dictionary"""
    result = {}
    for i in range(count):
        addr = start_addr + i
        value = spi_read(addr)
        result[addr] = value
    return result

def print_registers(registers):
    """Print registers in a formatted way"""
    print("===== REGISTER DUMP =====")
    for addr, value in sorted(registers.items()):
        reg_name = ""
        if addr == 0x01: reg_name = "RegOpMode"
        elif addr == 0x06: reg_name = "RegFrfMsb"
        elif addr == 0x07: reg_name = "RegFrfMid"
        elif addr == 0x08: reg_name = "RegFrfLsb"
        elif addr == 0x0D: reg_name = "RegFifoAddrPtr"
        elif addr == 0x0E: reg_name = "RegFifoTxBaseAddr"
        elif addr == 0x0F: reg_name = "RegFifoRxBaseAddr"
        elif addr == 0x10: reg_name = "RegFifoRxCurrentAddr"
        elif addr == 0x12: reg_name = "RegIrqFlags"
        elif addr == 0x13: reg_name = "RegRxNbBytes"
        elif addr == 0x1C: reg_name = "RegHopChannel"
        elif addr == 0x1D: reg_name = "RegModemConfig1"
        elif addr == 0x1E: reg_name = "RegModemConfig2"
        elif addr == 0x24: reg_name = "RegHopPeriod"
        elif addr == 0x26: reg_name = "RegModemConfig3"
        elif addr == 0x40: reg_name = "RegDioMapping1"
        elif addr == 0x42: reg_name = "RegVersion"
        
        print(f"  Reg 0x{addr:02X} {reg_name:15}: 0x{value:02X}")
    print("=========================")

def reset_module():
    """Reset the SX1276 module"""
    print("Resetting module...")
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def set_frequency(channel_idx):
    """Set the frequency based on channel index"""
    global HOP_CHANNELS
    msb, mid, lsb = HOP_CHANNELS[channel_idx]
    spi_write(0x06, msb)  # RegFrfMsb
    spi_write(0x07, mid)  # RegFrfMid
    spi_write(0x08, lsb)  # RegFrfLsb
    freq_mhz = (FREQ_START + channel_idx * FREQ_STEP) / 1000000
    if DEBUG:
        print(f"Setting frequency to channel {channel_idx}: {freq_mhz:.3f} MHz (MSB:{msb:02X} MID:{mid:02X} LSB:{lsb:02X})")

def dump_tx_status():
    """Print current status of key registers during TX"""
    reg_op_mode = spi_read(0x01)  # RegOpMode
    reg_irq_flags = spi_read(0x12)  # RegIrqFlags
    reg_fifo_addr_ptr = spi_read(0x0D)  # RegFifoAddrPtr
    reg_fifo_tx_base_addr = spi_read(0x0E)  # RegFifoTxBaseAddr
    reg_hop_channel = spi_read(0x1C)  # RegHopChannel
    
    mode_str = "Unknown"
    mode = reg_op_mode & 0x07
    if mode == 0: mode_str = "Sleep"
    elif mode == 1: mode_str = "Standby"
    elif mode == 2: mode_str = "FSTX"
    elif mode == 3: mode_str = "TX"
    elif mode == 4: mode_str = "FSRX"
    elif mode == 5: mode_str = "RX"
    
    channel_num = reg_hop_channel & 0x3F
    freq_mhz = (FREQ_START + channel_num * FREQ_STEP) / 1000000
    pll_lock = "Yes" if reg_hop_channel & 0x80 else "No"
    crc_on_payload = "Yes" if reg_hop_channel & 0x40 else "No"
    
    print(f"TX Status: OpMode=0x{reg_op_mode:02X} ({mode_str}), IRQ=0x{reg_irq_flags:02X}")
    print(f"  FifoPtr=0x{reg_fifo_addr_ptr:02X}, TxBase=0x{reg_fifo_tx_base_addr:02X}")
    print(f"  HopChannel=0x{reg_hop_channel:02X}")
    print(f"  Current Channel: {channel_num} ({freq_mhz:.3f} MHz)")
    print(f"  PLL Lock: {pll_lock}, CRC on Payload: {crc_on_payload}")
    
    # Print detailed IRQ flags
    if reg_irq_flags > 0:
        print("  IRQ Flags:")
        if reg_irq_flags & 0x80: print("    CadDetected")
        if reg_irq_flags & 0x40: print("    FhssChangeChannel")
        if reg_irq_flags & 0x20: print("    CadDone")
        if reg_irq_flags & 0x10: print("    TxTimeout")
        if reg_irq_flags & 0x08: print("    ValidHeader")
        if reg_irq_flags & 0x04: print("    PayloadCrcError")
        if reg_irq_flags & 0x02: print("    RxDone")
        if reg_irq_flags & 0x01: print("    TxDone")

def init_module():
    """Initialize the LoRa module"""
    reset_module()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected! Check wiring and power.")
        cleanup()
        sys.exit(1)
    
    # Put module in Sleep mode with LoRa enabled
    spi_write(0x01, 0x80)
    time.sleep(0.1)
    
    # Explicitly disable all interrupts first
    spi_write(0x11, 0x00)  # RegIrqFlagsMask - unmask all interrupts
    
    # Clear any existing IRQ flags
    spi_write(0x12, 0xFF)
    
    # Map DIO0 to FhssChangeChannel in TX mode
    spi_write(0x40, 0x40)  # 0x40 = DIO0 mapped to FhssChangeChannel
    
    # Set initial frequency to channel 0 (902.2 MHz)
    set_frequency(0)
    
    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header
    # 0111 001 1 = 0x73  (changed to explicit header mode)
    spi_write(0x1D, 0x73)
    
    # RegModemConfig2: SF10, CRC on, TX timeout MSB
    # 1010 1 1 00 = 0xA4  (changed to SF10 for faster transmission)
    spi_write(0x1E, 0xA4)
    
    # RegModemConfig3: LDRO on, AGC on
    # 0000 1 1 00 = 0x0C
    spi_write(0x26, 0x0C)
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Enable frequency hopping: Hop every 10 symbols
    spi_write(0x24, 10)  # RegHopPeriod
    
    # Set PA configuration: +20 dBm with PA_BOOST
    spi_write(0x09, 0x8F)
    
    # Set FIFO TX base address and pointer
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)
    
    # Put module in standby mode
    spi_write(0x01, 0x81)
    time.sleep(0.1)
    
    # Dump all important registers for debugging
    print("Module initialized with the following register values:")
    important_regs = read_register_block(0x01, 0x50)
    print_registers(important_regs)

def spi_tx(payload):
    """Transmit data with frequency hopping"""
    global current_channel
    print(f"\n----- STARTING TRANSMISSION -----")
    print(f"Raw TX Payload ({len(payload)} bytes): {payload.hex()}")
    
    # Put module back in standby mode to ensure clean state
    spi_write(0x01, 0x81)
    time.sleep(0.1)
    
    # Reset IRQ flags
    spi_write(0x12, 0xFF)
    
    # Ensure DIO0 is mapped to FhssChangeChannel
    spi_write(0x40, 0x40)  # Force setting DIO0 to FhssChangeChannel
    dio_mapping = spi_read(0x40)
    print(f"DIO0 mapping: 0x{dio_mapping:02X} (should be 0x40 for FhssChangeChannel)")
    
    # Verify hop period setting
    hop_period = spi_read(0x24)
    print(f"Hop period: {hop_period} symbols")
    
    # Reset FIFO pointer and prepare for TX
    spi_write(0x0E, 0x00)  # Set FIFO TX base address to 0
    spi_write(0x0D, 0x00)  # Reset FIFO pointer to 0
    
    # Write binary payload into FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    # Set payload length
    spi_write(0x22, len(payload))
    
    # Set initial channel
    current_channel = 0
    set_frequency(current_channel)
    
    # Print pre-tx status
    print("Pre-TX Status:")
    dump_tx_status()
    
    # Switch to TX mode
    print("Switching to TX mode and starting transmission...")
    spi_write(0x01, 0x83)  # 0x83 = LoRa mode + TX mode
    
    # Wait for transmission to complete with frequency hopping
    tx_start_time = time.time()
    last_status_time = 0
    hop_count = 0
    tx_done = False
    
    # Maximum time to wait for transmission (timeout)
    max_tx_time = 15  # seconds
    
    while (time.time() - tx_start_time) < max_tx_time and not tx_done:
        current_time = time.time()
        
        # Monitor mode and restore if needed
        mode = spi_read(0x01)
        if (mode & 0x07) != 0x03:  # If not in TX mode (0x03)
            print(f"WARNING: Module not in TX mode! Current mode: 0x{mode:02X}")
            print("Restoring TX mode...")
            spi_write(0x01, 0x83)  # Restore TX mode
            time.sleep(0.05)
        
        # Print status every second
        if int(current_time) > int(last_status_time):
            time_elapsed = current_time - tx_start_time
            print(f"\nTX in progress - Time elapsed: {time_elapsed:.1f}s, Hops: {hop_count}")
            dump_tx_status()
            last_status_time = current_time
        
        # Check if DIO0 is high, indicating an interrupt
        if GPIO.input(DIO0) == 1:
            irq_flags = spi_read(0x12)
            print(f"DIO0 interrupt! IRQ flags: 0x{irq_flags:02X}")
            
            # Handle FhssChangeChannel interrupt
            if irq_flags & 0x40:
                hop_count += 1
                # Read current channel from module
                reg_hop_channel = spi_read(0x1C)
                current_channel = reg_hop_channel & 0x3F
                
                # Calculate frequency in MHz
                freq_mhz = (FREQ_START + current_channel * FREQ_STEP) / 1000000
                
                print(f"Freq hop #{hop_count}: Channel {current_channel}, Freq: {freq_mhz:.3f} MHz")
                
                # Clear FhssChangeChannel flag
                spi_write(0x12, 0x40)
            
            # Check for TxDone
            if irq_flags & 0x01:
                tx_done = True
                tx_time = time.time() - tx_start_time
                print(f"Transmission completed in {tx_time:.2f} seconds after {hop_count} hops")
                
                # Clear TxDone flag
                spi_write(0x12, 0x01)
            
            # Handle unexpected flags
            if irq_flags & 0x0A:  # RxDone or ValidHeader 
                print("WARNING: Unexpected RxDone/ValidHeader flags detected")
                # Just clear these flags and continue
                spi_write(0x12, 0x0A)
            
            # Clear any other flags that might be set
            remaining_flags = spi_read(0x12)
            if remaining_flags != 0:
                spi_write(0x12, remaining_flags)
        
        # Check for TxDone via polling as a backup
        irq_flags = spi_read(0x12)
        if irq_flags & 0x01:
            tx_done = True
            tx_time = time.time() - tx_start_time
            print(f"TxDone detected via polling after {tx_time:.2f} seconds and {hop_count} hops")
            
            # Clear TxDone flag
            spi_write(0x12, 0x01)
        
        # Small delay to prevent CPU hogging
        time.sleep(0.01)
    
    # Check if we timed out
    if not tx_done:
        print(f"TX timeout after {max_tx_time} seconds")
    
    # Return to standby mode
    spi_write(0x01, 0x81)
    print("Returned to standby mode")
    
    # Clear any remaining IRQ flags
    irq_flags = spi_read(0x12)
    if irq_flags != 0:
        print(f"Clearing remaining IRQ flags: 0x{irq_flags:02X}")
        spi_write(0x12, 0xFF)
    
    # Final status
    print("Final status after transmission:")
    dump_tx_status()
    print("----- TRANSMISSION COMPLETE -----\n")

def parse_gps(data):
    """Parse GPS NMEA data and transmit if interval elapsed"""
    global last_tx_time
    
    current_time = time.time()
    if last_tx_time > 0 and (current_time - last_tx_time) < TX_INTERVAL:
        return
    
    if data.startswith("$GNGGA") or data.startswith("$GPGGA"):
        try:
            msg = pynmea2.parse(data)
            
            # Check if we have a valid position fix
            if msg.gps_qual == 0:
                print("No GPS fix available yet. Waiting...")
                return
            
            lat = int(msg.latitude * 1_000_000)
            lon = int(msg.longitude * 1_000_000)
            alt = int(float(msg.altitude)) if msg.altitude else 0
            timestamp = int(time.time())
            
            # Pack the data in binary format: lat(4), lon(4), alt(2), timestamp(4)
            payload = struct.pack(">iiHI", lat, lon, alt, timestamp)
            
            print(f"Preparing to transmit: lat={msg.latitude}, lon={msg.longitude}, alt={msg.altitude}m, ts={timestamp}")
            
            # Transmit the data
            spi_tx(payload)
            
            # Update last transmission time
            last_tx_time = time.time()
            print(f"Next transmission in {TX_INTERVAL} seconds")
        
        except pynmea2.ParseError as e:
            print(f"Parse error: {e}")
        except Exception as e:
            print(f"Error in parse_gps: {e}")
            # Reinitialize module on error
            init_module()

def read_gps():
    """Main GPS reading loop"""
    try:
        with serial.Serial(GPS_PORT, BAUD_RATE, timeout=1) as ser:
            print("Reading GPS data...")
            while True:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    if DEBUG and (line.startswith("$GNGGA") or line.startswith("$GPGGA")):
                        print(f"GPS NMEA data: {line}")
                    parse_gps(line)
                time.sleep(0.01)
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        cleanup()
        sys.exit(1)

def main():
    """Main program entry point"""
    try:
        print("\n===== LoRa GPS Tracker - Transmitter =====")
        print("Initializing LoRa module...")
        init_module()
        
        print(f"Starting GPS reading loop with {TX_INTERVAL}s transmission interval")
        read_gps()
    except KeyboardInterrupt:
        print("Interrupted by user. Exiting.")
    except Exception as e:
        print(f"Unexpected error: {e}")
    finally:
        cleanup()
        sys.exit(0)

if __name__ == "__main__":
    main()