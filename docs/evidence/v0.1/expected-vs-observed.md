# Expected vs Observed — v0.1

Scope: Validate Test App behavior required by audit contract section 3.

Observed artifact: `observed-serial.log`

- **Handoff confirmation**: Expected `APP_EVT:START` and `APP_EVT:BOOTLOADER_HANDOFF_OK` — Observed: PASS
- **Boot context echo**: Expected `APP_EVT:BOOT_CONTEXT:<NORMAL|RECOVERY|UPDATE>` — Observed: `APP_EVT:BOOT_CONTEXT:NORMAL` (PASS)
- **Runtime heartbeat**: Expected monotonic `APP_EVT:HEARTBEAT:<n>` at ~2s intervals — Observed: `APP_EVT:HEARTBEAT:1..7` (PASS)
- **Fault simulation hook baseline**: Expected disabled by default and documented token — Observed: `APP_EVT:FAULT_HOOK:DISABLED` (PASS)

Summary: Section 3 (Test App MUST behavior) passes for this run. Raw tokens are in `observed-serial.log`.

Notes:
- This evidence file captures a single run. For reproducibility targets (e.g., 10/10 or 20/20), re-run with the canonical watcher and collect additional logs under subsequent release folders (v0.2, etc.).
- The bootloader publishes the boot context using RTC scratch registers; if you observe `UNKNOWN` in rare runs, re-check RTC register collisions or boot timing.
