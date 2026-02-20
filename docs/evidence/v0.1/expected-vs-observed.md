# Expected vs Observed — v0.1

Scope: Validate Test App behavior required by audit contract section 3.

Observed artifacts:

- Build outputs from `build/` (no serial log captured yet)

- **Handoff confirmation**: Expected `APP_EVT:START` and `APP_EVT:BOOTLOADER_HANDOFF_OK` — Observed: PENDING (no serial log)
- **Boot context echo**: Expected `APP_EVT:BOOT_CONTEXT:<NORMAL|RECOVERY|UPDATE>` — Observed: PENDING (no serial log)
- **Runtime heartbeat**: Expected monotonic `APP_EVT:HEARTBEAT:<n>` at ~2s intervals — Observed: PENDING (no serial log)
- **Fault simulation hook baseline**: Expected disabled by default and documented token — Observed: PENDING (no serial log)

Build artifact check:

- `waveshare_esp32c3_zero_bootloader.bin` size: 134016 bytes (2026-02-20 19:13:35)
- `waveshare_esp32c3_zero_bootloader.elf` size: 3354780 bytes (2026-02-20 19:13:35)
- `waveshare_esp32c3_zero_bootloader.map` size: 2865417 bytes (2026-02-20 19:13:35)

Summary: PARTIAL. Build artifacts captured; serial log still required to validate Test App behavior.

Notes:
- Capture a full serial log using `scripts/watch_serial.py` and store under `docs/evidence/v0.1/logs/` as `observed-serial.log`.
- This evidence file captures a single run. For reproducibility targets (e.g., 10/10 or 20/20), re-run with the canonical watcher and collect additional logs under subsequent release folders (v0.2, etc.).
- The bootloader publishes the boot context using RTC scratch registers; if you observe `UNKNOWN` in rare runs, re-check RTC register collisions or boot timing.
