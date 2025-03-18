#!/usr/bin/env python3
import spidev
import RPi.GPIO as GPIO
import time
import sys

# Pin definitions (BCM mode)
RESET = 17  # Reset pin
NSS = 25    # SPI Chip Select pin
DIO0 = 4    # DIO0 pin for RxDone interrupt

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
    print(f"RegLna (0x0C): 0x{spi_read(0x0C):02X}")
    print(f"RegFifoRxBaseAddr (0x0F): 0x{spi_read(0x0F):02X}")
    print(f"RegFifoAddrPtr (0x0D): 0x{spi_read(0x0D):02X}")
    print(f"RegIrqFlags (0x12): 0x{spi_read(0x12):02X} ({bin(spi_read(0x12))})")
    print(f"RegRxNbBytes (0x13): 0x{spi_read(0x13):02X}")
    print(f"RegDioMapping1 (0x40): 0x{spi_read(0x40):02X}")
    print(f"RegDioMapping2 (0x41): 0x{spi_read(0x41):02X}")
    print("========================\n")

def read_extended_registers():
    """Extended register reading for debugging"""
    print("\n=== Extended Register Values ===")
    print(f"RegOpMode (0x01): 0x{spi_read(0x01):02X} ({bin(spi_read(0x01))})")
    print(f"RegFrf (0x06,07,08): 0x{spi_read(0x06):02X}{spi_read(0x07):02X}{spi_read(0x08):02X}")
    print(f"RegLna (0x0C): 0x{spi_read(0x0C):02X} ({bin(spi_read(0x0C))})")
    print(f"RegIrqFlags (0x12): 0x{spi_read(0x12):02X} ({bin(spi_read(0x12))})")
    print(f"RegIrqFlagsMask (0x11): 0x{spi_read(0x11):02X} ({bin(spi_read(0x11))})")
    print(f"RegRxNbBytes (0x13): 0x{spi_read(0x13):02X}")
    print(f"RegFifoRxCurrentAddr (0x10): 0x{spi_read(0x10):02X}")
    print(f"RegRssiValue (0x1A): 0x{spi_read(0x1A):02X}")
    print(f"RegPktSnrValue (0x19): 0x{spi_read(0x19):02X}")
    print(f"RegModemConfig1 (0x1D): 0x{spi_read(0x1D):02X} ({bin(spi_read(0x1D))})")
    print(f"RegModemConfig2 (0x1E): 0x{spi_read(0x1E):02X} ({bin(spi_read(0x1E))})")
    print(f"RegModemConfig3 (0x26): 0x{spi_read(0x26):02X} ({bin(spi_read(0x26))})")
    print(f"RegPreamble (0x20,21): 0x{spi_read(0x20):02X}{spi_read(0x21):02X}")
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
    spi_write(0x0F, 0x00)  # Reset RX base address
    spi_write(0x0D, 0x00)  # Reset FIFO pointer
    
    print("Reset complete.")

def init_lora_simple():
    """Initialize LoRa with simplified settings (SF7)"""
    reset_thoroughly()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    print("Initializing with SIMPLE settings (SF7)...")
    
    # Set to Sleep mode with LoRa enabled
    spi_write(0x01, 0x80)  # Sleep mode first
    time.sleep(0.1)
    
    # To standby
    spi_write(0x01, 0x81)  # LoRa mode, standby
    time.sleep(0.1)

    # Set frequency to 915 MHz
    frf = int(915000000 / 61.03515625)  # Fstep = 32e6 / 2^19 ≈ 61.035 Hz
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb

    # RegLna: Max gain, boost on
    spi_write(0x0C, 0x23)  # LNA gain set by AGC, boost on

    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header mode
    spi_write(0x1D, 0x72)  # BW=125kHz, CR=4/5, Explicit header mode

    # RegModemConfig2: SF7, CRC on
    spi_write(0x1E, 0x74)  # SF=7, RX single mode (not continuous), CRC on

    # RegModemConfig3: AGC on
    spi_write(0x26, 0x04)  # Low data rate optimize OFF, AGC ON

    # Preamble length: 8 symbols
    spi_write(0x20, 0x00)  # MSB
    spi_write(0x21, 0x08)  # LSB

    # Set FIFO RX base address and pointer
    spi_write(0x0F, 0x00)  # RegFifoRxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr

    # Map DIO0 to RxDone (00 in bits 7:6 when in rx mode)
    spi_write(0x40, 0x00)  # DIO0 = RxDone in Rx mode

    # Unmask all interrupts
    spi_write(0x11, 0x00)  # No masks

    # Clear IRQ flags
    spi_write(0x12, 0xFF)  # Clear all flags

    # Set to RX continuous mode
    spi_write(0x01, 0x85)  # LoRa mode, RX continuous

    print("LoRa module initialized with SIMPLE settings (SF7)")
    read_extended_registers()

def init_lora_full():
    """Initialize LoRa with full settings (SF12)"""
    reset_thoroughly()
    version = spi_read(0x42)
    print(f"SX1276 Version: {hex(version)}")
    if version != 0x12:
        print("Module not detected, check connections.")
        cleanup()
        sys.exit(1)

    print("Initializing with FULL settings (SF12)...")
    
    # Set to Sleep mode with LoRa enabled
    spi_write(0x01, 0x80)  # Sleep mode first
    time.sleep(0.1)
    
    # To standby
    spi_write(0x01, 0x81)  # LoRa mode, standby
    time.sleep(0.1)

    # Set frequency to 915 MHz
    frf = int(915000000 / 61.03515625)  # Fstep = 32e6 / 2^19 ≈ 61.035 Hz
    spi_write(0x06, (frf >> 16) & 0xFF)  # RegFrfMsb
    spi_write(0x07, (frf >> 8) & 0xFF)   # RegFrfMid
    spi_write(0x08, frf & 0xFF)          # RegFrfLsb

    # RegLna: Max gain, boost on
    spi_write(0x0C, 0xC3)  # LNA gain G4 (max), boost on for HF

    # RegModemConfig1: BW 125 kHz, CR 4/5, Explicit Header, CRC on
    spi_write(0x1D, 0xA3)  # BW=125 kHz, CR=4/5, Explicit header, CRC on

    # RegModemConfig2: SF12, Continuous mode, CRC on
    spi_write(0x1E, 0xC4)  # SF=12, RX continuous mode, CRC on

    # RegModemConfig3: LowDataRateOptimize on, AGC on
    spi_write(0x26, 0x0C)  # LDRO=1, AGC=1

    # Preamble length: 8 symbols
    spi_write(0x20, 0x00)  # MSB
    spi_write(0x21, 0x08)  # LSB

    # Set FIFO RX base address and pointer
    spi_write(0x0F, 0x00)  # RegFifoRxBaseAddr
    spi_write(0x0D, 0x00)  # RegFifoAddrPtr

    # Map DIO0 to RxDone (00 in bits 7:6)
    spi_write(0x40, 0x00)  # DIO0 = RxDone in rx mode

    # Unmask all interrupts
    spi_write(0x11, 0x00)  # No masks

    # Clear IRQ flags
    spi_write(0x12, 0xFF)  # Clear all flags

    # Set to RX continuous mode
    spi_write(0x01, 0x85)  # LoRa mode, RX continuous

    print("LoRa module initialized with FULL settings (SF12)")
    read_extended_registers()

def receive_loop(max_time=None):
    """Listen for incoming packets with timeout option"""
    start_time = time.time()
    packet_count = 0
    print("Listening for incoming LoRa packets...")
    
    # Ensure we're in RX mode
    current_mode = spi_read(0x01)
    if (current_mode & 0x07) != 0x05:  # Check if not in RX mode
        print(f"Warning: Not in RX mode. Current mode: 0x{current_mode:02X}")
        spi_write(0x01, 0x85)  # Set to RX continuous mode
        time.sleep(0.1)
    
    # Check DIO mapping for RxDone
    dio_mapping = spi_read(0x40)
    if (dio_mapping & 0xC0) != 0x00:  # Check bits 7:6 for RxDone mapping
        print(f"Warning: DIO0 not mapped to RxDone. Current mapping: 0x{dio_mapping:02X}")
        spi_write(0x40, (dio_mapping & 0x3F))  # Set bits 7:6 to 00 for RxDone
    
    try:
        while True:
            # Check if we've exceeded the maximum time (if specified)
            if max_time and (time.time() - start_time > max_time):
                print(f"Receive timeout after {max_time} seconds")
                break
            
            # Check DIO0 pin (for interrupt-driven detection)
            if GPIO.input(DIO0) == 1:
                print("DIO0 pin high - potential packet")
            
            # Read IRQ flags directly
            irq_flags = spi_read(0x12)
            
            # Check for RxDone (bit 6) and ValidHeader (bit 4) flags
            if irq_flags & 0x40:  # RxDone flag is set
                print(f"\n=== PACKET DETECTED ===")
                dio0_state = GPIO.input(DIO0)
                print(f"DIO0: {dio0_state}, IRQ Flags: {bin(irq_flags)} (0x{irq_flags:02x})")
                
                # Read payload length
                nb_bytes = spi_read(0x13)  # RegRxNbBytes
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
                
                # Check for CRC error
                if irq_flags & 0x20:
                    print("CRC error detected")
                else:
                    print("CRC check passed")
                
                # Log RSSI and SNR
                rssi_value = spi_read(0x1A)  # RegRssiValue
                raw_snr = spi_read(0x19)     # RegPktSnrValue (signed, divide by 4)
                snr_value = raw_snr if raw_snr < 128 else raw_snr - 256  # Convert to signed value
                
                print(f"RSSI: -{rssi_value} dBm, SNR: {snr_value / 4} dB")
                print(f"Modem Status: 0x{spi_read(0x18):02X} ({bin(spi_read(0x18))})")
                print("=== END OF PACKET ===\n")
                
                # Clear IRQ flags
                spi_write(0x12, 0xFF)
                
                # Count packet
                packet_count += 1
                
                # Reset FIFO address pointer for next packet
                spi_write(0x0D, 0x00)
            
            # Periodically print status if no packets received
            elif (int(time.time()) % 5 == 0) and (time.time() - start_time > 5):
                print(f"Still listening... [Time: {int(time.time() - start_time)}s, Packets: {packet_count}]")
                print(f"Current IRQ flags: 0x{irq_flags:02X} ({bin(irq_flags)}), DIO0: {GPIO.input(DIO0)}")
                print(f"OpMode: 0x{spi_read(0x01):02X}, Modem Status: 0x{spi_read(0x18):02X}")
                time.sleep(1)  # Avoid repeating every millisecond of this second
            
            time.sleep(0.01)  # Small delay to prevent CPU hogging
        
    except KeyboardInterrupt:
        print("\nReceive loop terminated by user")
    finally:
        print(f"Receive statistics: Duration: {time.time() - start_time:.1f}s, Packets received: {packet_count}")

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
    
    # Test 5: Initialize with simple settings (SF7)
    print("\n==== INITIALIZING WITH SIMPLE SETTINGS (SF7) ====\n")
    init_lora_simple()
    
    # Test 6: Short receive test with simple settings
    print("\n==== SIMPLE RECEIVE TEST (10 seconds) ====\n")
    receive_loop(max_time=10)
    
    # Test 7: Initialize with full settings (SF12)
    print("\n==== INITIALIZING WITH FULL SETTINGS (SF12) ====\n")
    init_lora_full()
    
    # Test 8: Short receive test with full settings
    print("\n==== FULL SETTINGS RECEIVE TEST (10 seconds) ====\n")
    receive_loop(max_time=10)
    
    print("\n==== DIAGNOSTIC TEST SEQUENCE COMPLETED ====\n")

if __name__ == "__main__":
    try:
        # Run diagnostic mode if requested
        if len(sys.argv) > 1 and sys.argv[1] == '--diagnostic':
            run_diagnostic_test()
        else:
            # Normal operation: choose one init mode
            print("Choose initialization mode:")
            print("1. Simple settings (SF7, better for high data rate)")
            print("2. Full settings (SF12, better for long range)")
            choice = input("Enter choice (1 or 2): ").strip()
            
            if choice == '2':
                init_lora_full()
            else:
                init_lora_simple()
            
            # How long to listen
            listen_time = input("How long to listen in seconds (empty for continuous): ").strip()
            
            if listen_time:
                receive_loop(max_time=float(listen_time))
            else:
                receive_loop()  # Continuous listening until interrupted
            
    except KeyboardInterrupt:
        print("\nTerminating...")
    finally:
        cleanup()
        sys.exit(0)