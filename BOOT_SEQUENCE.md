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

Boot decisions are deterministic and based on explicit GPIO inputs:

- Recovery trigger: `GPIO9` held low (stable sampled)
- Update candidate trigger: `GPIO8` held low (stable sampled)
- Forced CRC mismatch trigger (test path): `GPIO7` held low (stable sampled)


Input sampling uses repeated low-level reads with fixed delay to avoid transient decisions.

## 5. Recovery behavior

- On recovery trigger, normal handoff is blocked.
- `BL_EVT:DECISION_RECOVERY` is emitted before entering hold behavior.
- LED remains MAGENTA bright steady.
- Heartbeat token is emitted every 5 seconds.

## 6. Update + CRC baseline behavior

- Update evaluation state is always emitted via `BL_EVT:UPDATE_CHECK`.
- CRC baseline uses descriptor CRC (`factory.offset` + `factory.size`) with `esp_rom_crc32_le`.
- On forced mismatch (`GPIO7`):
  - emits `BL_EVT:UPDATE_VERIFY_FAIL`
  - does not handoff app
  - enters recovery hold path
- On pass:
  - emits `BL_EVT:UPDATE_VERIFY_OK`
  - proceeds to `LOAD_APP` and `HANDOFF`

## 7. Fatal behavior

Fatal loop is entered on:
- bootloader init failure
- partition table load failure
- unexpected post-handoff return

Behavior:
- emits `BL_EVT:FATAL_RESET`
- emits `BL_EVT:FATAL_RESET_CODE:<code>`
- blinks RED at ~3Hz and periodically requests reset
