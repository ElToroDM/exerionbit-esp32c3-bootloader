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

1. `BL_EVT:DECISION_NORMAL`
2. `BL_EVT:UPDATE_CHECK`
3. `BL_EVT:UPDATE_VERIFY_OK`
4. `BL_EVT:HANDOFF_APP`
5. `APP_EVT:START`
6. `APP_EVT:BOOTLOADER_HANDOFF_OK`

### T2 Recovery path
Required:

- `BL_EVT:DECISION_RECOVERY`
- No `BL_EVT:HANDOFF_APP` while recovery is active

### T3 Update integrity pass
Required:

- `BL_EVT:UPDATE_CHECK`
- `BL_EVT:UPDATE_VERIFY_OK`

### T4 Update integrity fail
Required:

- `BL_EVT:UPDATE_VERIFY_FAIL`
- No handoff to invalid path

## 4. Evidence artifacts per release

Required:

- `docs/evidence/<release>/expected-vs-observed.md`
- `docs/evidence/<release>/logs/*.log`

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
