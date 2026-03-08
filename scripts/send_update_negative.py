#!/usr/bin/env python3
"""Negative-case UART update sender for ESP32-C3 bootloader baseline.

Cases:
- corrupt_chunk: sends one frame with wrong CRC16 and expects CHUNK_FAIL
- oob_offset: sends header with out-of-bounds offset and expects CHUNK_FAIL
- invalid_final_crc: sends tampered image and expects APP_CRC_FAIL after END_UPDATE
- partial_transfer: starts a chunk, disconnects before payload completion, reconnects and observes timeout behavior
"""

from __future__ import annotations

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Optional

import serial


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


def log_line(handle, direction: str, line: str) -> None:
    stamp = datetime.now().isoformat(timespec="seconds")
    handle.write(f"{stamp} {direction} {line}\n")
    handle.flush()
    print(f"{direction}: {line}")


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


def wait_token_prefix(ser: serial.Serial, prefix: str, timeout_s: float, log_handle) -> Optional[str]:
    end = time.time() + timeout_s
    while time.time() < end:
        line = read_line_with_timeout(ser, max(0.05, end - time.time()))
        if line is None:
            continue
        log_line(log_handle, "RX", line)
        if line.startswith(prefix):
            return line
    return None


def wait_for_ready_for_chunk(ser: serial.Serial, timeout_s: float, log_handle) -> bool:
    end = time.time() + timeout_s
    while time.time() < end:
        line = wait_token_prefix(ser, "BL_EVT:", max(0.05, end - time.time()), log_handle)
        if line is None:
            return False
        if line.startswith("BL_EVT:READY_FOR_CHUNK"):
            return True
        if line.startswith("BL_EVT:READY_FOR_UPDATE"):
            ser.write(b"START_UPDATE\n")
            ser.flush()
            log_line(log_handle, "TX", "START_UPDATE")
            continue
        if line.startswith("BL_EVT:UPDATE_START_FAIL"):
            return False
    return False


def perform_handshake(ser: serial.Serial, timeout_s: float, log_handle) -> bool:
    # Send early START_UPDATE to tolerate attach race.
    ser.write(b"START_UPDATE\n")
    ser.flush()
    log_line(log_handle, "TX", "START_UPDATE (pre-handshake)")
    return wait_for_ready_for_chunk(ser, timeout_s, log_handle)


def case_corrupt_chunk(ser: serial.Serial, timeout_s: float, log_handle) -> int:
    if not perform_handshake(ser, timeout_s, log_handle):
        return 20

    payload = b"A" * 1024
    good_crc = crc16_ccitt(payload)
    bad_crc = (good_crc + 1) & 0xFFFF
    header = f"[{len(payload)}:0:{bad_crc}]\n".encode("ascii")

    ser.write(header)
    ser.write(payload)
    ser.flush()
    log_line(log_handle, "TX", f"{header.decode('ascii').strip()} + {len(payload)} bytes")

    fail = wait_token_prefix(ser, "BL_EVT:CHUNK_FAIL", timeout_s, log_handle)
    return 0 if fail else 21


def case_oob_offset(ser: serial.Serial, timeout_s: float, log_handle, partition_size: int) -> int:
    if not perform_handshake(ser, timeout_s, log_handle):
        return 30

    payload_len = 1024
    offset = partition_size
    header = f"[{payload_len}:{offset}:0]\n".encode("ascii")
    ser.write(header)
    ser.flush()
    log_line(log_handle, "TX", header.decode("ascii").strip())

    fail = wait_token_prefix(ser, "BL_EVT:CHUNK_FAIL", timeout_s, log_handle)
    return 0 if fail else 31


def case_invalid_final_crc(ser: serial.Serial, timeout_s: float, log_handle, image: bytes, chunk_size: int) -> int:
    if not perform_handshake(ser, timeout_s, log_handle):
        return 40

    tampered = bytearray(image)
    tampered[0] ^= 0x01

    offset = 0
    total = len(tampered)
    while offset < total:
        chunk = bytes(tampered[offset : offset + chunk_size])
        crc = crc16_ccitt(chunk)
        header = f"[{len(chunk)}:{offset}:{crc}]\n".encode("ascii")

        ser.write(header)
        ser.write(chunk)
        ser.flush()
        log_line(log_handle, "TX", f"{header.decode('ascii').strip()} + {len(chunk)} bytes")

        ok = wait_token_prefix(ser, "BL_EVT:CHUNK_OK", timeout_s, log_handle)
        if ok is None:
            return 41

        ready = wait_token_prefix(ser, "BL_EVT:READY_FOR_CHUNK", timeout_s, log_handle)
        if ready is None and (offset + len(chunk) < total):
            return 42

        offset += len(chunk)

    ser.write(b"END_UPDATE\n")
    ser.flush()
    log_line(log_handle, "TX", "END_UPDATE")

    session_end = wait_token_prefix(ser, "BL_EVT:UPDATE_SESSION_END", timeout_s, log_handle)
    if session_end is None:
        return 43

    crc_result = wait_token_prefix(ser, "BL_EVT:APP_CRC_", timeout_s, log_handle)
    if crc_result is None:
        return 44
    if crc_result == "BL_EVT:APP_CRC_CHECK":
        crc_result = wait_token_prefix(ser, "BL_EVT:APP_CRC_", timeout_s, log_handle)
    if crc_result != "BL_EVT:APP_CRC_FAIL":
        return 45

    return 0


def case_partial_transfer(
    port: str,
    baud: int,
    timeout_s: float,
    log_handle,
    observe_timeout_s: float,
) -> int:
    with serial.Serial(port=port, baudrate=baud, timeout=0.2) as ser:
        if not perform_handshake(ser, timeout_s, log_handle):
            return 50

        payload = b"B" * 1024
        crc = crc16_ccitt(payload)
        header = f"[{len(payload)}:0:{crc}]\n".encode("ascii")

        ser.write(header)
        ser.write(payload[:256])
        ser.flush()
        log_line(log_handle, "TX", f"{header.decode('ascii').strip()} + 256/1024 bytes")
        log_line(log_handle, "TX", "INTENTIONAL_DISCONNECT")

    try:
        wait_end = time.time() + observe_timeout_s
        while time.time() < wait_end:
            time.sleep(0.25)
    except KeyboardInterrupt:
        # Some host environments may interrupt long sleeps.
        # Continue with reconnect and observe resulting bootloader state.
        log_line(log_handle, "HOST", "INTERRUPT_DURING_OBSERVE_WAIT_CONTINUE")

    with serial.Serial(port=port, baudrate=baud, timeout=0.2) as ser:
        fail = wait_token_prefix(ser, "BL_EVT:CHUNK_FAIL", timeout_s, log_handle)
        ready = wait_token_prefix(ser, "BL_EVT:READY_FOR_CHUNK", timeout_s, log_handle)
        if fail is None and ready is None:
            # Recovery heartbeat after interruption is also a valid fail-safe outcome.
            recovery = wait_token_prefix(ser, "BL_EVT:RECOVERY_HEARTBEAT", timeout_s, log_handle)
            if recovery is None:
                return 51

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Run negative UART update protocol tests.")
    parser.add_argument("--case", required=True, choices=["corrupt_chunk", "oob_offset", "invalid_final_crc", "partial_transfer"])
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=12.0)
    parser.add_argument("--chunk-size", type=int, default=1024)
    parser.add_argument("--partition-size", type=int, default=1048576)
    parser.add_argument("--image", default=None)
    parser.add_argument("--observe-timeout", type=float, default=125.0)
    parser.add_argument("--log", required=True)
    args = parser.parse_args()

    log_path = Path(args.log)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    image = None
    if args.case == "invalid_final_crc":
        if not args.image:
            print("--image is required for invalid_final_crc")
            return 2
        image_path = Path(args.image)
        if not image_path.exists():
            print(f"Image not found: {image_path}")
            return 2
        image = image_path.read_bytes()
        if len(image) == 0 or (len(image) % 4) != 0:
            print("Image must be non-empty and 4-byte aligned")
            return 2

    if args.chunk_size <= 0 or args.chunk_size > 1024 or (args.chunk_size % 4) != 0:
        print("--chunk-size must be in range 4..1024 and 4-byte aligned")
        return 2

    with log_path.open("a", encoding="utf-8") as log_handle:
        log_line(log_handle, "HOST", f"START case={args.case} port={args.port} baud={args.baud}")
        try:
            if args.case == "partial_transfer":
                rc = case_partial_transfer(args.port, args.baud, args.timeout, log_handle, args.observe_timeout)
            else:
                with serial.Serial(port=args.port, baudrate=args.baud, timeout=0.2) as ser:
                    if args.case == "corrupt_chunk":
                        rc = case_corrupt_chunk(ser, args.timeout, log_handle)
                    elif args.case == "oob_offset":
                        rc = case_oob_offset(ser, args.timeout, log_handle, args.partition_size)
                    else:
                        rc = case_invalid_final_crc(ser, args.timeout, log_handle, image, args.chunk_size)

            if rc == 0:
                log_line(log_handle, "HOST", "RESULT PASS")
            else:
                log_line(log_handle, "HOST", f"RESULT FAIL rc={rc}")
            return rc
        except serial.SerialException as exc:
            log_line(log_handle, "HOST", f"SERIAL_ERROR {exc}")
            return 3


if __name__ == "__main__":
    sys.exit(main())
