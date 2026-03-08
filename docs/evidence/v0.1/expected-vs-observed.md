# Expected vs Observed — v0.1

**Operator Context — 2026-03-08 Phase A Anchor Capture**

- **Board**: Waveshare ESP32-C3 Zero (QFN32, rev v0.4)
- **Hardware MAC**: 10:00:3b:de:5e:3c
- **Serial Port**: COM4
- **Baud Rate**: 115200
- **Build Date**: 2026-03-08 (run at 11:34–11:36 UTC)
- **Flash Method**: `idf -p COM4 flash` (esptool v5.2.dev4, high-speed 460800 bps)
- **Image Size**: 1048576 bytes (0x100000) — CRC-32: 0xA7C1DB88
- **Capture Tool**: `python scripts/watch_serial.py --inactivity 15`
- **Log Stored**: `docs/evidence/v0.1/logs/normal_boot.log`

**Scope**: Validate Test App behavior required by audit contract section 3.

---

## Validation Results

### Boot Sequence (Normal Path)
Expected: Bootloader initializes, validates app CRC, and hands off to app.

Observed (from log):
```
BL_EVT:HW_READY
BL_EVT:PARTITION_TABLE_OK
BL_EVT:DECISION_NORMAL
BL_EVT:APP_CRC_CHECK
BL_EVT:APP_CRC_OK
BL_EVT:LOAD_APP
BL_EVT:HANDOFF
BL_EVT:HANDOFF_APP
```
**Result**: ✓ PASS

### Handoff Confirmation
Expected: `APP_EVT:START` and `APP_EVT:BOOTLOADER_HANDOFF_OK` present in output.

Observed:
```
APP_EVT:START
APP_EVT:BOOTLOADER_HANDOFF_OK
```
**Result**: ✓ PASS

### Boot Context Echo
Expected: `APP_EVT:BOOT_CONTEXT:<NORMAL|RECOVERY|UPDATE>` with correct context.

Observed:
```
APP_EVT:BOOT_CONTEXT:NORMAL
```
**Result**: ✓ PASS (Normal path confirmed via RTC register)

### Fault Simulation Hook Baseline
Expected: Fault hook disabled by default and documented token emitted.

Observed:
```
APP_EVT:FAULT_HOOK:DISABLED
```
**Result**: ✓ PASS

### Runtime Heartbeat
Expected: Monotonic `APP_EVT:HEARTBEAT:<n>` at ~2s intervals.

Observed: First heartbeat = 1, subsequent increments by 1 at ~2000ms intervals.
Sample sequence (from log):
```
APP_EVT:HEARTBEAT:1      [timestamp: 11:35:40.836]
APP_EVT:HEARTBEAT:2      [timestamp: 11:35:42.810] → Δ ~1974ms
APP_EVT:HEARTBEAT:3      [timestamp: 11:35:44.810] → Δ ~2000ms
... (continues monotonically)
```
**Result**: ✓ PASS

---

## Build Artifact Check (v0.1)

- `waveshare_esp32c3_zero_bootloader.bin` size: 1048576 bytes (0x100000) (CRC-32: 0xA7C1DB88)
- Bootloader binary: 0x4ca0 bytes (40% free in bootloader partition)
- Compressed app image during flash: 74108 bytes

Summary: **COMPLETE**. All audit contract section 3 requirements validated.

---

## Notes

- This evidence file captures a single operational run. For reproducibility targets (e.g., 10/10 or 20/20), re-run with the canonical watcher and collect additional logs under subsequent release folders (v0.2, etc.).
- The bootloader publishes the boot context using RTC scratch registers; if `UNKNOWN` is observed in rare runs, re-check RTC register collisions or boot timing.
- Default chunk size for UPDATE phase: 1 KB (set in Phase B).
