#!/usr/bin/env python3
"""Minimal UART update sender for Day-2 baseline.

Protocol contract:
- Wait for BL_EVT:READY_FOR_UPDATE
- Send START_UPDATE\n
- For each chunk:
  - Wait for BL_EVT:READY_FOR_CHUNK
  - Send header: [LEN:OFFSET:CRC16]\n  (decimal fields)
  - Send LEN bytes of binary payload
  - Wait for BL_EVT:CHUNK_OK:OFFSET

- Finalize:
  - Wait for BL_EVT:READY_FOR_CHUNK
  - Send END_UPDATE\n
  - Wait for BL_EVT:UPDATE_SESSION_END and BL_EVT:APP_CRC_OK
"""

from __future__ import annotations

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Callable, Optional, Tuple

try:
    import serial
    from serial.tools import list_ports
except Exception:
    print("Missing dependency 'pyserial'. Install with: python -m pip install pyserial")
    raise


def crc16_ccitt(data: bytes, seed: int = 0xFFFF) -> int:
    crc = seed & 0xFFFF
    for value in data:
        crc ^= (value << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def resolve_port(cli_port: Optional[str]) -> str:
    if cli_port:
        return cli_port
    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports detected. Connect the board or pass --port <PORT>.")
    return ports[0].device


def read_line_with_timeout(ser: serial.Serial, timeout_s: float) -> Optional[str]:
    end = time.time() + timeout_s
    while time.time() < end:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()
        if line:
            return line
    return None


def wait_for_token(
    ser: serial.Serial,
    token_prefix: str,
    timeout_s: float,
    log_handle,
    reconnect_cb: Optional[Callable[[], serial.Serial]] = None,
) -> Optional[str]:
    end = time.time() + timeout_s
    while time.time() < end:
        try:
            line = read_line_with_timeout(ser, max(0.05, end - time.time()))
        except (serial.SerialException, OSError):
            if reconnect_cb is None:
                return None
            try:
                ser.close()
            except Exception:
                pass
            remaining = end - time.time()
            if remaining <= 0:
                return None
            time.sleep(0.2)
            ser = reconnect_cb()
            continue
        if line is None:
            continue
        stamp = datetime.now().isoformat(timespec="seconds")
        log_handle.write(f"{stamp} RX {line}\n")
        log_handle.flush()
        print(f"RX: {line}")
        if line.startswith(token_prefix):
            return line
    return None


def parse_chunk_ok(line: str) -> Optional[int]:
    # Expected format: BL_EVT:CHUNK_OK:<offset>
    parts = line.split(":")
    if len(parts) != 3:
        return None
    if parts[0] != "BL_EVT" or parts[1] != "CHUNK_OK":
        return None
    try:
        return int(parts[2], 10)
    except ValueError:
        return None


def wait_for_chunk_result(ser: serial.Serial, timeout_s: float, log_handle) -> Optional[Tuple[str, str]]:
    """Wait for CHUNK_OK/CHUNK_FAIL and ignore unrelated tokens."""
    end = time.time() + timeout_s
    while time.time() < end:
        try:
            line = read_line_with_timeout(ser, max(0.05, end - time.time()))
        except (serial.SerialException, OSError):
            return None
        if line is None:
            continue
        stamp = datetime.now().isoformat(timespec="seconds")
        log_handle.write(f"{stamp} RX {line}\\n")
        log_handle.flush()
        print(f"RX: {line}")
        if line.startswith("BL_EVT:CHUNK_OK"):
            return ("OK", line)
        if line.startswith("BL_EVT:CHUNK_FAIL"):
            return ("FAIL", line)
    return None


def wait_for_app_crc_result(
    ser: serial.Serial,
    timeout_s: float,
    log_handle,
    reconnect_cb: Optional[Callable[[], serial.Serial]] = None,
) -> Optional[str]:
    """Wait for APP_CRC_OK or APP_CRC_FAIL, ignoring APP_CRC_CHECK."""
    end = time.time() + timeout_s
    while time.time() < end:
        token = wait_for_token(
            ser,
            "BL_EVT:APP_CRC_",
            max(0.05, end - time.time()),
            log_handle,
            reconnect_cb=reconnect_cb,
        )
        if token is None:
            return None
        if token == "BL_EVT:APP_CRC_CHECK":
            continue
        if token == "BL_EVT:APP_CRC_OK" or token == "BL_EVT:APP_CRC_FAIL":
            return token
    return None


def open_serial(port: str, baud: int) -> serial.Serial:
    return serial.Serial(port=port, baudrate=baud, timeout=0.2)


def reconnect_serial(port: str, baud: int, timeout_s: float = 6.0) -> serial.Serial:
    end = time.time() + timeout_s
    last_error: Optional[Exception] = None
    while time.time() < end:
        try:
            return open_serial(port, baud)
        except serial.SerialException as exc:
            last_error = exc
            time.sleep(0.2)
    raise serial.SerialException(f"Reconnect timeout on {port}: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Send firmware image to update-mode bootloader over UART.")
    parser.add_argument("--image", required=True, help="Path to image to send")
    parser.add_argument("--port", default=None, help="Serial port (auto-detect if omitted)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--chunk-size", type=int, default=1024, help="Chunk size in bytes (default: 1024)")
    parser.add_argument("--timeout", type=float, default=10.0, help="Per-step timeout in seconds")
    parser.add_argument(
        "--log",
        default="docs/evidence/v0.2/logs/recovery_update.log",
        help="Log file path for host+device transcript",
    )
    args = parser.parse_args()

    image_path = Path(args.image)
    if not image_path.exists():
        print(f"Image not found: {image_path}")
        return 2

    chunk_size = args.chunk_size
    if chunk_size <= 0 or chunk_size > 1024:
        print("Invalid --chunk-size: must be in range 1..1024")
        return 2
    if (chunk_size % 4) != 0:
        print("Invalid --chunk-size: must be 4-byte aligned")
        return 2

    image = image_path.read_bytes()
    if len(image) == 0:
        print("Image is empty")
        return 2
    if (len(image) % 4) != 0:
        print("Image size must be 4-byte aligned for bootloader flash writes")
        return 2

    port = resolve_port(args.port)
    log_path = Path(args.log)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    print(f"Using port={port} baud={args.baud} chunk_size={chunk_size} bytes image_size={len(image)}")

    with log_path.open("a", encoding="utf-8") as log_handle:
        stamp = datetime.now().isoformat(timespec="seconds")
        log_handle.write(f"{stamp} HOST start image={image_path} size={len(image)} port={port} baud={args.baud}\n")
        log_handle.flush()

        try:
            with open_serial(port, args.baud) as ser:
                # Allow transient USB reconnect to settle.
                time.sleep(0.2)

                # Race-safe handshake: send START_UPDATE immediately in case the
                # bootloader is already waiting and READY_FOR_UPDATE was emitted
                # before host attach.
                ser.write(b"START_UPDATE\n")
                ser.flush()
                stamp = datetime.now().isoformat(timespec="seconds")
                log_handle.write(f"{stamp} TX START_UPDATE\\n (pre-handshake)\\n")
                log_handle.flush()
                print("TX: START_UPDATE (pre-handshake)")

                ready = wait_for_token(
                    ser,
                    "BL_EVT:",
                    args.timeout,
                    log_handle,
                    reconnect_cb=lambda: reconnect_serial(port, args.baud),
                )
                if ready is None:
                    print("Timeout waiting for initial BL_EVT token in update handshake")
                    return 3

                initial_ready_for_chunk = False

                if ready.startswith("BL_EVT:READY_FOR_UPDATE"):
                    ser.write(b"START_UPDATE\n")
                    ser.flush()
                    stamp = datetime.now().isoformat(timespec="seconds")
                    log_handle.write(f"{stamp} TX START_UPDATE\\n\n")
                    log_handle.flush()
                    print("TX: START_UPDATE")
                elif ready.startswith("BL_EVT:READY_FOR_CHUNK"):
                    # START_UPDATE already accepted via pre-handshake write.
                    initial_ready_for_chunk = True
                elif ready.startswith("BL_EVT:UPDATE_START_FAIL") or ready.startswith("BL_EVT:DECISION_RECOVERY"):
                    print(f"Update mode handshake failed early: {ready}")
                    return 17
                else:
                    # Keep waiting until we see the next contract token.
                    while True:
                        next_token = wait_for_token(
                            ser,
                            "BL_EVT:",
                            args.timeout,
                            log_handle,
                            reconnect_cb=lambda: reconnect_serial(port, args.baud),
                        )
                        if next_token is None:
                            print("Timeout waiting for update handshake tokens")
                            return 3
                        if next_token.startswith("BL_EVT:READY_FOR_CHUNK"):
                            initial_ready_for_chunk = True
                            break
                        if next_token.startswith("BL_EVT:READY_FOR_UPDATE"):
                            ser.write(b"START_UPDATE\n")
                            ser.flush()
                            stamp = datetime.now().isoformat(timespec="seconds")
                            log_handle.write(f"{stamp} TX START_UPDATE\\n\n")
                            log_handle.flush()
                            print("TX: START_UPDATE")
                        if next_token.startswith("BL_EVT:UPDATE_START_FAIL") or next_token.startswith("BL_EVT:DECISION_RECOVERY"):
                            print(f"Update mode handshake failed: {next_token}")
                            return 17

                offset = 0
                total = len(image)
                while offset < total:
                    if initial_ready_for_chunk:
                        initial_ready_for_chunk = False
                    else:
                        ready_chunk = wait_for_token(
                            ser,
                            "BL_EVT:READY_FOR_CHUNK",
                            args.timeout,
                            log_handle,
                            reconnect_cb=lambda: reconnect_serial(port, args.baud),
                        )
                        if ready_chunk is None:
                            print(f"Timeout waiting for BL_EVT:READY_FOR_CHUNK at offset={offset}")
                            return 4

                    chunk = image[offset : offset + chunk_size]
                    crc16 = crc16_ccitt(chunk)
                    header = f"[{len(chunk)}:{offset}:{crc16}]\n".encode("ascii")

                    ser.write(header)
                    ser.write(chunk)
                    ser.flush()

                    stamp = datetime.now().isoformat(timespec="seconds")
                    log_handle.write(f"{stamp} TX {header.decode('ascii').strip()} + {len(chunk)} bytes\n")
                    log_handle.flush()
                    print(f"TX: offset={offset} len={len(chunk)} crc16={crc16}")

                    result = wait_for_chunk_result(ser, args.timeout, log_handle)
                    if result is None:
                        print(f"Timeout waiting for chunk response at offset={offset}")
                        return 5

                    status, line = result

                    if status == "FAIL":
                        print(f"Device reported chunk failure: {line}")
                        return 6

                    parsed_offset = parse_chunk_ok(line)
                    if parsed_offset is None:
                        print(f"Unexpected chunk response: {line}")
                        return 7
                    if parsed_offset != offset:
                        print(f"Chunk offset mismatch: expected={offset} got={parsed_offset}")
                        return 8

                    offset += len(chunk)

                ready_chunk = wait_for_token(
                    ser,
                    "BL_EVT:READY_FOR_CHUNK",
                    args.timeout,
                    log_handle,
                    reconnect_cb=lambda: reconnect_serial(port, args.baud),
                )
                if ready_chunk is None:
                    print("Timeout waiting for final BL_EVT:READY_FOR_CHUNK before END_UPDATE")
                    return 9

                ser.write(b"END_UPDATE\n")
                ser.flush()
                stamp = datetime.now().isoformat(timespec="seconds")
                log_handle.write(f"{stamp} TX END_UPDATE\\n\n")
                log_handle.flush()
                print("TX: END_UPDATE")

                session_end = wait_for_token(
                    ser,
                    "BL_EVT:UPDATE_SESSION_END",
                    args.timeout,
                    log_handle,
                    reconnect_cb=lambda: reconnect_serial(port, args.baud),
                )
                if session_end is None:
                    print("Timeout waiting for BL_EVT:UPDATE_SESSION_END")
                    return 10

                crc_result = wait_for_app_crc_result(
                    ser,
                    args.timeout,
                    log_handle,
                    reconnect_cb=lambda: reconnect_serial(port, args.baud),
                )
                if crc_result is None:
                    print("Timeout waiting for BL_EVT:APP_CRC_OK / BL_EVT:APP_CRC_FAIL")
                    return 11
                if crc_result != "BL_EVT:APP_CRC_OK":
                    print(f"CRC validation failed on device: {crc_result}")
                    return 12

                handoff = wait_for_token(
                    ser,
                    "BL_EVT:HANDOFF",
                    args.timeout,
                    log_handle,
                    reconnect_cb=lambda: reconnect_serial(port, args.baud),
                )
                if handoff is None:
                    print("Timeout waiting for BL_EVT:HANDOFF")
                    return 13

                app_start = wait_for_token(
                    ser,
                    "APP_EVT:START",
                    args.timeout,
                    log_handle,
                    reconnect_cb=lambda: reconnect_serial(port, args.baud),
                )
                if app_start is None:
                    print("Timeout waiting for APP_EVT:START")
                    return 14

        except serial.SerialException as exc:
            print(f"Serial error: {exc}")
            return 15
        except RuntimeError as exc:
            print(str(exc))
            return 16

        stamp = datetime.now().isoformat(timespec="seconds")
        log_handle.write(f"{stamp} HOST update_success\n")
        log_handle.flush()

    print("Update session completed successfully.")
    print(f"Transcript: {log_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
