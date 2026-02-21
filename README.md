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
```
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x15 (USB_UART_CHIP_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT)
Saved PC:0x4038315a
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd5830,len:0x1594
load:0x403cbf10,len:0xc9c
load:0x403ce710,len:0x3010
entry 0x403cbf1a
I (24) boot: ESP-IDF v6.1-dev-2309-g7d7f533357 2nd stage bootloader
I (24) boot: compile time Feb  2 2026 23:03:36
I (25) boot: chip revision: v0.4
I (25) boot: Enabling RNG early entropy source...
I (25) boot: Partition Table:
I (26) boot: ## Label            Usage          Type ST Offset   Length
I (26) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (26) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (26) boot:  2 factory          factory app      00 00 00010000 00100000
I (27) boot: End of partition table
I (27) esp_image: segment 0: paddr=00010020 vaddr=3c010020 size=0736ch ( 29548) map
I (32) esp_image: segment 1: paddr=00017394 vaddr=3fc89800 size=01840h (  6208) load
I (39) esp_image: segment 3: paddr=00020020 vaddr=42000020 size=0e5ach ( 58796) map
I (49) esp_image: segment 4: paddr=0002e5d4 vaddr=4038743c size=02368h (  9064) load
I (54) boot: Loaded app from partition at offset 0x10000
I (54) boot: Disabling RNG early entropy source...
```

## USB serial behavior

ESP32-C3 native USB CDC disconnects during reset. Early ROM and bootloader output is invisible to
`idf.py monitor` until the app reinitializes USB. Use `scripts/watch_serial.py` for complete boot
sequence capture with automatic reconnection. See [SETUP.md](SETUP.md) for full setup, watcher
flags, and troubleshooting.

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
│   └── boot_core/              # ← Custom bootloader (replaces ESP-IDF default)
│       ├── CMakeLists.txt
│       └── main.c              # Custom call_start_cpu0() implementation
└── scripts/
    └── watch_serial.py         # Serial monitoring with USB reconnection handling
```

## Key Implementation Details
- **Custom bootloader**: `bootloader_components/boot_core/main.c` contains our `call_start_cpu0()`
- **Component override**: Our `boot_core` component replaces ESP-IDF's default bootloader main
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
