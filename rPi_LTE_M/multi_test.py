#!/usr/bin/env python3
import serial
import time
import re

# -------------------------------
# Configuration
# -------------------------------
SERIAL_PORT = "/dev/ttyAMA0"  # or "/dev/ttyUSB0"
BAUDRATE = 115200
TIMEOUT = 2

# APN for Emnify
APN = "em"

# Known test domain & IP for example.com
TEST_DOMAIN = "example.com"
TEST_DOMAIN_IP = "93.184.216.34"  # Resolved IP of example.com

# Optional: If your firmware supports ping
# e.g., AT+SNPING or AT+QPING
SUPPORTS_PING = True

# -------------------------------
# Utility
# -------------------------------
def send_at(ser, command, delay=1.5):
    """
    Sends an AT command, waits, and returns the raw response as a string.
    """
    ser.reset_input_buffer()
    ser.write((command + "\r\n").encode())
    time.sleep(delay)
    resp = ser.read_all().decode(errors="ignore")
    print(f">>> {command}")
    print(resp.strip())
    return resp

def at_http_get(ser, url, cid=1, delay=10):
    """
    Perform an HTTP GET to 'url' using the internal SIM7000G HTTP stack.
    Returns the +HTTPACTION result (e.g. '0,200,<length>') or the entire response if not found.
    """
    # Clean up any leftover
    send_at(ser, "AT+HTTPTERM", 1)

    # Init HTTP
    send_at(ser, "AT+HTTPINIT", 1)

    # Select PDP context
    send_at(ser, f'AT+HTTPPARA="CID",{cid}', 1)

    # Set URL
    send_at(ser, f'AT+HTTPPARA="URL","{url}"', 1)

    # Perform GET
    resp = send_at(ser, "AT+HTTPACTION=0", delay)

    # Look for +HTTPACTION: 0,<status>,<len>
    match = re.search(r'\+HTTPACTION:\s*0,(\d+),(\d+)', resp)
    if match:
        code = match.group(1)
        length = match.group(2)
        # Optionally read the body
        send_at(ser, "AT+HTTPREAD", 1.5)
        # Terminate
        send_at(ser, "AT+HTTPTERM", 1)
        return code, length

    # If not found in immediate 'resp', we may read more
    # but let's just close & return everything
    send_at(ser, "AT+HTTPREAD", 1.5)
    send_at(ser, "AT+HTTPTERM", 1)
    return resp, None

def main():
    # 1) Open Serial
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
        time.sleep(2)
    except Exception as e:
        print("❌ Could not open serial port:", e)
        return

    # 2) Reset IP stack and turn radio off
    send_at(ser, "AT+CIPSHUT", 2)
    send_at(ser, "AT+CFUN=0", 2)

    # 3) Force LTE-only + Cat-M1
    send_at(ser, "AT+CNMP=38")  # 38=LTE only
    send_at(ser, "AT+CMNB=1")   # 1=CAT-M1 only

    # 4) Full reboot to apply
    send_at(ser, "AT+CFUN=1,1", 2)
    print("Waiting 5s for module to reboot...")
    time.sleep(5)

    # 5) Basic checks
    send_at(ser, "AT")
    send_at(ser, "AT+CPIN?")
    csq_resp = send_at(ser, "AT+CSQ")     # Check signal
    creg_resp = send_at(ser, "AT+CREG?")  # 2G/3G
    cereg_resp = send_at(ser, "AT+CEREG?")# 4G (LTE)

    # 6) PDP context
    send_at(ser, f'AT+CGDCONT=1,"IP","{APN}"', 1.5)
    send_at(ser, "AT+CGATT=1", 3)
    # Activate context
    send_at(ser, f'AT+CNACT=1,"{APN}"', 5)
    pdp_resp = send_at(ser, "AT+CNACT?", 1.5)

    # 7) HTTP GET - domain name
    print("\n=== TEST 1: HTTP GET with domain name ===")
    code1, length1 = at_http_get(ser, f"http://{TEST_DOMAIN}", cid=1, delay=10)
    if isinstance(code1, str) and code1.isdigit():
        code1 = int(code1)
    print(f"Result: +HTTPACTION: 0,{code1},{length1}")

    # 8) HTTP GET - direct IP
    print("\n=== TEST 2: HTTP GET with direct IP ===")
    code2, length2 = at_http_get(ser, f"http://{TEST_DOMAIN_IP}", cid=1, delay=10)
    if isinstance(code2, str) and code2.isdigit():
        code2 = int(code2)
    print(f"Result: +HTTPACTION: 0,{code2},{length2}")

    # 9) Optional ping test
    # Some firmware have: AT+SNPING="8.8.8.8" or AT+QPING=1,"8.8.8.8",4,32,1000
    # If your firmware doesn't support it, you'll see ERROR.
    if SUPPORTS_PING:
        print("\n=== TEST 3: Ping 8.8.8.8 (if supported) ===")
        ping_resp = send_at(ser, 'AT+SNPING="8.8.8.8",4,32,1000', 8)
        # If your firmware doesn't have SNPING, try QPING:
        # ping_resp = send_at(ser, 'AT+QPING=1,"8.8.8.8",4,32,1000', 8)
        print("Ping response:", ping_resp)

    # Close serial
    ser.close()

    # 10) Print some hints
    print("\n=== FINAL RESULTS / DIAGNOSTICS ===")

    # Parse CSQ
    csq_match = re.search(r'\+CSQ:\s*(\d+),(\d+)', csq_resp)
    if csq_match:
        rssi = int(csq_match.group(1))
        if rssi == 99:
            print("Signal: 99 → unknown / searching. Might have coverage issues.")
        else:
            print(f"Signal: {rssi} → {rssi*2 - 113} dBm approx.")
    else:
        print("Could not parse +CSQ.")

    # CEREG parse
    # +CEREG: 0,5 => registered roaming
    # +CEREG: 0,1 => registered home
    # +CEREG: 0,2 => searching
    # +CEREG: 0,3 => denied
    creg_match = re.search(r'CEREG:\s*\d,(\d)', cereg_resp)
    if creg_match:
        reg_stat = creg_match.group(1)
        if reg_stat in ["1","5"]:
            print("LTE Registration status: OK")
        elif reg_stat == "2":
            print("LTE Registration status: Searching, might need more time.")
        else:
            print(f"LTE Registration status: {reg_stat} → Denied or not registered.")
    else:
        print("Could not parse +CEREG?")

    # PDP parse
    # +CNACT: 1,"xxx.xxx.xxx.xxx"
    if "100." in pdp_resp or "+CNACT:" in pdp_resp:
        print("PDP context is active. IP assigned.")
    else:
        print("PDP context not confirmed active. Might cause 601 errors.")

    # HTTP test results
    # code1, code2 come from +HTTPACTION: 0,<status>,<len>
    # 200 => OK, 601 => network error, 603 => DNS error

    print("\nHTTP GET domain name => ", end="")
    if isinstance(code1, int):
        print(f"Status code = {code1}")
        if code1 == 200:
            print("SUCCESS - Domain name resolution & data path are good.")
        elif code1 == 601:
            print("NETWORK ERROR - possibly no route or blocked by carrier/policy.")
        elif code1 == 603:
            print("DNS ERROR - domain name could not be resolved.")
        else:
            print("Other HTTP error code. Check docs.")
    else:
        print(f"Unexpected response: {code1}")

    print("HTTP GET direct IP => ", end="")
    if isinstance(code2, int):
        print(f"Status code = {code2}")
        if code2 == 200:
            print("SUCCESS - Reached the server by IP.")
        elif code2 == 601:
            print("NETWORK ERROR - no route or blocked. Possibly not a DNS problem.")
        else:
            print("Other HTTP error code. Check docs.")
    else:
        print(f"Unexpected response: {code2}")

    print("\nIf both tests yield 601, traffic is likely blocked by your carrier or setup.")
    print("If domain test yields 603 but IP yields 200, then it's purely a DNS issue.")
    print("Done.")

if __name__ == "__main__":
    main()
