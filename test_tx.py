#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys
import argparse

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
    print("Resetting module...")
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def print_registers():
    """Print key register values for debugging"""
    print("\n=== SX1276 Register Values ===")
    regs = {
        "RegOpMode (0x01)": 0x01,
        "RegPaConfig (0x09)": 0x09,
        "RegOcp (0x0B)": 0x0B,
        "RegFifoTxBaseAddr (0x0E)": 0x0E,
        "RegFifoAddrPtr (0x0D)": 0x0D,
        "RegIrqFlags (0x12)": 0x12,
        "RegModemConfig1 (0x1D)": 0x1D,
        "RegModemConfig2 (0x1E)": 0x1E,
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

def init_lora(sf=12, explicit_header=False, frequency_mhz=915):
    """Initialize LoRa with custom settings"""
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
    frf = int((frequency_mhz * 1000000 * (2**19)) / 32000000)
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb
    print(f"Frequency set to {frequency_mhz} MHz")
    
    # Configure modem - Spreading Factor
    if sf == 7:
        sf_val = 0x70  # SF7 (7:4 = 0111)
    elif sf == 8:
        sf_val = 0x80  # SF8 (7:4 = 1000)
    elif sf == 9:
        sf_val = 0x90  # SF9 (7:4 = 1001)
    elif sf == 10:
        sf_val = 0xA0  # SF10 (7:4 = 1010)
    elif sf == 11:
        sf_val = 0xB0  # SF11 (7:4 = 1011)
    else:  # Default to SF12
        sf_val = 0xC0  # SF12 (7:4 = 1100)
        sf = 12
    
    # ModemConfig1: Bandwidth, Coding Rate, Header Mode
    if explicit_header:
        # BW=125kHz (bits 7:6=10), CR=4/5 (bits 5:3=001), Explicit header (bit 2=0)
        spi_write(0x1D, 0xA2)  # 0x72 for SF7-11, 0xA2 for SF12
        print("Header Mode: Explicit")
    else:
        # BW=125kHz (bits 7:6=01), CR=4/8 (bits 5:3=100), Implicit header (bit 2=1)
        spi_write(0x1D, 0x78)  # Same for all SF
        print("Header Mode: Implicit")
    
    # ModemConfig2: Spreading Factor, CRC
    # SF (bits 7:4), normal mode (bit 3=0), CRC on (bit 2=1)
    spi_write(0x1E, sf_val | 0x04)  # CRC enabled
    print(f"Spreading Factor: SF{sf}")
    
    # ModemConfig3: LowDataRateOptimize for SF11/12
    if sf >= 11:
        # LDRO enabled (bit 3=1), AGC auto (bit 2=1)
        spi_write(0x26, 0x0C)
        print("Low Data Rate Optimization: Enabled")
    else:
        # LDRO disabled (bit 3=0), AGC auto (bit 2=1)
        spi_write(0x26, 0x04)
        print("Low Data Rate Optimization: Disabled")
    
    # Set preamble length to 8 symbols
    spi_write(0x20, 0x00)
    spi_write(0x21, 0x08)
    
    # Configure PA for high power
    spi_write(0x09, 0x8F)  # PA_BOOST, max output power
    spi_write(0x4D, 0x87)  # PA_DAC high power mode
    
    # Disable over current protection
    spi_write(0x0B, 0x20)  # Disable OCP

    # Set TX FIFO base address
    spi_write(0x0E, 0x00)
    spi_write(0x0D, 0x00)  # Reset FIFO pointer
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    print("LoRa module initialized")
    return True

def transmit(message, explicit_header=False):
    """Transmit a message and wait for completion"""
    # Convert string to bytes if needed
    if isinstance(message, str):
        payload = message.encode('ascii')
    else:
        payload = message
    
    # Limit payload size (255 bytes max for LoRa)
    if len(payload) > 255:
        payload = payload[:255]
        print(f"WARNING: Payload truncated to 255 bytes")
    
    # Show payload details
    print(f"Transmitting: {message}")
    print(f"Payload length: {len(payload)} bytes")
    
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
    
    while time.time() - start_time < 10:  # 10 second timeout
        # Check IRQ flags for completion
        irq_flags = spi_read(0x12)
        
        # Check mode (might have left TX)
        op_mode = spi_read(0x01)
        tx_mode = (op_mode & 0x07) == 0x03
        
        # Check for TxDone flag (bit 3)
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
    
    # 9. Return success based on TxDone
    return bool(irq_flags & 0x08)

def cleanup():
    spi.close()
    GPIO.cleanup()
    print("Resources released.")

def main():
    parser = argparse.ArgumentParser(description='LoRa SX1276 Test Transmitter')
    parser.add_argument('--sf', type=int, choices=[7, 8, 9, 10, 11, 12], default=12,
                        help='Spreading Factor (7-12, default: 12)')
    parser.add_argument('--header', choices=['implicit', 'explicit'], default='implicit',
                        help='Header mode (default: implicit)')
    parser.add_argument('--freq', type=float, default=915.0,
                        help='Frequency in MHz (default: 915.0)')
    parser.add_argument('--message', type=str, default='Test Message',
                        help='Message to transmit (default: "Test Message")')
    parser.add_argument('--count', type=int, default=3,
                        help='Number of transmissions (default: 3)')
    parser.add_argument('--interval', type=float, default=5.0,
                        help='Interval between transmissions in seconds (default: 5.0)')
    parser.add_argument('--registers', action='store_true',
                        help='Print register values before starting')
    
    args = parser.parse_args()
    
    try:
        # Initialize with the specified settings
        explicit_header = (args.header == 'explicit')
        success = init_lora(sf=args.sf, explicit_header=explicit_header, frequency_mhz=args.freq)
        
        if not success:
            print("Failed to initialize LoRa module. Exiting.")
            cleanup()
            sys.exit(1)
        
        # Print register values if requested
        if args.registers:
            print_registers()
        
        # Perform transmissions
        successes = 0
        for i in range(args.count):
            print(f"\n=== Transmission {i+1}/{args.count} ===")
            msg = f"{args.message} #{i+1}"
            if transmit(msg, explicit_header=explicit_header):
                successes += 1
            
            if i < args.count - 1:
                print(f"Waiting {args.interval} seconds before next transmission...")
                time.sleep(args.interval)
        
        print(f"\nTransmission summary: {successes}/{args.count} successful")
        
    except KeyboardInterrupt:
        print("\nProgram interrupted by user.")
    finally:
        cleanup()

if __name__ == "__main__":
    main()