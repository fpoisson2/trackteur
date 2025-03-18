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

def test_spi_connection():
    """Test SPI connection by writing and reading back a value"""
    print("\n=== SPI Connection Test ===")
    
    # Test various registers
    test_registers = [0x0E, 0x0D, 0x1D, 0x1E]
    for reg in test_registers:
        # Save original value
        original = spi_read(reg)
        
        # Write test value (if not in sleep mode, avoid writing to OpMode)
        test_val = 0x42 if reg != 0x01 else original
        spi_write(reg, test_val)
        
        # Read back and verify
        readback = spi_read(reg)
        print(f"Register 0x{reg:02X}: Wrote 0x{test_val:02X}, Read 0x{readback:02X}, Match: {test_val == readback}")
        
        # Restore original value
        spi_write(reg, original)
    
    print("========================\n")

def test_dio0():
    """Test DIO0 pin functionality"""
    print("\n=== DIO0 Pin Test ===")
    
    # Save original state
    original_config = GPIO.gpio_function(DIO0)
    
    # Test reading DIO0 normally
    print(f"Initial DIO0 state: {GPIO.input(DIO0)}")
    
    # Test with pullup/pulldown if available
    try:
        GPIO.setup(DIO0, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
        time.sleep(0.1)
        print(f"DIO0 with pulldown: {GPIO.input(DIO0)}")
        
        GPIO.setup(DIO0, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        time.sleep(0.1)
        print(f"DIO0 with pullup: {GPIO.input(DIO0)}")
    except:
        print("Pull up/down test failed - might not be supported or pin in use")
    
    # Restore to normal input
    GPIO.setup(DIO0, GPIO.IN)
    print("====================\n")

def read_registers():
    """Read and print key registers for debugging"""
    print("\n=== Basic Register Values ===")
    print(f"RegOpMode (0x01): 0x{spi_read(0x01):02X} ({bin(spi_read(0x01))})")
    print(f"RegPaConfig (0x09): 0x{spi_read(0x09):02X}")
    print(f"RegPaRamp (0x0A): 0x{spi_read(0x0A):02X}")
    print(f"RegOcp (0x0B): 0x{spi_read(0x0B):02X}")
    print(f"RegFifoTxBaseAddr (0x0E): 0x{spi_read(0x0E):02X}")
    print(f"RegFifoAddrPtr (0x0D): 0x{spi_read(0x0D):02X}")
    print(f"RegIrqFlags (0x12): 0x{spi_read(0x12):02X} ({bin(spi_read(0x12))})")
    print(f"RegDioMapping1 (0x40): 0x{spi_read(0x40):02X}")
    print(f"RegDioMapping2 (0x41): 0x{spi_read(0x41):02X}")
    print(f"RegPaDac (0x4D): 0x{spi_read(0x4D):02X}")
    print("========================\n")

def read_extended_registers():
    """Extended register reading for debugging"""
    print("\n=== Extended Register Values ===")
    print(f"RegOpMode (0x01): 0x{spi_read(0x01):02X} ({bin(spi_read(0x01))})")
    print(f"RegFrf (0x06,07,08): 0x{spi_read(0x06):02X}{spi_read(0x07):02X}{spi_read(0x08):02X}")
    print(f"RegIrqFlags (0x12): 0x{spi_read(0x12):02X} ({bin(spi_read(0x12))})")
    print(f"RegIrqFlagsMask (0x11): 0x{spi_read(0x11):02X} ({bin(spi_read(0x11))})")
    print(f"RegModemConfig1 (0x1D): 0x{spi_read(0x1D):02X} ({bin(spi_read(0x1D))})")
    print(f"RegModemConfig2 (0x1E): 0x{spi_read(0x1E):02X} ({bin(spi_read(0x1E))})")
    print(f"RegModemConfig3 (0x26): 0x{spi_read(0x26):02X} ({bin(spi_read(0x26))})")
    print(f"RegSymbTimeout (0x1F): 0x{spi_read(0x1F):02X}")
    print(f"RegPreamble (0x20,21): 0x{spi_read(0x20):02X}{spi_read(0x21):02X}")
    print(f"RegPayloadLength (0x22): 0x{spi_read(0x22):02X}")
    print(f"RegVersion (0x42): 0x{spi_read(0x42):02X}")
    print("===========================\n")

def reset_module():
    """Basic module reset via reset pin"""
    print("Basic reset via RESET pin...")
    GPIO.output(RESET, GPIO.LOW)
    time.sleep(0.1)
    GPIO.output(RESET, GPIO.HIGH)
    time.sleep(0.1)

def reset_thoroughly():
    """Complete reset sequence with mode transitions"""
    print("Performing thorough reset sequence...")
    # Hardware reset
    reset_module()
    
    # Software reset sequence
    print("Transitioning through all modes...")
    # To FSK mode first
    spi_write(0x01, 0x00)  # FSK/OOK mode, sleep
    time.sleep(0.1)
    
    # To LoRa sleep
    spi_write(0x01, 0x80)  # LoRa mode, sleep
    time.sleep(0.1)
    
    # To LoRa standby
    spi_write(0x01, 0x81)  # LoRa mode, standby
    time.sleep(0.1)
    
    # Clear all IRQ flags
    spi_write(0x12, 0xFF)
    
    # Reset FIFO pointers
    spi_write(0x0E, 0x00)  # Reset TX base address
    spi_write(0x0D, 0x00)  # Reset FIFO pointer
    
    print("Reset complete.")

def init_lora_simple():
    """Initialize LoRa with simplified settings (lower SF, etc.)"""
    reset_thoroughly()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    print("Initializing with SIMPLE settings...")
    
    # Set to Sleep mode
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

    # Configure PA with moderate power
    spi_write(0x09, 0x8C)  # PA_BOOST pin, +14dBm
    
    # Turn off Over Current Protection
    spi_write(0x0B, 0x20)  # RegOcp: OCP disabled

    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header, CRC on
    spi_write(0x1D, 0x72)  # BW=125 kHz, CR=4/5, Explicit header

    # RegModemConfig2: SF7 (lowest), CRC on
    spi_write(0x1E, 0x74)  # SF=7, Normal mode, CRC on

    # RegModemConfig3: LNA enabled, AGC on
    spi_write(0x26, 0x04)  # LowDataRateOptimize off, AGC on
    
    # Set preamble length: 8 symbols
    spi_write(0x20, 0x00)  # Preamble length MSB
    spi_write(0x21, 0x08)  # Preamble length LSB

    # Set FIFO TX base address
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr = 0
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr = 0

    # Map DIO0 to TxDone (01 in bits 7:6 = 0x40)
    spi_write(0x40, 0x40)  # RegDioMapping1: TxDone on DIO0
    spi_write(0x41, 0x00)  # RegDioMapping2: defaults

    # Unmask all interrupts
    spi_write(0x11, 0x00)  # No masks
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)  # RegIrqFlags: Clear all IRQ flags
    
    print("LoRa module initialized with SIMPLE settings")
    read_extended_registers()

def init_lora_full():
    """Initialize LoRa with full power settings"""
    reset_thoroughly()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    print("Initializing with FULL POWER settings...")
    
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

    # RegModemConfig2: SF12, Normal mode, CRC on
    spi_write(0x1E, 0xC4)  # SF=12, Normal mode, CRC on

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
    spi_write(0x41, 0x00)  # RegDioMapping2: defaults

    # Unmask all interrupts
    spi_write(0x11, 0x00)  # No masks
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)  # RegIrqFlags: Clear all IRQ flags
    
    print("LoRa module initialized with FULL POWER settings")
    read_extended_registers()

def transmit_message(message, timeout=5):
    payload = message.encode('ascii')
    if len(payload) > 255:  # LoRa packet max size
        payload = payload[:255]
    
    print(f"Transmitting: {message} ({len(payload)} bytes)")
    
    # Ensure the module is in standby mode
    spi_write(0x01, 0x81)  # LoRa mode, standby
    
    # Clear IRQ flags
    spi_write(0x12, 0xFF)
    
    # Reset FIFO pointer to the TX base address
    spi_write(0x0E, 0x00)  # RegFifoTxBaseAddr = 0
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr = 0
    
    # Write payload to FIFO
    for byte in payload:
        spi_write(0x00, byte)
    
    # Set payload length
    spi_write(0x22, len(payload))  # RegPayloadLength
    
    # Start transmission
    spi_write(0x01, 0x83)  # LoRa mode, TX
    
    print("Transmission started - waiting for completion")
    print(f"Initial IRQ flags: 0x{spi_read(0x12):02X} ({bin(spi_read(0x12))})")
    print(f"Initial DIO0 state: {GPIO.input(DIO0)}")
    
    # Wait for transmission to complete
    start_time = time.time()
    while time.time() - start_time < timeout:
        irq_flags = spi_read(0x12)
        dio0_state = GPIO.input(DIO0)
        op_mode = spi_read(0x01)
        
        # Check if TxDone flag (bit 3) is set
        if irq_flags & 0x08:  # TxDone flag detected
            print(f"Transmission complete! IRQ flags: 0x{irq_flags:02X} ({bin(irq_flags)}), DIO0: {dio0_state}")
            print(f"Final OpMode: 0x{op_mode:02X} ({bin(op_mode)})")
            break
        
        # Check if manually returned to standby (should not happen normally)
        if (op_mode & 0x07) != 0x03:  # No longer in TX mode
            print(f"Warning: Device left TX mode unexpectedly. OpMode: 0x{op_mode:02X} ({bin(op_mode)})")
            break
        
        time.sleep(0.1)
        # Print periodic status every 0.5 seconds
        if int((time.time() - start_time) * 10) % 5 == 0:
            print(f"Waiting... IRQ flags: 0x{irq_flags:02X} ({bin(irq_flags)}), DIO0: {dio0_state}, OpMode: 0x{op_mode:02X}")
    
    # Check if timed out
    if time.time() - start_time >= timeout:
        print(f"TX timeout after {timeout}s. Final status:")
        print(f"IRQ flags: 0x{spi_read(0x12):02X} ({bin(spi_read(0x12))})")
        print(f"OpMode: 0x{spi_read(0x01):02X} ({bin(spi_read(0x01))})")
        print(f"DIO0: {GPIO.input(DIO0)}")
    
    # Clear IRQ flags and return to standby
    spi_write(0x12, 0xFF)
    spi_write(0x01, 0x81)  # Back to standby
    
    # Final register check
    print("Status after transmission attempt:")
    read_extended_registers()

def cleanup():
    spi.close()
    GPIO.cleanup()

def run_diagnostic_test():
    """Run a complete diagnostic test sequence"""
    print("\n==== STARTING DIAGNOSTIC TEST SEQUENCE ====\n")
    
    # Test 1: SPI connection
    test_spi_connection()
    
    # Test 2: DIO pin functionality
    test_dio0()
    
    # Test 3: Basic register reading
    read_registers()
    
    # Test 4: Extended register reading
    read_extended_registers()
    
    # Test 5: Initialize with simple settings
    print("\n==== INITIALIZING WITH SIMPLE SETTINGS ====\n")
    init_lora_simple()
    
    # Test 6: Transmit short message with simple settings
    print("\n==== SIMPLE TRANSMISSION TEST ====\n")
    transmit_message("Test message with SIMPLE settings", timeout=3)
    
    # Test 7: Initialize with full power settings
    print("\n==== INITIALIZING WITH FULL POWER SETTINGS ====\n")
    init_lora_full()
    
    # Test 8: Transmit short message with full power settings
    print("\n==== FULL POWER TRANSMISSION TEST ====\n")
    transmit_message("Test message with FULL POWER settings", timeout=3)
    
    print("\n==== DIAGNOSTIC TEST SEQUENCE COMPLETED ====\n")

if __name__ == "__main__":
    try:
        # Run diagnostic mode if requested
        if len(sys.argv) > 1 and sys.argv[1] == '--diagnostic':
            run_diagnostic_test()
        else:
            # Normal operation: choose one init mode
            print("Choose initialization mode:")
            print("1. Simple settings (SF7, +14dBm)")
            print("2. Full power settings (SF12, +20dBm)")
            choice = input("Enter choice (1 or 2): ").strip()
            
            if choice == '2':
                init_lora_full()
            else:
                init_lora_simple()
            
            message = "Hello, SX1276!"
            counter = 0
            
            # How many messages to send
            num_messages = int(input("How many messages to send (default 3): ") or "3")
            
            # Delay between messages
            delay = float(input("Delay between messages in seconds (default 5): ") or "5")
            
            for i in range(num_messages):
                transmit_message(f"{message} #{counter}")
                counter += 1
                if i < num_messages - 1:  # Don't delay after the last message
                    print(f"Waiting {delay} seconds before next transmission...")
                    time.sleep(delay)
            
            print("All messages sent. Exiting...")
            
    except KeyboardInterrupt:
        print("\nTerminating...")
    finally:
        cleanup()
        sys.exit(0)