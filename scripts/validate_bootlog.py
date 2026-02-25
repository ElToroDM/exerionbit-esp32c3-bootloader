#!/usr/bin/env python3
"""Validate ESP32-C3 boot log token sequence from captured serial logs."""

import argparse
import sys
from pathlib import Path


REQUIRED_ORDER = [
    "BL_EVT:INIT",
    "BL_EVT:HW_READY",
    "BL_EVT:PARTITION_TABLE_OK",
    "BL_EVT:DECISION_NORMAL",
    "BL_EVT:APP_CRC_CHECK",
    "BL_EVT:APP_CRC_OK",
    "BL_EVT:LOAD_APP",
    "BL_EVT:HANDOFF",
    "BL_EVT:HANDOFF_APP",
    "APP_EVT:START",
    "APP_EVT:BOOTLOADER_HANDOFF_OK",
]


def load_lines(log_path: Path) -> list[str]:
    if not log_path.exists():
        raise FileNotFoundError(f"Log file not found: {log_path}")
    return log_path.read_text(encoding="utf-8", errors="replace").splitlines()


def find_in_order(lines: list[str], required_tokens: list[str]) -> tuple[bool, list[str]]:
    failures: list[str] = []
    index = 0

    for token in required_tokens:
        found = False
        while index < len(lines):
            if token in lines[index]:
                found = True
                index += 1
                break
            index += 1

        if not found:
            failures.append(f"Missing token in order: {token}")

    return len(failures) == 0, failures


def check_recovery(lines: list[str]) -> tuple[bool, str]:
    recovery_idx = next((i for i, line in enumerate(lines) if "BL_EVT:DECISION_RECOVERY" in line), -1)
    if recovery_idx == -1:
        return True, "Recovery path not observed in this log (non-blocking)."

    handoff_after_recovery = any("BL_EVT:HANDOFF_APP" in line for line in lines[recovery_idx + 1 :])
    if handoff_after_recovery:
        return False, "Recovery token observed but handoff occurred afterward in same trace."

    return True, "Recovery path observed without app handoff."


def check_crc_fail_safety(lines: list[str]) -> tuple[bool, str]:
    crc_fail_idx = next((i for i, line in enumerate(lines) if "BL_EVT:APP_CRC_FAIL" in line), -1)
    if crc_fail_idx == -1:
        return True, "CRC fail path not observed in this log (non-blocking)."

    handoff_after_crc_fail = any("BL_EVT:HANDOFF_APP" in line for line in lines[crc_fail_idx + 1 :])
    if handoff_after_crc_fail:
        return False, "CRC fail observed but app handoff occurred afterward in same trace."

    return True, "CRC fail path observed without app handoff."


def run_validation(log_path: Path) -> int:
    lines = load_lines(log_path)

    success, failures = find_in_order(lines, REQUIRED_ORDER)
    recovery_ok, recovery_msg = check_recovery(lines)
    crc_fail_ok, crc_fail_msg = check_crc_fail_safety(lines)

    print("=== ESP32-C3 Bootlog Validation ===")
    print(f"Log: {log_path}")
    print(f"Lines: {len(lines)}")

    for token in REQUIRED_ORDER:
        status = "PASS" if any(token in line for line in lines) else "MISS"
        print(f"[{status}] {token}")

    print(f"[{'PASS' if recovery_ok else 'FAIL'}] {recovery_msg}")
    print(f"[{'PASS' if crc_fail_ok else 'FAIL'}] {crc_fail_msg}")

    if not success:
        print("\nValidation failures:")
        for failure in failures:
            print(f"- {failure}")

    final_ok = success and recovery_ok and crc_fail_ok
    print(f"\nFINAL: {'PASS' if final_ok else 'FAIL'}")
    return 0 if final_ok else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate ESP32-C3 boot tokens from a captured serial log")
    parser.add_argument("--log", default="build/bootlog.txt", help="Path to captured log file")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        return run_validation(Path(args.log))
    except FileNotFoundError as exc:
        print(exc)
        return 2
    except Exception as exc:
        print(f"Unexpected error: {exc}")
        return 3


if __name__ == "__main__":
    sys.exit(main())
