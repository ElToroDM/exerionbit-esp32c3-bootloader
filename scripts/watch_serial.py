#!/usr/bin/env python3
import argparse
import time
import sys
import re
from datetime import datetime

try:
    import serial
    from serial.tools import list_ports
except Exception as e:
    print("Missing dependency 'pyserial'. Install with: python -m pip install pyserial")
    raise

parser = argparse.ArgumentParser(description='Watch serial port and detect stalls')
parser.add_argument('--port', default=None, help='Serial port (optional; auto-detects first available port if omitted)')
parser.add_argument('--baud', type=int, default=115200)
parser.add_argument('--inactivity', type=float, default=3.0, help='seconds of no new lines to consider inactivity')
parser.add_argument('--same-time', type=float, default=3.0, help='seconds the same line persists to consider a stall')
parser.add_argument('--log', default='build/bootlog.txt')
args = parser.parse_args()

def resolve_port(cli_port: str | None) -> str:
    if cli_port:
        return cli_port
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports detected. Connect a device or pass --port <PORT>.")
        sys.exit(2)
    selected = ports[0].device
    print(f"Auto-detected serial port: {selected}")
    return selected


port = resolve_port(args.port)
baud = args.baud
inactive_thresh = args.inactivity
same_thresh = args.same_time
log_path = args.log

print(f"Opening serial {port} @ {baud} bps. Logging to {log_path}")

last_line = None
last_time = time.time()
last_change_time = time.time()
line_counter = 0  # monotonic line counter for every incoming line
prefixed_re = re.compile(r'^\[#\d{4}\]')  # detect lines already prefixed by device

ser = None

while True:
    if ser is None:
        try:
            ser = serial.Serial(port, baud, timeout=1)
            print(f"Connected to {port}")
        except Exception as e:
            sys.stdout.write(f"Could not open {port}: {e}\n")
            sys.stdout.flush()
            time.sleep(1)
            continue

    try:
        line = ser.readline().decode('utf-8', errors='replace').rstrip('\r\n')
        now = time.time()
        if line:
            timestamp = datetime.now().isoformat()
            line_counter += 1
            # If the device already prefixed the line (e.g. [#0001]), don't add another prefix
            if prefixed_re.match(line):
                out = f"{timestamp}  {line}"
            else:
                prefix = f"[#%04d] " % line_counter
                out = f"{timestamp}  {prefix}{line}"
            print(out)
            with open(log_path, 'a', encoding='utf-8') as f:
                f.write(out + '\n')
            if line != last_line:
                last_line = line
                last_change_time = now
            last_time = now
        else:
            # no data this loop
            now = time.time()
            if now - last_time > inactive_thresh:
                print(f"[WARNING] No new lines for {now-last_time:.1f}s (threshold {inactive_thresh}s)")
                with open(log_path, 'a', encoding='utf-8') as f:
                    f.write(f"[{datetime.now().isoformat()}] WARNING: inactivity {now-last_time:.1f}s\n")
                last_time = now
            if last_line is not None and (now - last_change_time) > same_thresh:
                print(f"[STALL] Last line unchanged for {now-last_change_time:.1f}s: '{last_line}'")
                with open(log_path, 'a', encoding='utf-8') as f:
                    f.write(f"[{datetime.now().isoformat()}] STALL: '{last_line}' for {now-last_change_time:.1f}s\n")
                # don't repeatedly spam; advance last_change_time
                last_change_time = now
    except KeyboardInterrupt:
        print('\nExiting on user request')
        try:
            ser.close()
        except:
            pass
        break
    except Exception as e:
        # Handle transient disconnects (USB re-enumeration on reset) more gracefully.
        # Common symptom on Windows: ClearCommError / PermissionError during reset.
        err_str = str(e)
        if isinstance(e, PermissionError) or 'ClearCommError' in err_str or isinstance(e, serial.SerialException):
            print(f"[INFO] Port {port} disconnected/reset (transient). Waiting to reconnect...")
        else:
            print(f"Serial read error: {e}")
        try:
            ser.close()
        except:
            pass
        ser = None
        # small backoff to allow OS re-enumeration
        time.sleep(1)
        continue
