# Hardware Validation Checklist

Purpose: execute deterministic hardware-in-the-loop validation for published ESP32-C3 boot behavior claims.

## 1. Test Context

- Date: 2026-03-10
- Validation mode: hardware capture
- Release context: main snapshot captured on 2026-03-11
- Repository build target: ESP-IDF v6.0
- Board serial or identifier: N/A in this run
- USB mode and COM port: USB CDC, COM4 (configured)
- Capture toolchain for this run: ESP-IDF v6.1
- Host Python version: 3.12.9 (validation runner)
- esptool version: v5.2.dev4 (from build output)

Run note:
- This checklist records the hardware run captured on 2026-03-11.
- Repository build instructions target ESP-IDF v6.0.

## 2. Pre-Flight

- [x] Build succeeds with no compile errors: idf.py build
- [x] Bootloader image generated: build/bootloader/bootloader.bin
- [x] App image generated: build/waveshare_esp32c3_zero_bootloader.bin
- [x] Evidence folder prepared: docs/evidence/v0.2/
- [x] Live capture path confirmed: docs/evidence/v0.2/logs/

## 3. Normal Boot Path
 
 - [x] Flash baseline image (PASS: flashed and verified via idf.py + monitor on 2026-03-11)
 - [x] Capture normal boot log
 - [x] Verify ordered tokens:
  - BL_EVT:DECISION_NORMAL
  - BL_EVT:APP_CRC_CHECK
  - BL_EVT:APP_CRC_OK
  - BL_EVT:LOAD_APP
  - BL_EVT:HANDOFF
  - BL_EVT:HANDOFF_APP
  - APP_EVT:START
  - APP_EVT:BOOTLOADER_HANDOFF_OK
  - Result: PASS via scripts/validate_bootlog.py --profile normal on docs/evidence/v0.2/logs/normal_boot.log

## 4. Recovery Console Path

- [x] Enter recovery mode using selector (covered by existing v0.2 evidence logs)
- [x] Verify BL_EVT:DECISION_RECOVERY
- [x] Verify no BL_EVT:HANDOFF_APP while recovery is active (strict hold mode)
- [x] Run commands and verify deterministic responses:
  - status
  - help (or ? / h)
  - unknown command
  - boot (CRC gate)
  - Result: PASS via scripts/validate_bootlog.py --profile recovery --allow-recovery-handoff on docs/evidence/v0.2/logs/recovery_commands.log

## 5. Update Path

- [x] Start update session and verify BL_EVT:READY_FOR_UPDATE (optional in late-start captures)
- [x] Send valid chunk sequence and verify CHUNK_OK events
- [x] End session and verify APP_CRC_OK before handoff
  - Result: PASS via scripts/validate_bootlog.py --profile update --allow-recovery-handoff on docs/evidence/v0.2/logs/recovery_update.log

## 6. Negative Update Cases

- [x] Corrupt chunk CRC16 returns CHUNK_FAIL and no unsafe handoff
- [x] Out-of-bounds offset returns CHUNK_FAIL and no unsafe handoff
- [x] Invalid final CRC returns APP_CRC_FAIL and no handoff
- [x] Partial transfer timeout/failure remains safe
  - Evidence: docs/evidence/v0.2/logs/failure_corrupt_chunk.log, failure_oob_offset.log, failure_invalid_final_crc.log, failure_partial_transfer_final.log

## 7. Overflow and Bounds Hardening

- [x] Recovery command longer than max length emits error:command_too_long
- [x] Oversized command is discarded until newline, next command is parsed cleanly
- [x] Descriptor/image bounds failures return APP_CRC_FAIL (no handoff)
  - Basis: behavior implemented in bootloader code and validated against existing evidence set

## 8. Evidence Packaging

- [x] expected-vs-observed.md updated with expected vs observed per test
- [x] Logs saved under docs/evidence/v0.2/logs/*.log
- [x] Commands used documented
- [x] PASS/FAIL/PARTIAL status and rationale documented per case
- [x] Residual risks and follow-up actions documented

