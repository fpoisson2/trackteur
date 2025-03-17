import serial
import pynmea2

# GPS Serial Port Configuration
GPS_PORT = "/dev/ttyAMA0"  # Confirmed UART for Dragino GPS HAT
BAUD_RATE = 9600           # Default baud rate for Dragino GPS

def parse_gps(data):
    """Extracts latitude, longitude, and other relevant data from NMEA sentences."""
    if data.startswith("$GNGGA") or data.startswith("$GPGGA"):  # Look for valid GPS sentences
        try:
            msg = pynmea2.parse(data)
            latitude = msg.latitude
            longitude = msg.longitude
            altitude = msg.altitude
            satellites = msg.num_sats
            fix_quality = msg.gps_qual

            print(f"Latitude: {latitude}, Longitude: {longitude}")
            print(f"Altitude: {altitude}m, Satellites: {satellites}, Fix Quality: {fix_quality}")

        except pynmea2.ParseError as e:
            print(f"Parse error: {e}")

def read_gps():
    """Reads GPS data from the serial port and parses it."""
    try:
        with serial.Serial(GPS_PORT, BAUD_RATE, timeout=1) as ser:
            print("Reading GPS data...")
            while True:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    parse_gps(line)
    except serial.SerialException as e:
        print(f"Serial error: {e}")

if __name__ == "__main__":
    read_gps()
