# Expected vs Observed - v0.1

Scope: historical baseline capture for normal boot, handoff confirmation, boot-context echo, and runtime heartbeat.

## Environment

- Date: 2026-03-08
- Release context: historical baseline capture archived as v0.1
- Board: Waveshare ESP32-C3 Zero
- Connection: USB CDC on the validated host serial port
- Baud: 115200
- ESP-IDF: v6.1 development workspace used at capture time
- Host Python: ESP-IDF Python 3.11 environment
- esptool: v5.2.dev4

## Commands used

- Build: `idf.py build`
- Flash: `idf.py -p <PORT> flash`
- Monitor/capture: `python scripts/watch_serial.py --port <PORT> --inactivity 15`
- Extra validators: manual token review against `docs/evidence/v0.1/logs/normal_boot.log`

## Normal boot and handoff

- Log: `docs/evidence/v0.1/logs/normal_boot.log`
- Expected: `DECISION_NORMAL -> APP_CRC_CHECK -> APP_CRC_OK -> LOAD_APP -> HANDOFF -> HANDOFF_APP -> APP_EVT:START -> APP_EVT:BOOTLOADER_HANDOFF_OK`
- Observed: the log contains the full normal-path token sequence followed by `APP_EVT:START` and `APP_EVT:BOOTLOADER_HANDOFF_OK`
- Status: PASS
- Rationale: bootloader CRC validation passed and control transferred cleanly to the test app

## Boot context echo

- Log: `docs/evidence/v0.1/logs/normal_boot.log`
- Expected: `APP_EVT:BOOT_CONTEXT:NORMAL`
- Observed: `APP_EVT:BOOT_CONTEXT:NORMAL`
- Status: PASS
- Rationale: the application received and reported the expected normal-path handoff context

## Fault hook baseline

- Log: `docs/evidence/v0.1/logs/normal_boot.log`
- Expected: `APP_EVT:FAULT_HOOK:DISABLED`
- Observed: `APP_EVT:FAULT_HOOK:DISABLED`
- Status: PASS
- Rationale: the default public baseline keeps the test fault hook disabled

## Runtime heartbeat

- Log: `docs/evidence/v0.1/logs/normal_boot.log`
- Expected: monotonic `APP_EVT:HEARTBEAT:<n>` lines at approximately 2-second intervals after app start
- Observed: the captured run shows heartbeat lines increasing monotonically from `1` onward with approximately 2-second spacing
- Status: PASS
- Rationale: runtime liveness remained stable after handoff during the observed capture window

## Notes

- This release is retained as a historical baseline and predates the broader v0.2 evidence matrix.
- `waveshare_esp32c3_zero_bootloader.bin` at capture time was recorded as a 1048576-byte partition image with CRC-32 `0xA7C1DB88`.
- The bootloader binary size recorded for this run was `0x4ca0`, leaving approximately 40% free space in the bootloader partition.
