# Known Limits — Waveshare ESP32-C3 Zero Custom Bootloader

**Version:** v0.2  
**Date:** 2026-03-08

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
- ESP-IDF integrated monitor may not show local keyboard echo while still sending input bytes
  to the target. Recovery command validation should rely on `BL_EVT:RECOVERY_CMD_*` and `BL_RSP:*`
  responses rather than local input echo.

## Security scope

- Secure Boot is **disabled** in this public baseline. This repository demonstrates boot
  decision logic, LED/token signaling, and structural correctness only.
- Flash Encryption is **disabled**.
- Production hardening (key provisioning, anti-tamper, rollback protection) is out of scope
  for this proof asset.

## Component override mechanism

- The `main` component in `bootloader_components/main/` replaces ESP-IDF's default
  bootloader main component. This override is validated in CI (see `.github/workflows/`).
- Build size is tracked as a proxy indicator that the override is active (see `VALIDATION_PROFILE.md`).

## OTA and update protocol

- UART update receive/write protocol is implemented as a minimal deterministic baseline in update mode.
- Transport is intentionally simple: ASCII frame header (`[LEN:OFFSET:CRC16]`) + binary payload.
- Current implementation is host-driven and single-session only (no resume).
- The partition table (`partitions.csv`) includes only NVS and factory app partitions (no OTA slot scheme).

## Integrity check scope

- Full app payload CRC verification is implemented with descriptor-based metadata (`image_size` + `crc32` at partition tail).
- Chunk transfer uses CRC16 per frame for transport corruption detection.
- This remains an integrity baseline only; authenticity and anti-rollback are out of scope.

## Power-loss and partial-transfer behavior

- Interrupted transfer handling is fail-safe but minimal:
  - Missing chunk bytes eventually trigger `CHUNK_FAIL` after timeout.
  - Repeated failures (10 consecutive) abort update mode to recovery.
- No transactional resume exists across reset/power-loss; host must restart transfer from offset 0.
- No dual-bank rollback path exists in public baseline.

## Protocol and operability constraints

- Maximum chunk size is fixed to 1024 bytes in bootloader.
- Header and payload offsets must be 4-byte aligned.
- Per-chunk timeout is 120 seconds to tolerate reconnect races; this increases worst-case failure detection latency.
- Operator must intentionally select UPDATE mode via single-button selector (GPIO9).
- Failure-case validation requires physical access because UPDATE mode entry is hardware-driven.
- Recovery idle liveness indication is LED-based in the current baseline (violet gentle blink),
  not periodic serial heartbeat tokens.

## Board-specific notes

- Only tested on Waveshare ESP32-C3 Zero (ESP32-C3FH4, 4 MB SPI flash).
- Behavior on other ESP32-C3 modules may differ in USB timing and GPIO mapping.

---

*For the full validation contract, see [VALIDATION_PROFILE.md](../VALIDATION_PROFILE.md) and [BOOT_SEQUENCE.md](../BOOT_SEQUENCE.md).*
