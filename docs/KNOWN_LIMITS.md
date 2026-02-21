# Known Limits — Waveshare ESP32-C3 Zero Custom Bootloader

**Version:** v0.1  
**Date:** 2026-02-20

---

## Hardware dependency

- Live validation requires physical ESP32-C3 hardware (Waveshare ESP32-C3 Zero).
- QEMU emulation is not available for this target; all evidence artifacts derive from hardware runs.

## USB serial visibility

- ESP32-C3 native USB CDC disconnects during reset. Early ROM and bootloader log output
  is invisible to `idf.py monitor` unless an external UART (GPIO20/21) or JTAG probe is used.
- The included `scripts/watch_serial.py` watcher mitigates this by reconnecting automatically
  and logging to `build/bootlog.txt`, but a brief window of output immediately at power-on
  may still be missed.

## Security scope

- Secure Boot is **disabled** in this public baseline. This repository demonstrates boot
  decision logic, LED/token signaling, and structural correctness only.
- Flash Encryption is **disabled**.
- Production hardening (key provisioning, anti-tamper, rollback protection) is out of scope
  for this proof asset.

## Component override mechanism

- The `boot_core` component in `bootloader_components/boot_core/` replaces ESP-IDF's default
  bootloader main component. This override is validated in CI (see `.github/workflows/`).
- Build size is tracked as a proxy indicator that the override is active (see `VALIDATION_PROFILE.md`).

## OTA and update protocol

- No OTA partition or update protocol is implemented in this baseline.
- The partition table (`partitions.csv`) includes only NVS and factory app partitions.

## Board-specific notes

- Only tested on Waveshare ESP32-C3 Zero (ESP32-C3FH4, 4 MB SPI flash).
- Behavior on other ESP32-C3 modules may differ in USB timing and GPIO mapping.

---

*For the full validation contract, see [VALIDATION_PROFILE.md](../VALIDATION_PROFILE.md) and [BOOT_SEQUENCE.md](../BOOT_SEQUENCE.md).*
