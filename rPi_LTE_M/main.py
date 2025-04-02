#!/usr/bin/env python3
import serial
import time

# ------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------
SERIAL_PORT   = "/dev/ttyAMA0"  # or "/dev/ttyUSB0" etc.
BAUDRATE      = 115200
TIMEOUT       = 2

# Replace with your Traccar server's hostname/IP and port:
TRACCAR_HOST = "10.82.185.13"
TRACCAR_PORT = 5055

# Dummy GPS coordinates to send:
DUMMY_LAT = "46.8139"
DUMMY_LON = "-71.2082"
DEVICE_ID = "212910"  # or your Traccar device ID

def send_at(ser, command, delay=1.0):
    """
    Helper to send an AT command, wait 'delay' seconds, and read response.
    """
    ser.reset_input_buffer()
    ser.write((command + "\r\n").encode())
    time.sleep(delay)
    resp = ser.read_all().decode(errors="ignore")
    print(f">>> {command}")
    print(resp.strip())
    return resp

def main():
    # 1) Open serial
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
        time.sleep(2)
    except Exception as e:
        print("❌ Could not open serial port:", e)
        return

    # 2) Clean up previous sessions
    send_at(ser, "AT+CIPSHUT", 2)

    # 3) Ensure we're attached and have a PDP context active
    #    (Assuming you've already done AT+CFUN=1,1, set APN with AT+CGDCONT, and AT+CNACT=1,"APN")
    #    We'll do a quick check or re-attach if needed (light approach).
    send_at(ser, "AT+CGATT=1", 2)

    # 4) Start TCP connection to your Traccar server
    #    Host + port
    resp = send_at(ser, f'AT+CIPSTART="TCP","{TRACCAR_HOST}",{TRACCAR_PORT}', 8)
    if "CONNECT OK" not in resp:
        print("❌ Failed to connect via CIP. Check coverage or server details.")
        ser.close()
        return

    # 5) Build the HTTP GET string with dummy lat/lon
    #    Minimal HTTP GET request. Adjust Host header if needed.
    http_get = (
        f"GET /?id={DEVICE_ID}&lat={DUMMY_LAT}&lon={DUMMY_LON} HTTP/1.1\r\n"
        f"Host: {TRACCAR_HOST}\r\n"
        "User-Agent: SIM7000\r\n"
        "Connection: close\r\n"
        "\r\n"  # End of headers
    )

    # 6) Send the HTTP GET request
    #    AT+CIPSEND puts us in "send data" mode. We end with Ctrl+Z (ASCII 26).
    send_at(ser, "AT+CIPSEND", 1)
    print(">>> Sending HTTP GET data...")
    ser.write(http_get.encode())  # Write the GET request
    time.sleep(1)

    # Send Ctrl+Z = ASCII 26
    ser.write(bytes([26]))
    time.sleep(2)

    # 7) (Optional) Read any +IPD data from the server response
    #    In basic mode, the response might come as +IPD chunks. We'll just read what's in the buffer:
    resp_data = ser.read_all().decode(errors="ignore")
    print("<<< Server response (raw):")
    print(resp_data.strip())

    # 8) Close the socket
    send_at(ser, "AT+CIPCLOSE", 2)

    # 9) SHUT the IP session
    send_at(ser, "AT+CIPSHUT", 2)

    # Done
    ser.close()
    print("\n✅ Dummy GPS position sent to Traccar via CIP!")

if __name__ == "__main__":
    main()
