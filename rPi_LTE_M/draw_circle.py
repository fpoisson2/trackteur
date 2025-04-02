#!/usr/bin/env python3
import serial
import time
import math
from datetime import datetime, timedelta

SERIAL_PORT  = "/dev/ttyAMA0"
BAUDRATE     = 115200
TIMEOUT      = 2

TRACCAR_HOST = "trackteur.ve2fpd.com"
TRACCAR_PORT = 5055
DEVICE_ID    = "212910"

POINT_COUNT  = 24  # e.g., 24 circle points
CENTER_LAT   = 46.8139
CENTER_LON   = -71.2082
RADIUS_DEG   = 0.01

def send_at(ser, command, delay=1.0):
    ser.reset_input_buffer()
    ser.write((command + "\r\n").encode())
    time.sleep(delay)
    return ser.read_all().decode(errors="ignore")

def main():
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    time.sleep(2)

    # Clean IP
    send_at(ser, "AT+CIPSHUT", 2)

    # (Optional) ensure PDP context is up, e.g.:
    # send_at(ser, "AT+CGATT=1", 3)
    # send_at(ser, 'AT+CNACT=1,"em"', 5)

    # Connect CIP once
    resp = send_at(ser, f'AT+CIPSTART="TCP","{TRACCAR_HOST}",{TRACCAR_PORT}', 8)
    print(resp)
    if "CONNECT OK" not in resp:
        print("Failed CIP connect.")
        ser.close()
        return

    # Generate circle points
    yesterday = datetime.now() - timedelta(days=1)
    for i in range(POINT_COUNT):
        # Timestamp
        dt = datetime(yesterday.year, yesterday.month, yesterday.day, i, 0, 0)
        ts = dt.strftime("%Y-%m-%d%%20%H:%M:%S")  # URL-encoded space

        # Circle math
        angle = 2*math.pi*i/POINT_COUNT
        lat = CENTER_LAT + RADIUS_DEG*math.cos(angle)
        lon = CENTER_LON + RADIUS_DEG*math.sin(angle)

        # Build GET
        get_request = (
            f"GET /?id={DEVICE_ID}&lat={lat:.6f}&lon={lon:.6f}&timestamp={ts} HTTP/1.1\r\n"
            f"Host: {TRACCAR_HOST}\r\n"
            "User-Agent: SIM7000\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        )

        # CIPSend
        resp = send_at(ser, "AT+CIPSEND", 1)
        print(resp)
        if ">" not in resp:
            print("No prompt for data, aborting.")
            break

        # Send GET, then Ctrl+Z
        ser.write(get_request.encode())
        time.sleep(1)
        ser.write(bytes([26]))  # Ctrl+Z
        time.sleep(2)

        # Read server response
        data = ser.read_all().decode(errors="ignore")
        print("Server response:\n", data)

        # If the server uses "Connection: close" after each request, CIP might say "CLOSED".
        # If that happens, you can reconnect CIP if you want to continue.

    # Done - close
    send_at(ser, "AT+CIPCLOSE", 2)
    send_at(ser, "AT+CIPSHUT", 2)
    ser.close()
    print("Done batch in one CIP session (multiple CIPSEND).")

if __name__ == "__main__":
    main()
