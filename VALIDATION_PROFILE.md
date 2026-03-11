# Validation Profile — ESP32-C3 Bootloader

Purpose: define the canonical validation execution profile and PASS/FAIL criteria for this repository.

## 1. Defaults (locked)

- Serial port auto-detection: enabled by default
- Manual port override: supported
- Default full validation timeout: 10 seconds
- Missing visual media (GIF/video): non-blocking warning for marketing packaging
- Protocol validation PASS/FAIL: based on deterministic token checks

## 2. Canonical token families

- Bootloader tokens: `BL_EVT:<TOKEN>[:VALUE]`
- Application tokens: `APP_EVT:<TOKEN>[:VALUE]`

## 3. Minimum pass criteria

### T1 Normal path
Required tokens (ordered):

1. `BL_EVT:DECISION_NORMAL` or `BL_EVT:DECISION_UPDATE` (update execution emits `DECISION_UPDATE`)
2. `BL_EVT:APP_CRC_CHECK`
3. `BL_EVT:APP_CRC_OK`
4. `BL_EVT:LOAD_APP`
5. `BL_EVT:HANDOFF`
6. `BL_EVT:HANDOFF_APP`
7. `APP_EVT:START`
8. `APP_EVT:BOOTLOADER_HANDOFF_OK`

### T2 Recovery path
Required:

- `BL_EVT:DECISION_RECOVERY`
- No `BL_EVT:HANDOFF_APP` while recovery is active

### T2b Recovery command console
Required:

- Deterministic help response via any alias (`?`, `h`, `help`):
	- `BL_EVT:RECOVERY_CMD_HELP`
	- `BL_RSP:help:status,reboot,update,erase,boot,help`
- Deterministic status response:
	- `BL_EVT:RECOVERY_CMD_STATUS`
	- `BL_RSP:status:recovery_active:ready`
- Unknown command handling:
	- `BL_EVT:RECOVERY_CMD_UNKNOWN`
	- `BL_RSP:error:unknown_command`
- `boot` command CRC gate behavior:
	- `BL_EVT:RECOVERY_CMD_BOOT`
	- `BL_EVT:APP_CRC_CHECK`
	- success path: `BL_EVT:APP_CRC_OK` + normal handoff sequence
	- fail path: `BL_EVT:APP_CRC_FAIL` and stay in recovery

### T3 Update integrity pass
Required:

- `BL_EVT:APP_CRC_CHECK`
- `BL_EVT:APP_CRC_OK`

### T4 Update integrity fail
Required:

- `BL_EVT:APP_CRC_FAIL`
- No handoff to invalid path

## 4. Evidence artifacts per release

Required:

- `docs/evidence/<release>/expected-vs-observed.md`
- `docs/evidence/<release>/logs/*.log`

`expected-vs-observed.md` must include these fields per test case:
- date/time and release tag or commit id
- board and connection context (target board, serial mode/port)
- toolchain/runtime context (ESP-IDF version, Python version, esptool version)
- command(s) used for build/flash/monitor or script execution
- expected token sequence and observed token sequence
- final result status (`PASS`, `FAIL`, or `PARTIAL`) with rationale

When live serial logs are unavailable (hardware window closed), the release record must:

- include build artifact metadata in `expected-vs-observed.md`
- mark the run status as `PARTIAL`
- schedule a follow-up capture to add `logs/*.log`

Optional but recommended for public packaging:

- `docs/evidence/<release>/screenshots/*`
- `docs/media/<release>/*`

## 5. Hardware availability note

This repository includes hardware-dependent checks.
When hardware is unavailable, you may run parser-level validation on existing logs and defer live serial execution to the next hardware window.

Recovery liveness note:
- Current baseline signals recovery idle liveness via LED behavior, not periodic serial heartbeat tokens.
