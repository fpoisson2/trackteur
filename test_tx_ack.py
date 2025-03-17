import time
import spidev
import RPi.GPIO as GPIO

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

# Frequency settings for 902.2 MHz
FREQ_START = 902200000  # 902.2 MHz
FXTAL = 32000000
FRF_FACTOR = 2**19
frf = int((FREQ_START * FRF_FACTOR) / FXTAL)
msb = (frf >> 16) & 0xFF
mid = (frf >> 8) & 0xFF
lsb = frf & 0xFF

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

def set_frequency():
    spi_write(0x06, msb)  # RegFrfMsb
    spi_write(0x07, mid)  # RegFrfMid
    spi_write(0x08, lsb)  # RegFrfLsb

def init_lora():
    reset_module()
    spi_write(0x01, 0x80)  # Sleep mode, LoRa mode
    time.sleep(0.1)
    set_frequency()
    spi_write(0x1D, 0x78)  # RegModemConfig1: e.g., BW=125 kHz, CR=4/7, explicit header
    spi_write(0x1E, 0xC4)  # RegModemConfig2: e.g., SF7, CRC on
    spi_write(0x26, 0x0C)
    spi_write(0x20, 0x00)  # Preamble length MSB
    spi_write(0x21, 0x08)  # Preamble length LSB: 8 symbols
    spi_write(0x24, 0)     # Disable frequency hopping
    spi_write(0x01, 0x81)  # Standby mode

def send_ack():
    ack_payload = b"ACK"
    spi_write(0x0E, 0x00)  # FIFO TX base address
    spi_write(0x0D, 0x00)  # FIFO pointer
    for byte in ack_payload:
        spi_write(0x00, byte)
    spi_write(0x22, len(ack_payload))  # Payload length
    spi_write(0x12, 0xFF)  # Clear IRQ flags
    spi_write(0x40, 0x00)  # Map DIO0 to TxDone
    spi_write(0x01, 0x83)  # TX mode
    while not (spi_read(0x12) & 0x08):  # Wait for TxDone
        time.sleep(0.01)
    print("ACK sent!")
    spi_write(0x12, 0xFF)  # Clear IRQ flags

# Initialize LoRa module
init_lora()

# Send ACK every 5 seconds
while True:
    send_ack()
    time.sleep(5)