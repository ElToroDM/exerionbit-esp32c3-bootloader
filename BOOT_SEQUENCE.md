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

Note: `BL_EVT:UPDATE_CHECK` and `BL_EVT:UPDATE_VERIFY_OK` are emitted in **all** boot paths
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
- `BL_EVT:UPDATE_CHECK`
- `BL_EVT:UPDATE_VERIFY_OK`
- `BL_EVT:UPDATE_VERIFY_FAIL`
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

## 3. LED mapping

Normal path:
- `INIT`: MAGENTA tenue `(8,0,8)`
- `HW_READY`: BLUE soft `(0,8,16)`
- `PARTITION_TABLE_OK`: CYAN `(0,12,12)`
- `DECISION_NORMAL`: AMBER `(16,10,0)`
- `LOAD_APP`: GREEN soft `(0,16,4)`
- `HANDOFF`: GREEN bright `(0,20,0)`
- before app jump: OFF `(0,0,0)`

Recovery/update/fatal:
- `DECISION_RECOVERY`: MAGENTA bright steady `(20,0,20)`
- `UPDATE_CHECK`: YELLOW pulse `(16,10,0 ↔ 8,5,0 @ 2Hz)`
- `UPDATE_VERIFY_OK`: GREEN bright `(0,20,0)` for `200ms`
- `UPDATE_VERIFY_FAIL`: RED bright `(20,0,0)` for `300ms`
- `FATAL_RESET`: RED blink `(20,0,0 @ ~3Hz)` until reset

## 4. Explicit decision inputs

Boot decisions are deterministic and based on a single explicit input:

- `GPIO9` (BOOT button)

Input sampling uses repeated low-level reads with fixed delay to avoid transient decisions.

Selector rules (`GPIO9`):
- Selector must be armed only after reset boot has started (no requirement to hold button during reset).
- `GUARD_RELEASE_MS` enforces a short post-reset stabilization and release guard.
- If button remains pressed for `ARM_HOLD_MS`, selector mode is armed and emits `BL_EVT:MODE_SELECT_ARMED`.
- On release after arming, selector enters first mode and emits `BL_EVT:MODE_SELECTED:<mode>`.
- Short press (`SHORT_PRESS_MIN_MS..SHORT_PRESS_MAX_MS`) cycles to next mode (circular ring).
- Long press (`LONG_PRESS_MIN_MS`) executes selected mode and emits `BL_EVT:MODE_EXECUTE:<mode>`.

Default selector mode ring (circular):
- `UPDATE` -> `RECOVERY` -> `NORMAL` -> `UPDATE`

Safety timeout:
- if no valid interaction within `SELECT_TIMEOUT_MS`, boot proceeds in `NORMAL` path.

## 5. Recovery behavior

- On recovery trigger, normal handoff is blocked.
- `BL_EVT:DECISION_RECOVERY` is emitted before entering hold behavior.
- LED remains MAGENTA bright steady.
- Heartbeat token is emitted every 5 seconds.

Recovery console baseline (public scope):
- `status`: print build/version, reset reason, last boot error code.
- `reboot`: perform controlled reboot.
- `enter_update`: exit hold and run update path.

## 6. Update + CRC baseline behavior

- Update mode is entered through `GPIO9` selector (`MODE_EXECUTE:UPDATE`) or recovery command (`enter_update`).
- Update evaluation state is emitted via `BL_EVT:UPDATE_CHECK`.
- CRC baseline uses descriptor CRC (`factory.offset` + `factory.size`) with `esp_rom_crc32_le`.
- On verify fail:
  - emits `BL_EVT:UPDATE_VERIFY_FAIL`
  - does not handoff app
  - enters recovery hold path
- On pass:
  - emits `BL_EVT:UPDATE_VERIFY_OK`
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
- `GUARD_RELEASE_MS = 120`
- `ARM_HOLD_MS = 600`
- `SHORT_PRESS_MIN_MS = 60`
- `SHORT_PRESS_MAX_MS = 250`
- `LONG_PRESS_MIN_MS = 700`
- `SELECT_TIMEOUT_MS = 2500`

These values are selected for:
- compatibility with single-button boards
- deterministic operator behavior
- low accidental mode activation during normal resets
