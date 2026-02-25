# Waveshare ESP32-C3 Zero Custom Bootloader
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](https://opensource.org/licenses/MIT)&nbsp;&nbsp;
[![Build Status](https://github.com/ElToroDM/exerionbit-esp32c3-bootloader/actions/workflows/ci-build.yml/badge.svg?style=flat-square)](https://github.com/ElToroDM/exerionbit-esp32c3-bootloader/actions)&nbsp;&nbsp;
[![Target: ESP32-C3](https://img.shields.io/badge/Target-ESP32--C3-success?style=flat-square&logo=espressif)](https://www.espressif.com/en/products/socs/esp32-c3)&nbsp;&nbsp;
[![Custom Paid Ports](https://img.shields.io/badge/Custom-Paid%20Ports-brightgreen?style=flat-square)](https://github.com/ElToroDM/exerionbit-esp32c3-bootloader/issues)&nbsp;&nbsp;

Minimal ESP-IDF bootloader for the Waveshare ESP32-C3 Zero board.

## What this repository proves

- Deterministic ESP32-C3 second-stage boot behavior on real hardware baseline
- Explicit boot decision states with stable serial tokens and LED mapping
- Reproducible normal/recovery/update baseline suitable for audit-style review

## What this repository does not prove

- Advanced production hardening internals
- Full key management architecture
- Client-specific anti-tamper implementation details

## Quick demo

1. Build and flash using ESP-IDF.
2. Capture logs with `scripts/watch_serial.py`.
3. Validate token sequence with the validation profile (`VALIDATION_PROFILE.md`).

## Known limits

- Live validation requires physical ESP32-C3 hardware access
- Native USB re-enumeration can hide very early boot lines without watcher tooling
- Public repository documents baseline behavior, not full production hardening

## Contact

- Open an issue for scoped bootloader adaptation work
- See `BOOT_SEQUENCE.md` and `VALIDATION_PROFILE.md` for the canonical contract

## Hardware
- **Board**: Waveshare ESP32-C3 Zero
- **SoC**: ESP32-C3FH4 (RISC-V, 160MHz)
- **Flash**: 4MB onboard
- **USB**: USB-CDC (no external USB-UART chip)
- **BOOT**: GPIO9 (manual sequence required)

## Manual Boot Sequence
To flash the board:
1. Press and hold **BOOT** button (GPIO9)
2. Press and release **RESET** button
3. Release **BOOT** button
4. Run flash command

## Quick Start

1. Install ESP‑IDF v6.1 and the VS Code ESP‑IDF extension (recommended).
2. Open this folder in VS Code and point the extension to `C:\esp\esp-idf`.
3. Use the ESP‑IDF extension: Build → Flash → Monitor.
4. Or in PowerShell (example): `set IDF_PATH=C:\esp\esp-idf ; idf.py -p COM4 flash monitor`
5. To capture full boot logs (ESP32‑C3 USB re‑enumerates): `python scripts/watch_serial.py --port COM4`
6. See `SETUP.md` for environment setup, toolchain PATH, PowerShell alias, and troubleshooting.

---

## Expected Boot Messages
Look for these in the serial monitor:

Normal path (ordered):
- `BL_EVT:INIT`
- `BL_EVT:HW_READY`
- `BL_EVT:PARTITION_TABLE_OK`
- `BL_EVT:DECISION_NORMAL`
- `BL_EVT:APP_CRC_CHECK`
- `BL_EVT:APP_CRC_OK`
- `BL_EVT:LOAD_APP`
- `BL_EVT:HANDOFF`
- `BL_EVT:HANDOFF_APP`
- `APP_EVT:START`
- `APP_EVT:BOOTLOADER_HANDOFF_OK`

Selector/update path (`GPIO9`):
- `BL_EVT:MODE_SELECT_ARMED`
- `BL_EVT:MODE_SELECTED:UPDATE` (and mode cycling on short press)
- `BL_EVT:MODE_EXECUTE:UPDATE` (long press)
- followed by normal CRC + handoff sequence

Recovery hold path:
- `BL_EVT:DECISION_RECOVERY`
- `BL_EVT:RECOVERY_HEARTBEAT:<n>` every 5s
- no `BL_EVT:HANDOFF_APP` while recovery is active

## USB serial behavior

ESP32-C3 native USB CDC disconnects during reset. Early ROM and bootloader output is invisible to `idf.py monitor` until the app reinitializes USB. Use `scripts/watch_serial.py` for complete boot sequence capture with automatic reconnection. See [SETUP.md](SETUP.md) for full setup, watcher flags, and troubleshooting.

---

## Validation profile

This repository provides a minimal bootloader demo and instructions for building and flashing on the Waveshare ESP32-C3 Zero.

**Boot sequence specification**: See [BOOT_SEQUENCE.md](BOOT_SEQUENCE.md) for complete LED mapping, timing budget, and serial token definitions.

Locked validation defaults:
- Serial port auto-detection enabled by default
- Total validation timeout: `10s`
- Manual port override remains available

Evidence policy:
- Protocol PASS/FAIL is based on deterministic boot/app token checks.

## Project Structure
```
./
├── CMakeLists.txt              # Top-level project (includes bootloader_components, app_test)
├── sdkconfig.defaults          # Minimal bootloader config
├── partitions.csv              # Partition table (NVS + factory app)
├── app_test/
│   ├── CMakeLists.txt
│   └── main.c                  # Validation/integration app
├── bootloader_components/
│   └── main/                   # ← Custom bootloader (replaces ESP-IDF default)
│       ├── CMakeLists.txt
│       └── main.c              # Custom call_start_cpu0() implementation
└── scripts/
    └── watch_serial.py         # Serial monitoring with USB reconnection handling
```

## Key Implementation Details
- **Custom bootloader**: `bootloader_components/main/main.c` contains our `call_start_cpu0()`
- **Component override**: Our `main` component replaces ESP-IDF's default bootloader main
- **USB handling**: Scripts handle ESP32-C3 USB reenumeration during reset
- **Build verification**: Bootloader size `0x5160 bytes` (vs default `0x52a0`)

## Configuration Notes
- Secure Boot: **DISABLED** (development mode)
- Flash Encryption: **DISABLED** (development mode)
- Bootloader Log Level: **ERROR** (reduce noise)
- Partition Table Offset: **0x8000** (default)
- Compiler Optimization: **Size (-Os)**

## Future Extensions
- OTA update support (add ota_0, ota_1, otadata partitions)
- Image signature verification
- JTAG debugging (add ESP-PROG probe)
- USB DFU mode
