# Boot Sequence Specification — ESP32-C3 Bootloader

Purpose: define the implemented and auditable boot state behavior, serial tokens, LED mapping, and deterministic timing.

## 1. Deterministic timing model

- `TOTAL_VISUAL_MS = 4000` (normal visual sequence)
- `USB_RECONNECT_MS = 1000` (fixed reconnect window)
- State durations derived from `TOTAL_VISUAL_MS` by percentage (no per-state hardcoded literals outside mapping table)

Normal path percentages:
- `INIT`: 10% (400ms)
- `HW_READY`: 10% (400ms)
- `PARTITION_TABLE_OK`: 15% (600ms)
- `DECISION_NORMAL`: 15% (600ms)
- `LOAD_APP`: 20% (800ms)
- `HANDOFF`: 30% (1200ms)

Note: `BL_EVT:APP_CRC_CHECK` and `BL_EVT:APP_CRC_OK` are emitted in **all** boot paths
between `DECISION_NORMAL` and `LOAD_APP`. Their visual duration uses a fixed 4-pulse
sequence at `UPDATE_PULSE_HZ` and is not allocated within `TOTAL_VISUAL_MS`.

## 2. Serial token contract

Token format:

`BL_EVT:<TOKEN>[:VALUE]`

Implemented state tokens:
- `BL_EVT:INIT`
- `BL_EVT:HW_READY`
- `BL_EVT:PARTITION_TABLE_OK`
- `BL_EVT:DECISION_NORMAL`
- `BL_EVT:DECISION_RECOVERY`
- `BL_EVT:APP_CRC_CHECK`
- `BL_EVT:APP_CRC_OK`
- `BL_EVT:APP_CRC_FAIL`
- `BL_EVT:LOAD_APP`
- `BL_EVT:HANDOFF`
- `BL_EVT:HANDOFF_APP`
- `BL_EVT:FATAL_RESET`
- `BL_EVT:MODE_SELECT_ARMED`
- `BL_EVT:MODE_SELECTED:<mode>`
- `BL_EVT:MODE_EXECUTE:<mode>`

Additional diagnostic tokens:
- `BL_EVT:RECOVERY_HEARTBEAT:<n>`
- `BL_EVT:FATAL_RESET_CODE:<code>`

Token sequence note:
- `BL_EVT:HANDOFF`: bootloader cleanup complete, app entry pending (LED GREEN bright).
- `BL_EVT:HANDOFF_APP`: CPU jump to application imminent (LED OFF); last token emitted before control transfers.

## 3. LED mapping

Normal path:
- `INIT`: MAGENTA tenue `(8,0,8)`
- `HW_READY`: BLUE soft `(0,8,16)`
- `PARTITION_TABLE_OK`: CYAN `(0,12,12)`
- `DECISION_NORMAL`: AMBER `(16,10,0)`
- `LOAD_APP`: GREEN soft `(0,16,4)`
- `HANDOFF`: GREEN bright `(0,20,0)`
- `HANDOFF_APP` (immediate pre-jump): OFF `(0,0,0)` — last token before CPU transfers to application

Recovery/update/fatal:
- `DECISION_RECOVERY`: MAGENTA bright steady `(20,0,20)`
- `APP_CRC_CHECK`: YELLOW pulse `(16,10,0 ↔ 8,5,0 @ 2Hz)` — emitted in all paths
- `APP_CRC_OK`: GREEN brief `(0,20,0)` for `200ms` — emitted in all paths
- `APP_CRC_FAIL`: RED brief `(20,0,0)` for `300ms` — emitted in all paths
- `FATAL_RESET`: RED blink `(20,0,0 @ ~3Hz)` until reset

## 4. Explicit decision inputs

Boot decisions are deterministic and based on a single explicit input:

- `GPIO9` (BOOT button)

Input sampling uses repeated low-level reads with fixed delay to avoid transient decisions.

Selector rules (`GPIO9`):
- Selector can be armed immediately (holding BOOT from the reset/power-on moment).
- A short stable hold (`ARM_DETECT_MS`, currently 60 ms) arms the selector and emits `BL_EVT:MODE_SELECT_ARMED`.
- On release after arming the selector enters the first mode and emits `BL_EVT:MODE_SELECTED:<mode>`; the release itself does not execute a mode.
- Subsequent interaction semantics (after release):
  - Short press (`SHORT_PRESS_MIN_MS..SHORT_PRESS_MAX_MS`) cycles to the next mode in the ring.
  - Long press (`LONG_PRESS_MIN_MS`) executes the currently selected mode and emits `BL_EVT:MODE_EXECUTE:<mode>`.
- Default selector mode ring (circular): `UPDATE` -> `RECOVERY` -> `NORMAL` -> `UPDATE`.
- There is no selection timeout in the baseline: once armed the selector remains active until a long‑press executes a mode or the operator cancels by power/reset.

## 5. Recovery behavior

- On recovery trigger, normal handoff is blocked.
- `BL_EVT:DECISION_RECOVERY` is emitted before entering hold behavior.
- LED remains MAGENTA bright steady.
- Heartbeat token is emitted every 5 seconds.

Recovery console status:
- Current public baseline: command parser is not implemented yet.
- Recovery behavior is hold + heartbeat only (`BL_EVT:RECOVERY_HEARTBEAT:<n>`).
- Planned command baseline (next cycle): `status`, `reboot`, `enter_update`, `erase_app`, `boot`.

## 6. App CRC verification (all paths)

- App image CRC is verified in **all** boot paths between `DECISION_NORMAL` and `LOAD_APP`.
- **Security Scope:** This baseline check provides *integrity* validation against corruption or incomplete updates. Cryptographic *authenticity* (hardware Secure Boot) is configured separately via eFuses and is outside the scope of this sequence document.
- In update mode (`MODE_EXECUTE:UPDATE` or recovery `enter_update`), the same check validates the newly targeted image before handoff.
- CRC check is announced via `BL_EVT:APP_CRC_CHECK`.
- Current CRC baseline uses descriptor CRC (`factory.offset` + `factory.size`) with `esp_rom_crc32_le`.
- Full app payload CRC verification is planned for the next cycle.
- On verify fail:
  - emits `BL_EVT:APP_CRC_FAIL`
  - does not handoff app
  - enters recovery hold path
- On pass:
  - emits `BL_EVT:APP_CRC_OK`
  - proceeds to `LOAD_APP` and `HANDOFF`

Test-path note:
- Forced CRC mismatch for validation is enabled by build/test configuration, not by dedicated public GPIO pins.

## 7. Fatal behavior

Fatal loop is entered on:
- bootloader init failure
- partition table load failure
- unexpected post-handoff return

Behavior:
- emits `BL_EVT:FATAL_RESET`
- emits `BL_EVT:FATAL_RESET_CODE:<code>`
- blinks RED at ~3Hz and periodically requests reset

## 8. Selector timing profile (baseline)

Reference timing constants (subject to tuning per board stability):
- `ARM_DETECT_MS = 60` (short stable hold to arm selector)
- `SHORT_PRESS_MIN_MS = 60`
- `SHORT_PRESS_MAX_MS = 250`
- `LONG_PRESS_MIN_MS = 700`

These values are selected for:
- compatibility with single-button boards
- deterministic operator behavior
- low accidental mode activation during normal resets
