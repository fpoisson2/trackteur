#!/usr/bin/env python3
import serial
import time
import re

# -----------------------------------------
# Configuration
# -----------------------------------------
SERIAL_PORT = "/dev/ttyAMA0"  # or "/dev/ttyUSB0"
BAUDRATE    = 115200
TIMEOUT     = 2

APN = "em"  # Emnify APN

# Public test server (Cloudflare DNS IP + port 80)
TEST_PUBLIC_IP   = "1.1.1.1"
TEST_PUBLIC_PORT = 80

# Private server
TEST_PRIVATE_IP   = "10.82.185.13"
TEST_PRIVATE_PORT = 5055

def send_at(ser, command, delay=1.5):
    """
    Sends an AT command, waits, and returns the full response.
    """
    ser.reset_input_buffer()
    ser.write((command + "\r\n").encode())
    time.sleep(delay)
    resp = ser.read_all().decode(errors="ignore")
    print(f">>> {command}")
    print(resp.strip())
    return resp

# ---------------------------------------------------------
# Part A: CIP approach (AT+CIPSTART) - older library
# ---------------------------------------------------------
def try_cip_tcp(ser, ip, port, delay=8):
    """
    Attempt a TCP connection with the CIP library:
      AT+CIPSTART="TCP","<ip>",<port>
    Returns:
      "CONNECT OK" if success,
      "FAIL" or "ERROR" or None if it fails or is unsupported.
    """
    # CIP mode can require CIPSHUT first
    send_at(ser, "AT+CIPSHUT", 2)

    # Try to start:
    resp = send_at(ser, f'AT+CIPSTART="TCP","{ip}",{port}', delay)
    if "CONNECT OK" in resp:
        return "CONNECT OK"
    elif "ERROR" in resp or "FAIL" in resp:
        return "ERROR"
    else:
        # Maybe the result is asynchronous. Read more if needed.
        extra = ser.read_all().decode(errors="ignore")
        print(extra.strip())
        if "CONNECT OK" in extra:
            return "CONNECT OK"
        if "FAIL" in extra or "ERROR" in extra:
            return "ERROR"
    return None

# ---------------------------------------------------------
# Part B: CSOC approach (AT+CSOC, AT+CSOCON) - modern library
# ---------------------------------------------------------
def try_csoc_tcp(ser, ip, port, delay=5):
    """
    Create a TCP socket with AT+CSOC,
    connect with AT+CSOCON.

    Returns:
      True if connect success,
      False or None if fails/unsupported
    """
    # Create a socket:
    resp = send_at(ser, 'AT+CSOC=1,2,1')  # domain=1=IPv4, type=2=STREAM, protocol=1=TCP
    # Expect: +CSOC: <socketID>
    match = re.search(r"\+CSOC:\s*(\d+)", resp)
    if not match:
        # "ERROR" or not supported
        return None
    sock_id = match.group(1)

    # Now connect
    resp = send_at(ser, f'AT+CSOCON={sock_id},1,"{ip}",{port}', delay)
    # Expect: +CSOCON: <id>,<err>,<localPort>,<remotePort>
    # err=0 => success
    con_match = re.search(r"\+CSOCON:\s*(\d+),(\d+)", resp)
    if con_match:
        err_code = con_match.group(2)
        if err_code == "0":
            # Success
            return True

    # Possibly more data is asynchronous
    extra = ser.read_all().decode(errors="ignore")
    print(extra.strip())
    con_match2 = re.search(r"\+CSOCON:\s*(\d+),(\d+)", extra)
    if con_match2:
        err_code = con_match2.group(2)
        if err_code == "0":
            return True

    return False

def main():
    # 1) Open serial
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
        time.sleep(2)
    except Exception as e:
        print("❌ Could not open serial:", e)
        return

    print("=== STEP 1: Configure for LTE-M and get PDP context ===")

    # 2) Reset IP stack, turn radio off
    send_at(ser, "AT+CIPSHUT", 2)
    send_at(ser, "AT+CFUN=0", 2)
    send_at(ser, "AT+CNMP=38")  # LTE only
    send_at(ser, "AT+CMNB=1")   # Cat-M1
    send_at(ser, "AT+CFUN=1,1", 2)
    print("Waiting 5s for module reboot...")
    time.sleep(5)

    # Basic checks
    send_at(ser, "AT")
    send_at(ser, "AT+CPIN?")
    send_at(ser, "AT+CSQ")
    send_at(ser, "AT+CREG?")
    send_at(ser, "AT+CEREG?")

    # Setup APN
    send_at(ser, f'AT+CGDCONT=1,"IP","{APN}"', 1.5)
    send_at(ser, "AT+CGATT=1", 3)
    send_at(ser, f'AT+CNACT=1,"{APN}"', 5)
    send_at(ser, "AT+CNACT?", 2)

    print("\n=== STEP 2: Try CIP approach (if supported) ===")
    print("-- Attempt CIP to PUBLIC IP:PORT --")
    cip_public = try_cip_tcp(ser, TEST_PUBLIC_IP, TEST_PUBLIC_PORT)
    print(f"Public CIP result: {cip_public}")

    if cip_public is None:
        print("No CIP response. Possibly unsupported.")
    elif cip_public == "ERROR":
        print("CIP commands recognized but failed to connect or blocked.")
    elif cip_public == "CONNECT OK":
        print("CIP connected successfully to public IP!")
        # We can forcibly end with CIPSHUT
        send_at(ser, "AT+CIPSHUT", 2)

    print("-- Attempt CIP to PRIVATE IP:PORT --")
    cip_private = try_cip_tcp(ser, TEST_PRIVATE_IP, TEST_PRIVATE_PORT)
    print(f"Private CIP result: {cip_private}")
    if cip_private == "CONNECT OK":
        print("CIP connection to private IP success!")
        send_at(ser, "AT+CIPSHUT", 2)

    print("\n=== STEP 3: Try CSOC approach (modern) ===")
    print("-- Attempt CSOC to PUBLIC IP:PORT --")
    csoc_public = try_csoc_tcp(ser, TEST_PUBLIC_IP, TEST_PUBLIC_PORT)
    print(f"CSOC public result: {csoc_public}")
    # If success, you might do AT+CSOCL=<socketID> to close.

    print("-- Attempt CSOC to PRIVATE IP:PORT --")
    csoc_private = try_csoc_tcp(ser, TEST_PRIVATE_IP, TEST_PRIVATE_PORT)
    print(f"CSOC private result: {csoc_private}")

    # Cleanup
    send_at(ser, "AT+CSOCL=0", 1)  # attempt close socket ID 0 (if used)
    # Or if multiple sockets, close them all
    for i in range(5):
        send_at(ser, f"AT+CSOCL={i}", 0.5)

    ser.close()
    print("\n=== DONE! Check the above results. ===\n")
    print("If CIP is not supported, the CIP attempts yield 'ERROR' or no response.")
    print("If CSOC is not supported, you'll see 'False' or None for those attempts.")
    print("Success means a handshake worked. But if your route to 10.82.x.x is blocked, the private attempt fails.")

if __name__ == "__main__":
    main()
