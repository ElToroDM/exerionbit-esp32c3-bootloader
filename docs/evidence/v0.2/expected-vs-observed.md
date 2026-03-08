# Expected vs Observed - v0.2

Scope: UART update baseline (happy path + failure-case validation set).

## Environment

- Board: Waveshare ESP32-C3 Zero
- Port: COM4
- Baud: 115200
- Host Python: `C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe`

## Happy path

- Existing transcript: `docs/evidence/v0.2/logs/recovery_update.log`
- Status: PASS
- Confirmed sequence includes:
  - `READY_FOR_UPDATE`
  - `READY_FOR_CHUNK`
  - `CHUNK_OK:*`
  - `UPDATE_SESSION_END`
  - `APP_CRC_OK`
  - `LOAD_APP`
  - `HANDOFF`
  - `HANDOFF_APP`
  - `APP_EVT:START`

## Failure cases

### Corrupt chunk CRC16

- Expected: `BL_EVT:CHUNK_FAIL:<offset>` without app handoff.
- Attempt log: `docs/evidence/v0.2/logs/failure_corrupt_chunk.log`
- Observed: `BL_EVT:READY_FOR_CHUNK` -> `BL_EVT:CHUNK_FAIL:0`
- Status: PASS

### Out-of-bounds offset

- Expected: `BL_EVT:CHUNK_FAIL:<offset>` where `<offset>` is the invalid offset.
- Attempt log: `docs/evidence/v0.2/logs/failure_oob_offset.log`
- Observed: `BL_EVT:READY_FOR_CHUNK` -> `BL_EVT:CHUNK_FAIL:1048576`
- Status: PASS

### Invalid final CRC (tampered image)

- Expected: `BL_EVT:UPDATE_SESSION_END` then `BL_EVT:APP_CRC_FAIL` and recovery hold.
- Attempt log: `docs/evidence/v0.2/logs/failure_invalid_final_crc.log`
- Observed: full transfer completed, followed by `BL_EVT:UPDATE_SESSION_END`, `BL_EVT:APP_CRC_CHECK`, `BL_EVT:APP_CRC_FAIL`.
- Status: PASS

### Partial transfer / disconnect

- Expected: chunk timeout path (`BL_EVT:CHUNK_FAIL`) and continued ready loop or recovery after threshold.
- Attempt log: `docs/evidence/v0.2/logs/failure_partial_transfer_final.log`
- Observed: interrupted frame (`256/1024` bytes sent) followed by recovery heartbeat sequence (`BL_EVT:RECOVERY_HEARTBEAT:25..35`) in the same dedicated run.
- Status: PASS

## Notes

- Negative test runner added: `scripts/send_update_negative.py`
- Cases available:
  - `corrupt_chunk`
  - `oob_offset`
  - `invalid_final_crc`
  - `partial_transfer`

All four Day-2 negative cases are now validated on hardware.
