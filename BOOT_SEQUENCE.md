# Boot Sequence Specification — ESP32-C3 Bootloader

**Purpose**: Define the exact boot sequence, LED states, serial tokens, and timing for the ExerionBit ESP32-C3 custom bootloader.

**Status**: Specification approved, pending implementation.

**References**: 
- Audit contract: `ops/exerionbit-esp32c3-bootloader_audit_contract.md`
- Master plan: `ops/exerionbit-esp32c3-bootloader.md`

---

## 1. Normal Boot Path (Happy Path)

### Phase sequence

| Phase | Duration | LED Color | RGB Values | Serial Token | Purpose |
|-------|----------|-----------|------------|--------------|---------|
| **INIT** | 400ms (10%) | MAGENTA tenue | (8,0,8) | `BL_EVT:INIT` | Hardware initialization start |
| **HW_READY** | 400ms (10%) | BLUE soft | (0,8,16) | `BL_EVT:HW_READY` | Peripherals initialized |
| **PARTITION_OK** | 600ms (15%) | CYAN | (0,12,12) | `BL_EVT:PARTITION_TABLE_OK` | Partition table validated |
| **USB_RECONNECT** | 1000ms (fixed) | *hold CYAN* | (0,12,12) | *(log buffer flush after 1000ms)* | Allow USB CDC reconnection |
| **DECISION** | 600ms (15%) | AMBER | (16,10,0) | `BL_EVT:DECISION_NORMAL` | Boot path decision made |
| **LOAD_APP** | 800ms (20%) | GREEN soft | (0,16,4) | `BL_EVT:LOAD_APP` | Loading application image |
| **HANDOFF** | 1200ms (30%) | GREEN bright | (0,20,0) | `BL_EVT:HANDOFF` | Ready to jump to app |
| **JUMP** | 0ms | OFF | (0,0,0) | `BL_EVT:HANDOFF_APP:<partition>` | Transfer control to app |

**Timing constants (public demo profile):**
- `TOTAL_VISUAL_MS = 4000`
- `USB_RECONNECT_MS = 1000`
- `TOTAL_BOOT_DEMO_MS ≈ 5000`

**Total time**: ~5.0 seconds (1000ms reconnect + 4000ms visual sequence)

### Implementation notes

- **Token emission**: Print `BL_EVT:` tokens immediately with `bootlog_print()` for real-time visibility
- **Buffering strategy**: Use `bootlog_buffer()` for verbose messages; flush buffer after USB reconnect window
- **LED transitions**: Clean instant transitions (no fade); deterministic timing via `ets_delay_us()`
- **Duration model**: Phase durations MUST be calculated as fractions of `TOTAL_VISUAL_MS`
- **USB reconnect window**: Fixed 1000ms at CYAN phase allows host to reattach serial monitor reliably
- **Handoff LED**: GREEN bright held for 1200ms provides an unmistakable "success" signal before app takeover

---

## 2. Recovery Boot Path

### Trigger conditions (explicit and documented)

1. **GPIO9 (BOOT button) held during reset**
2. **NVS flag `force_recovery=1` set**
3. **Watchdog reset after failed boot attempts** (future: implement boot counter)

### Phase sequence

| Phase | Duration | LED Color | RGB Values | Serial Token | Purpose |
|-------|----------|-----------|------------|--------------|---------|
| **INIT** | 400ms (10%) | MAGENTA tenue | (8,0,8) | `BL_EVT:INIT` | Hardware initialization |
| **HW_READY** | 400ms (10%) | BLUE soft | (0,8,16) | `BL_EVT:HW_READY` | Peripherals ready |
| **PARTITION_OK** | 600ms (15%) | CYAN | (0,12,12) | `BL_EVT:PARTITION_TABLE_OK` | Partition table OK |
| **USB_RECONNECT** | 1000ms (fixed) | *hold CYAN* | (0,12,12) | *(buffer flush)* | USB reconnect window |
| **DECISION_RECOVERY** | steady | MAGENTA bright | (20,0,20) | `BL_EVT:DECISION_RECOVERY` | Enter recovery mode |
| **RECOVERY_ACTIVE** | hold | MAGENTA bright | (20,0,20) | `BL_EVT:RECOVERY_ACTIVE` | Wait for recovery action |

**Recovery behavior**:
- Do NOT proceed to normal app handoff
- Hold MAGENTA LED steady (clear visual indicator)
- Emit periodic heartbeat: `BL_EVT:RECOVERY_HEARTBEAT:<n>` every 5s
- Wait for serial command / future OTA trigger / manual reset

---

## 3. Update Path (Integrity Check)

### Trigger conditions

1. **OTA partition marked as pending** (via `esp_ota_*` APIs)
2. **Update marker flag in NVS**: `update_pending=1`

### Phase sequence (additional steps in normal flow)

| Phase | Duration | LED Color | RGB Values | Serial Token | Purpose |
|-------|----------|-----------|------------|--------------|---------|
| *(normal init phases)* | — | — | — | — | — |
| **UPDATE_CHECK** | variable | YELLOW pulsing | (16,10,0)↔(8,5,0) @ 2Hz | `BL_EVT:UPDATE_CHECK` | Checking update candidate |
| **UPDATE_VERIFY_OK** | 200ms | GREEN bright | (0,20,0) | `BL_EVT:UPDATE_VERIFY_OK` | CRC/hash validated |
| **UPDATE_VERIFY_FAIL** | 300ms | RED bright | (20,0,0) | `BL_EVT:UPDATE_VERIFY_FAIL` | CRC/hash mismatch |

**On UPDATE_VERIFY_OK**: Proceed to `LOAD_APP` with validated image  
**On UPDATE_VERIFY_FAIL**: 
- Emit `BL_EVT:FALLBACK_TO_FACTORY`
- Set LED to AMBER for 200ms
- Load last known good image (factory partition)
- Continue to `HANDOFF`

---

## 4. Fatal Error Path

### Trigger conditions

- `bootloader_init()` fails
- Partition table load fails critically (corrupted flash)
- App image load fails with no fallback available

### Behavior

| Phase | Duration | LED Color | RGB Values | Serial Token | Purpose |
|-------|----------|-----------|------------|--------------|---------|
| **FATAL** | continuous | RED fast blink | (20,0,0) @ 3Hz | `BL_EVT:FATAL_RESET:<code>` | Unrecoverable error |
| **RESET** | after 3s | — | — | *(reset triggered)* | Automatic reset attempt |

**Blink pattern**: 166ms ON, 166ms OFF (3Hz)  
**Error codes** (future): 
- `0x01`: Hardware init failed
- `0x02`: Partition table corrupted
- `0x03`: No valid app image

---

## 5. Serial Token Format

### Standard format

```
BL_EVT:<TOKEN>[:OPTIONAL_PAYLOAD]
```

### Required tokens (minimum viable)

- `BL_EVT:INIT`
- `BL_EVT:HW_READY`
- `BL_EVT:PARTITION_TABLE_OK`
- `BL_EVT:PARTITION_TABLE_FAIL`
- `BL_EVT:DECISION_NORMAL`
- `BL_EVT:DECISION_RECOVERY`
- `BL_EVT:UPDATE_CHECK`
- `BL_EVT:UPDATE_VERIFY_OK`
- `BL_EVT:UPDATE_VERIFY_FAIL`
- `BL_EVT:LOAD_APP`
- `BL_EVT:HANDOFF`
- `BL_EVT:HANDOFF_APP:<partition_name>`
- `BL_EVT:FATAL_RESET:<error_code>`
- `BL_EVT:RECOVERY_ACTIVE`
- `BL_EVT:RECOVERY_HEARTBEAT:<count>`
- `BL_EVT:FALLBACK_TO_FACTORY`

### Token rules

- **Uniqueness**: Each token has exactly one meaning; no reuse
- **Parsability**: Machine-readable format for HIL validator
- **Stability**: Tokens stable across releases (breaking changes require major version bump)
- **Immediate emission**: Critical tokens printed immediately (not buffered)

---

## 6. Test Application Tokens

### Required app tokens (audit contract compliance)

- `APP_EVT:START` — app_main() entry
- `APP_EVT:BOOTLOADER_HANDOFF_OK` — confirms successful bootloader→app transition
- `APP_EVT:BOOT_CONTEXT:NORMAL` — reports boot path received from bootloader
- `APP_EVT:BOOT_CONTEXT:RECOVERY` — recovery path indication
- `APP_EVT:HEARTBEAT:<n>` — monotonic counter, emitted every 2s

### Boot context passing (future)

Use custom RTC memory or NVS flag to pass boot context from bootloader to app:
- `boot_context=NORMAL|RECOVERY|UPDATE`
- App reads this value at startup and echoes via `APP_EVT:BOOT_CONTEXT:<value>`

---

## 7. Implementation Checklist

When implementing this specification:

- [ ] Add `BL_EVT:` token printing to all state transitions
- [ ] Implement LED phase table with exact RGB values and timings
- [ ] Add GPIO9 recovery trigger detection
- [ ] Add NVS flag support for `force_recovery`
- [ ] Implement basic CRC check for update verification
- [ ] Add fallback logic on `UPDATE_VERIFY_FAIL`
- [ ] Update `main.c` test app with `APP_EVT:` tokens
- [ ] Add boot context passing mechanism (RTC memory or NVS)
- [ ] Update `scripts/watch_serial.py` to validate token sequence
- [ ] Create LED capture GIF with annotated phases
- [ ] Document expected token sequence in `docs/evidence/v1.0/expected-vs-observed.md`

---

## 8. Evidence Requirements (per release)

Each release must include:

1. **LED sequence GIF** (`docs/media/v1.0/led_sequence.gif`):
   - Full boot cycle visible
   - Each phase color clearly distinguishable
   - Timing annotated (optional overlay)

2. **Serial log capture** (`docs/evidence/v1.0/logs/normal_boot.txt`):
   - Complete token sequence from reset to app heartbeat
   - Captured with `scripts/watch_serial.py`
   - Line numbers and timestamps preserved

3. **Expected vs observed checklist** (`docs/evidence/v1.0/expected-vs-observed.md`):
   - Token-by-token comparison
   - LED phase verification
   - Timing measurements

4. **Demo video** (`docs/media/v1.0/demo.mp4`):
   - 30-60 seconds
   - Shows board, LED, and serial output
   - Demonstrates normal boot + one special path (recovery or update)

---

## 9. Timing Budget Rationale

**USB reconnect window (1000ms)**:
- ESP32-C3 USB CDC disconnects on reset
- Host OS needs time to re-enumerate device
- Serial monitor needs time to reattach
- 1000ms prioritizes demo reliability over minimal latency

**LED phase durations**:
- Derived from `TOTAL_VISUAL_MS = 4000` using fixed fractions per state
- Long enough for client-visible live demos and clean video capture
- HANDOFF phase longest (30% = 1200ms) to emphasize "success" state before jump

**LED brightness (reduced values)**:
- WS2812 at full brightness (255) is too intense for camera capture
- Values 8-20 provide good visibility without oversaturation
- Allows better color differentiation in GIF/video evidence

---

## 10. Future Enhancements (out of initial scope)

- Fade transitions between LED phases (aesthetic improvement)
- Boot counter in NVS for automatic recovery trigger
- Secure boot signature verification (private/client scope)
- Advanced error diagnostics with detailed error codes
- Serial command interface in recovery mode
- Web-based recovery UI via WiFi AP mode

---

**Document version**: 1.0  
**Date**: 2026-02-16  
**Status**: Approved for implementation
