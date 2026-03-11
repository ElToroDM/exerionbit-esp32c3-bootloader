#!/usr/bin/env python3
"""Validate release evidence contract files without requiring hardware access."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REQUIRED_FIELDS = [
    "Date:",
    "Release context:",
    "Board:",
    "Connection:",
    "ESP-IDF:",
    "Host Python:",
    "esptool:",
    "Build:",
    "Flash:",
    "Monitor/capture:",
    "Expected:",
    "Observed:",
    "Status:",
]

ALLOWED_STATUS = {"PASS", "FAIL", "PARTIAL"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate docs/evidence contract files")
    parser.add_argument("--release", default="v0.2", help="Release folder under docs/evidence")
    return parser.parse_args()


def check_file_exists(path: Path, failures: list[str]) -> None:
    if not path.exists():
        failures.append(f"Missing required file: {path}")


def check_required_fields(content: str, failures: list[str]) -> None:
    for field in REQUIRED_FIELDS:
        if field not in content:
            failures.append(f"Missing required field marker: {field}")


def check_status_values(content: str, failures: list[str]) -> None:
    statuses = re.findall(r"Status:\s*([A-Z]+)", content)
    if not statuses:
        failures.append("No Status values found")
        return

    for status in statuses:
        if status not in ALLOWED_STATUS:
            failures.append(f"Invalid status value: {status}")


def check_logs_folder(logs_dir: Path, failures: list[str]) -> None:
    if not logs_dir.exists() or not logs_dir.is_dir():
        failures.append(f"Missing logs directory: {logs_dir}")
        return

    logs = sorted(logs_dir.glob("*.log"))
    if not logs:
        failures.append(f"No .log files found in {logs_dir}")


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parents[1]
    release_dir = root / "docs" / "evidence" / args.release
    expected_file = release_dir / "expected-vs-observed.md"
    logs_dir = release_dir / "logs"

    failures: list[str] = []

    check_file_exists(expected_file, failures)
    check_logs_folder(logs_dir, failures)

    if expected_file.exists():
        content = expected_file.read_text(encoding="utf-8", errors="replace")
        check_required_fields(content, failures)
        check_status_values(content, failures)

    print("=== Evidence Contract Validation ===")
    print(f"Release: {args.release}")
    print(f"Evidence file: {expected_file}")
    print(f"Logs directory: {logs_dir}")

    if failures:
        print("\nFINAL: FAIL")
        for item in failures:
            print(f"- {item}")
        return 1

    print("\nFINAL: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
