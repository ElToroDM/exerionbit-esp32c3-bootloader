# Setup & Environment — Waveshare ESP32‑C3 Zero Bootloader

This document collects environment, installation, monitoring, and troubleshooting details required to build, flash, and capture full boot logs for the Waveshare ESP32‑C3 Zero bootloader.

## Contract documents

- Boot sequence contract: `BOOT_SEQUENCE.md`
- Validation execution profile: `VALIDATION_PROFILE.md`

## Quick verification (after environment setup)

Run these in PowerShell to confirm all dependencies are ready:
```powershell
# Test 1: ESP-IDF (both 'idf' and 'idf.py' work after profile setup)
idf --version             # Should print "ESP-IDF v6.0..."

# Test 2: cmake and ninja (required for idf build)
cmake --version           # Should print "cmake version 3.30.2"
ninja --version           # Should print "1.12.1"

# Test 3: RISC-V addr2line
riscv32-esp-elf-addr2line --version    # Should print "GNU addr2line..."

# Test 4: Python and pyserial
python -c "import serial; print(f'pyserial {serial.__version__}')"

# Test 5: IDF_PATH
$env:IDF_PATH             # Should print "C:\esp\v6.0\esp-idf"
```

If all tests pass, you're ready to build. Start with `idf build` or `idf.py build`.

**If any command fails:**
- Verify your PowerShell profile was saved and reloaded
- Run `. $PROFILE` (dot-source) to reload environment — `&` does not import functions into the current session
- Restart PowerShell and try again

## Validation entry point

- **Live monitoring**: `idf monitor` (or `idf.py monitor`)
- **Long-duration logging** (demo mode): `python scripts/watch_serial.py --inactivity 10` (logs to `build/bootlog.txt`)
- **Offline validation** (if available): `python scripts/validate_bootlog.py --log build/bootlog.txt`
- Optional port override for any command: `-p <PORT>` or `--port <PORT>`

## Required software
- ESP‑IDF: **v6.0** (stable target)
- Python 3.11 (use ESP‑IDF Python virtual env)
- VS Code + ESP‑IDF Extension (recommended)
- pyserial (for `watch_serial.py`): `python -m pip install pyserial`
- RISC‑V toolchain (installed by IDF tools; ensure `riscv32-esp-elf-*` are on PATH for addr2line)

## Recommended paths (Windows examples)
- IDF: `C:\esp\v6.0\esp-idf`
- IDF tools/env: `C:\Espressif\tools\python_env\idf6.0_py3.11_env\Scripts\python.exe`
- IDF tools path: `IDF_TOOLS_PATH = C:\Espressif\tools`

## Set up environment (PowerShell)

### For this workstation (recommended quick setup)

1. **Install pyserial** (one-time):
  ```powershell
  $pythonEnv = 'C:\Espressif\tools\python_env\idf6.0_py3.11_env\Scripts\python.exe'
  & $pythonEnv -m pip install pyserial
  ```

2. **Temporary session** (current PowerShell window only):
  ```powershell
  $env:IDF_PATH = 'C:\esp\v6.0\esp-idf'
  $env:IDF_TOOLS_PATH = 'C:\Espressif\tools'
  . "$env:IDF_PATH\export.ps1"

  # Create convenient alias
  function idf { & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" @args }
  ```
  Then: `idf build` / `idf flash` / `idf monitor`

3. **Permanent setup** (add to PowerShell profile):
   
   Run `notepad $PROFILE` and add:
   ```powershell
   # ESP-IDF environment configuration

  # IDF base path
  $env:IDF_PATH = 'C:\esp\v6.0\esp-idf'
  $env:IDF_TOOLS_PATH = 'C:\Espressif\tools'

  # Load the ESP-IDF 6.0 environment into the current shell
  . "$env:IDF_PATH\export.ps1"

  # Convenience wrappers — both 'idf' and 'idf.py' are supported
  function idf    { & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" @args }
  function idf.py { & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" @args }
   ```
   Then reload: `. $PROFILE` (dot-source, or restart PowerShell)

### Dot-sourcing vs. running the profile

When you edit your PowerShell profile, prefer *dot-sourcing* to load functions and exported variables into the current session. Running the profile with `& $PROFILE` executes it in a child scope which may not import functions into your interactive session.

Use this to apply the profile changes immediately in the current shell:

```powershell
. $PROFILE    # dot-source: imports functions and env vars into this session
```

> **Why dot-source?** Running `& $PROFILE` executes the script in a child scope — environment variables propagate, but function definitions do not. Dot-sourcing (`. $PROFILE`) runs the script in the current scope so `idf` and `idf.py` are available immediately.

Quick checks after dot-sourcing:

```powershell
Get-Command idf -CommandType Function   # should list the 'idf' function
idf --version                           # should print ESP-IDF version (no WARNING)
cmake --version                         # should print cmake version 3.30.2
ninja --version                         # should print 1.12.1
python -c "import serial; print(serial.__version__)"  # should print 3.5
riscv32-esp-elf-addr2line --version     # should print GNU addr2line
```

If `Get-Command idf` returns nothing, run `. $PROFILE` (dot-source) and try again.

### For other workstations (generic setup)

1. Run ESP‑IDF install/setup per Espressif docs (or use VS Code extension installer).
2. Source the ESP‑IDF environment:
  - Short (temporary): `$env:IDF_PATH = 'C:\esp\v6.0\esp-idf'`
   - Permanent (PowerShell profile): add the `export.ps1` invocation from ESP‑IDF or set `IDF_PATH`.

## Common commands
- Build: `idf.py build`
- Flash (full): `idf.py flash`
- App fast‑flash: `idf.py app-flash`
- Bootloader only: `idf.py bootloader` / `idf.py bootloader-flash`
- Erase flash: `idf.py erase-flash`
- Backup flash (explicit port required by esptool): `python -m esptool --chip esp32c3 --port <PORT> read_flash 0x0 0x400000 backup.bin`
- Optional port override for IDF commands: append `-p <PORT>`

## Monitor tips & capturing the full boot

**For complete first-boot capture** (ROM bootloader, all `BL_EVT:*`, app startup):
- Use `idf.py monitor` or `idf monitor` — shows everything from ROM, provides symbol/panic decoding.
- If needed, force a port: `idf.py -p <PORT> monitor`

**Known behavior** (USB re-enumeration):
- ESP32‑C3 USB CDC disconnects during reset.
- Early ROM bootloader output may briefly be invisible to the monitor while USB re‑enumerates.
- Bootloader intentionally flushes buffered logs after ~1.0s to allow USB to re-enumerate and stabilize before late-stage logs.
- In practice, `idf monitor` captures the complete sequence once USB reconnects (usually within 1-2 seconds).


### Use the included serial watcher (for long-running logging)

`watch_serial.py` is useful for long-duration monitoring, timestamped logging to file, and handling USB reconnections. However, it does **not** capture very early bootloader output (ROM bootloader logs, initial `BL_EVT:*` events); `idf monitor` shows these. Use `idf monitor` for complete first-boot capture; use `watch_serial.py` for demo-mode logging, validation loops, or when you need timestamped file output.

- Dependency: `pyserial`
- Example:
  ```powershell
  $env:IDF_PATH='C:\esp\v6.0\esp-idf'
  $env:IDF_TOOLS_PATH='C:\Espressif\tools'
  . "$env:IDF_PATH\export.ps1"
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" scripts/watch_serial.py --inactivity 10
  ```
- What it does:
  - Reconnects after USB re‑enumeration (useful when the device is reset)
  - Adds monotonic line numbers and timestamps
  - Logs to `build/bootlog.txt`
- CLI flags:
  - `--port` (optional; use when auto-detection is not enough)
  - `--baud` (default `115200`)
  - `--inactivity` seconds (default `3.0`)
  - `--same-time` stall detection (default `3.0`)
  - `--log` (default `build/bootlog.txt`)

#### Smoke test — verify HEARTBEAT
- Run the watcher, press the device reset button, wait for the watcher to show `Connected to <port>` and then check the log for `APP_EVT:HEARTBEAT`.
- PowerShell quick check:
  ```powershell
  Select-String -Path .\build\bootlog.txt -Pattern 'APP_EVT:HEARTBEAT'
  ```
- Note: a transient message such as `Serial read error: ClearCommError` during a manual reset is expected — the watcher will reconnect automatically and continue logging.

## Toolchain / addr2line note
- If monitor prints errors about `riscv32-esp-elf-addr2line` missing: the RISC‑V cross‑toolchain is not on PATH.
- Fix: run ESP‑IDF `export.ps1` (or use the IDF Extension shell) so the toolchain binaries are available. This is cosmetic — serial monitor still prints logs.

## Advanced notes (boot timing and debug strategies)
- Boot delay: bootloader flushes buffered logs after ~1.0s to allow USB re‑enumeration; this is intentional to ensure logs appear after USB reconnects in demo mode.
- To capture boot earlier:
  - Use external UART on GPIO20/21
  - Use JTAG (ESP‑PROG + OpenOCD)
  - Temporarily increase boot delay in bootloader for debugging

## Selector flow (GPIO9)
- The selector can be armed by holding the BOOT button (GPIO9) from reset/power-on.
- A short stable hold (`ARM_DETECT_MS = 60 ms`) arms the selector and emits `BL_EVT:MODE_SELECT_ARMED` and lights the first mode color.
- Release after arming: the release itself does not execute a mode; it simply readies the selector for button interaction.
- After release:
  - Short presses (60 ms–250 ms) cycle `UPDATE → RECOVERY → NORMAL` (circular), each change emits `BL_EVT:MODE_SELECTED:<mode>` and updates the LED color.
  - A long press (≥700 ms) executes the currently selected mode and emits `BL_EVT:MODE_EXECUTE:<mode>`.
- There is no automatic selection timeout in the baseline; once armed, the selector remains active until a long press executes a mode or the device is reset.

## Workspace / editor hints
- VS Code (workspace) may include `idf.pythonBinPath`; ensure it points to the IDF Python env used by your IDF install.
- `sdkconfig` indicates IDF init/version (`CONFIG_IDF_INIT_VERSION:"6.0.0"`).

## Testing CRC functionality

The bootloader validates app image CRC at boot.
The ESP-IDF extension **Build** button regenerates a CRC-OK app image in place, so extension **Flash** uses a valid descriptor by default.

**Normal flash (CRC OK):**
1. Build, Flash and Monitor: use ESP-IDF extension "Build, Flash and Monitor" button
2. Expect: `BL_EVT:APP_CRC_OK` → `BL_EVT:HANDOFF_APP` → `APP_EVT:START`

**Testing CRC failure (image corruption):**
1. Build: use ESP-IDF extension "Build" button
2. Corrupt CRC in the built image:
   ```powershell
   python scripts/append_crc.py --bad-crc build/waveshare_esp32c3_zero_bootloader.bin --out-image build/waveshare_esp32c3_zero_bootloader.bin
   ```
3. Flash: use ESP-IDF extension "Flash" button
4. Monitor: use ESP-IDF extension "Monitor" button
4. Expect: `BL_EVT:APP_CRC_FAIL` → recovery mode (heartbeat every 5s)

To return to CRC OK: rebuild with extension Build button (automatically regenerates CRC OK image).

Manual equivalent (PowerShell):
```powershell
# CRC OK
python scripts/append_crc.py build/waveshare_esp32c3_zero_bootloader.bin --out-image build/waveshare_esp32c3_zero_bootloader.bin
python -m esptool --chip esp32c3 --port <PORT> --baud 460800 write-flash 0x10000 build/waveshare_esp32c3_zero_bootloader.bin

# CRC FAIL
python scripts/append_crc.py --bad-crc build/waveshare_esp32c3_zero_bootloader.bin --out-image build/waveshare_esp32c3_zero_bootloader.bin
python -m esptool --chip esp32c3 --port <PORT> --baud 460800 write-flash 0x10000 build/waveshare_esp32c3_zero_bootloader.bin
```

Use `idf monitor` for complete boot capture, or `scripts/watch_serial.py --inactivity 10` for long-duration timestamped logging.

## Troubleshooting checklist
1. `idf.py --version` shows ESP‑IDF v6.0 (or your configured version).
2. `python -c "import serial"` succeeds (pyserial installed).
3. `riscv32-esp-elf-addr2line --version` available in PATH (for addr2line decoding).
4. Serial watcher can open the port and writes `build/bootlog.txt`.

## Validation defaults (locked)

For the hardware-in-the-loop validator profile used in this project:

- Serial port auto-detection is the default behavior.
- Manual port selection remains supported as an override.
- Default full validation timeout is 10 seconds.
 
