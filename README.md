# Waveshare ESP32-C3 Zero Custom Bootloader
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](https://opensource.org/licenses/MIT)&nbsp;&nbsp;
[![Build Status](https://github.com/ElToroDM/exerionbit-esp32c3-bootloader/actions/workflows/ci-build.yml/badge.svg?style=flat-square)](https://github.com/ElToroDM/exerionbit-esp32c3-bootloader/actions)&nbsp;&nbsp;
[![Target: ESP32-C3](https://img.shields.io/badge/Target-ESP32--C3-success?style=flat-square&logo=espressif)](https://www.espressif.com/en/products/socs/esp32-c3)&nbsp;&nbsp;
[![Custom Paid Ports](https://img.shields.io/badge/Custom-Paid%20Ports-brightgreen?style=flat-square)](https://github.com/ElToroDM/exerionbit-esp32c3-bootloader/issues)&nbsp;&nbsp;

Minimal ESP-IDF bootloader for the Waveshare ESP32-C3 Zero board.

## What this repository proves

- Deterministic ESP32-C3 second-stage boot behavior on real hardware baseline
- Explicit boot decision states with stable serial tokens and LED mapping
- Reproducible normal path and selector-driven mode decisions suitable for audit-style review

## What this repository does not prove

- Advanced production hardening internals
- Full key management architecture
- Client-specific anti-tamper implementation details

## Quick demo

1. Build and flash using ESP-IDF.
2. Capture logs with `scripts/watch_serial.py`.
3. Validate token sequence with the validation profile (`VALIDATION_PROFILE.md`).
4. Optional: hold `GPIO9` at boot to enter selector and verify mode tokens (`MODE_SELECT_ARMED`, `MODE_SELECTED`, `MODE_EXECUTE`).
5. Optional: test CRC validation via `SETUP.md` (CRC OK path and CRC failure path with recovery).

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

### Automated setup (recommended)

1. Install ESP‑IDF v6.1 (or use VS Code ESP‑IDF extension).
2. See [SETUP.md](SETUP.md) for one-time PowerShell profile setup that configures all paths, tools, and convenience wrappers.
3. After setup, use any of:
   - **VS Code**: ESP‑IDF extension → Build → Flash → Monitor
    - **PowerShell**: `idf build` / `idf flash` / `idf monitor`
    - Optional port override: `idf -p <PORT> flash` / `idf -p <PORT> monitor`


### Manual setup (if not using profile)

In PowerShell:
```powershell
$env:IDF_PATH = 'C:\esp\esp-idf'
$idfPythonDir = "$env:USERPROFILE\.espressif\python_env\idf6.1_py3.11_env\Scripts"
& "$idfPythonDir\python.exe" C:\esp\esp-idf\tools\idf.py build
```

See [SETUP.md](SETUP.md) for full environment details, toolchain paths, and troubleshooting.

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
- `BL_EVT:DECISION_UPDATE` will be emitted when update mode is executed; the bootloader also publishes `BOOT_CTX_UPDATE` to the application.
- Current baseline: update mode does not receive/write a new image yet; it exercises decision/context flow and then follows the existing CRC + handoff path.

Recovery hold path:
- `BL_EVT:DECISION_RECOVERY`
- `BL_EVT:RECOVERY_HEARTBEAT:<n>` every 5s
- no `BL_EVT:HANDOFF_APP` while recovery is active
- Current baseline: no UART command parser is implemented in recovery hold yet.

## USB serial behavior

ESP32-C3 native USB CDC disconnects during reset. Early ROM and bootloader output is invisible to `idf monitor` until the app reinitializes USB.

For early complete boot capture, use `idf monitor` (default tool). For long-running logging with timestamps and file output, use `python scripts/watch_serial.py` (note: starts logging after initial bootloader events already pass).

Example:
```powershell
python scripts/watch_serial.py --inactivity 10
```

See [SETUP.md](SETUP.md) for full setup, watcher flags, and troubleshooting.

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
- UART update protocol (receive/write/verify/apply flow)
- Recovery console command parser (`status`, `reboot`, `enter_update`, `erase_app`, `boot`)
- Full app payload CRC verification (beyond descriptor-level baseline)
- OTA update support (add ota_0, ota_1, otadata partitions)
- Image signature verification
- JTAG debugging (add ESP-PROG probe)
- USB DFU mode
