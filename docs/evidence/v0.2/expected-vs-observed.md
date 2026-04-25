# Expected vs Observed - v0.2

Scope: normal boot, update protocol, recovery command console, and failure-case validation.

## Environment

- Date: 2026-03-10
- Last validated: 2026-03-11 (flashed and monitored using `idf.py` + `idf_monitor`)
- Release context: main snapshot validated on 2026-03-11
- Repository build instructions target: ESP-IDF v6.0
- Board: Waveshare ESP32-C3 Zero
- Connection: USB CDC on the validated host serial port
- Baud: 115200
- Capture toolchain for this run: ESP-IDF v6.1
- Host Python: ESP-IDF Python 3.11 environment
- esptool: v5.2.dev4 (from build output)

## Commands used

- Build: `idf.py build`
- Flash: `idf.py -p <PORT> flash`
- Monitor/capture: `idf.py -p <PORT> monitor` and `python scripts/watch_serial.py --port <PORT> --inactivity 10`
- Negative update tests: `python scripts/send_update_negative.py --port <PORT> --case <case_name>`

## Normal boot

- Log: `docs/evidence/v0.2/logs/normal_boot.log`
- Expected: `DECISION_NORMAL -> APP_CRC_CHECK -> APP_CRC_OK -> LOAD_APP -> HANDOFF -> HANDOFF_APP -> APP_EVT:START`
- Observed: full expected sequence present, plus `APP_EVT:BOOTLOADER_HANDOFF_OK` and `APP_EVT:BOOT_CONTEXT:NORMAL`
- Status: PASS

## Recovery command console

- Log: `docs/evidence/v0.2/logs/recovery_commands.log`
- Expected:
  - Recovery entry token: `BL_EVT:DECISION_RECOVERY`
  - Help aliases emit deterministic line: `BL_RSP:help:status,reboot,update,erase,boot,help`
  - Unknown command emits deterministic line: `BL_RSP:error:unknown_command`
  - `boot` command is CRC-gated and transitions to normal path on pass
- Observed:
  - `BL_EVT:DECISION_RECOVERY`
  - `BL_EVT:RECOVERY_CMD_HELP` + deterministic help response
  - `BL_EVT:RECOVERY_CMD_STATUS` + deterministic status response
  - `BL_EVT:RECOVERY_CMD_UNKNOWN` + deterministic unknown response
  - `BL_EVT:RECOVERY_CMD_BOOT` + `APP_CRC_OK` + `RECOVERY_CMD_BOOT_OK` + normal handoff tokens
- Status: PASS

## Update happy path

- Log: `docs/evidence/v0.2/logs/recovery_update.log`
- Expected: `READY_FOR_UPDATE -> READY_FOR_CHUNK -> CHUNK_OK:* -> UPDATE_SESSION_END -> APP_CRC_OK -> LOAD_APP -> HANDOFF`
- Observed: full expected sequence present
- Status: PASS

## Failure cases

### Corrupt chunk CRC16

- Log: `docs/evidence/v0.2/logs/failure_corrupt_chunk.log`
- Expected: `BL_EVT:CHUNK_FAIL:<offset>` without app handoff
- Observed: `BL_EVT:READY_FOR_CHUNK -> BL_EVT:CHUNK_FAIL:0`
- Status: PASS

### Out-of-bounds offset

- Log: `docs/evidence/v0.2/logs/failure_oob_offset.log`
- Expected: `BL_EVT:CHUNK_FAIL:<offset>` for invalid offset
- Observed: `BL_EVT:READY_FOR_CHUNK -> BL_EVT:CHUNK_FAIL:1048576`
- Status: PASS

### Invalid final CRC (tampered image)

- Log: `docs/evidence/v0.2/logs/failure_invalid_final_crc.log`
- Expected: `BL_EVT:UPDATE_SESSION_END` then `BL_EVT:APP_CRC_FAIL` and no handoff
- Observed: full transfer completed, then `BL_EVT:UPDATE_SESSION_END`, `BL_EVT:APP_CRC_CHECK`, `BL_EVT:APP_CRC_FAIL`
- Status: PASS

### Partial transfer / disconnect

- Log: `docs/evidence/v0.2/logs/failure_partial_transfer_final.log`
- Expected: timeout/fail path (`BL_EVT:CHUNK_FAIL`) and safe non-handoff behavior
- Observed: interrupted frame (`256/1024` bytes) and fail-safe recovery behavior
- Status: PASS

## Size and media references

- Size snapshot: `docs/evidence/v0.2/size_report.txt`
- Normal boot video: `docs/media/v0.2/waveshare-esp32-c3-zero-custom-bootloader-normal-boot.mp4`
- Media inventory: `docs/media/v0.2/README.md`

## Notes

- Negative test runner: `scripts/send_update_negative.py`
- Cases: `corrupt_chunk`, `oob_offset`, `invalid_final_crc`, `partial_transfer`
- Recovery idle liveness in this implementation is LED-only (violet gentle blink), not periodic serial heartbeat tokens.
- Repository build instructions target ESP-IDF v6.0. This evidence set was captured on 2026-03-11.

## Validation summary

- Evidence validated: `docs/evidence/v0.2`
- Validation commands run:
  - `python scripts/validate_evidence_contract.py --release v0.2` → FINAL: PASS (2026-03-11)
  - `python scripts/validate_bootlog.py --profile normal --log docs/evidence/v0.2/logs/normal_boot.log` → FINAL: PASS
  - `python scripts/validate_bootlog.py --profile recovery --allow-recovery-handoff --log docs/evidence/v0.2/logs/recovery_commands.log` → FINAL: PASS

The referenced logs were captured during the live flash/monitor session on 2026-03-11 and are maintained under `docs/evidence/v0.2/` as part of the release evidence set.

## Validator outputs

Normal boot validator (command):
`python scripts/validate_bootlog.py --profile normal --log docs/evidence/v0.2/logs/normal_boot.log`

```
Log: docs\evidence\v0.2\logs\normal_boot.log
Profile: normal
Lines: 15
[PASS] BL_EVT:DECISION_ANY
[PASS] BL_EVT:APP_CRC_CHECK
[PASS] BL_EVT:APP_CRC_OK
[PASS] BL_EVT:LOAD_APP
[PASS] BL_EVT:HANDOFF
[PASS] BL_EVT:HANDOFF_APP
[PASS] APP_EVT:START
[PASS] APP_EVT:BOOTLOADER_HANDOFF_OK
[PASS] Recovery path not observed in this log (non-blocking).
[PASS] CRC fail path not observed in this log (non-blocking).

FINAL: PASS
```

Recovery console validator (command):
`python scripts/validate_bootlog.py --profile recovery --allow-recovery-handoff --log docs/evidence/v0.2/logs/recovery_commands.log`

```
Log: docs\evidence\v0.2\logs\recovery_commands.log
Profile: recovery
Lines: 38
[PASS] BL_EVT:DECISION_RECOVERY
[PASS] BL_EVT:RECOVERY_CMD_STATUS
[PASS] Recovery token observed with later handoff (allowed by profile).
[PASS] CRC fail path not observed in this log (non-blocking).

FINAL: PASS
```
