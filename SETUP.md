# Setup & Environment — Waveshare ESP32‑C3 Zero Bootloader

This document collects environment, installation, monitoring, and troubleshooting details required to build, flash, and capture full boot logs for the Waveshare ESP32‑C3 Zero bootloader.

## Required software
- ESP‑IDF: **v6.1** (tested)
- Python 3.11 (use ESP‑IDF Python virtual env)
- VS Code + ESP‑IDF Extension (recommended)
- pyserial (for `watch_serial.py`): `python -m pip install pyserial`
- RISC‑V toolchain (installed by IDF tools; ensure `riscv32-esp-elf-*` are on PATH for addr2line)

## Recommended paths (Windows examples)
- IDF: `C:\esp\esp-idf`
- IDF tools/env: `C:\Users\<User>\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe`
- IDF tools path: `IDF_TOOLS_PATH = C:\Users\<User>\.espressif`

## Set up environment (PowerShell)
1. Run ESP‑IDF install/setup per Espressif docs (or use VS Code extension installer).
2. Source the ESP‑IDF environment:
   - Short (temporary): `$env:IDF_PATH = 'C:\esp\esp-idf'`
   - Permanent (PowerShell profile): add the `export.ps1` invocation from ESP‑IDF or set `IDF_PATH`.
3. Example alias (add to your PowerShell profile):
   ```powershell
   function idf { & C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe C:\esp\esp-idf\tools\idf.py $args }
   $env:IDF_PATH = "C:\esp\esp-idf"
   ```
   Then: `idf build` / `idf -p COM4 flash` / `idf -p COM4 monitor`

## Common commands
- Build: `idf.py build`
- Flash (full): `idf.py -p COM4 flash`
- App fast‑flash: `idf.py -p COM4 app-flash`
- Bootloader only: `idf.py bootloader` / `idf.py -p COM4 bootloader-flash`
- Erase flash: `idf.py -p COM4 erase-flash`
- Backup flash: `python -m esptool --chip esp32c3 --port COM4 read_flash 0x0 0x400000 backup.bin`

## Monitor tips & capturing the full boot
- `idf.py -p COM4 monitor` is convenient and provides symbol/panic decoding.
- ESP32‑C3 USB CDC disconnects during reset → very early boot messages may be missed while USB re‑enumerates.
- Bootloader uses a 1.0s reconnect window to allow USB to re-enumerate reliably in demos (see bootloader log buffer).

### Use the included serial watcher (recommended to capture everything)
- Dependency: `pyserial`
- Example:
  ```powershell
  $env:IDF_PATH='C:\esp\esp-idf'
  C:\Users\<User>\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe scripts/watch_serial.py --port COM4 --inactivity 10
  ```
- What it does:
  - Reconnects after USB re‑enumeration
  - Adds monotonic line numbers and timestamps
  - Logs to `build/bootlog.txt`
- CLI flags:
  - `--port` (default `COM4`)
  - `--baud` (default `115200`)
  - `--inactivity` seconds (default `3.0`)
  - `--same-time` stall detection (default `3.0`)
  - `--log` (default `build/bootlog.txt`)

## Toolchain / addr2line note
- If monitor prints errors about `riscv32-esp-elf-addr2line` missing: the RISC‑V cross‑toolchain is not on PATH.
- Fix: run ESP‑IDF `export.ps1` (or use the IDF Extension shell) so the toolchain binaries are available. This is cosmetic — serial monitor still prints logs.

## Advanced notes (boot timing and debug strategies)
- Boot delay: bootloader flushes buffered logs after ~1.0s to allow USB re‑enumeration; this is intentional to ensure logs appear after USB reconnects in demo mode.
- To capture boot earlier:
  - Use external UART on GPIO20/21
  - Use JTAG (ESP‑PROG + OpenOCD)
  - Temporarily increase boot delay in bootloader for debugging

## Workspace / editor hints
- VS Code (workspace) may include `idf.pythonBinPath`; ensure it points to the IDF Python env used by your IDF install.
- `sdkconfig` / `sdkconfig.defaults` indicate IDF init/version (`CONFIG_IDF_INIT_VERSION:"6.1.0"`).

## Troubleshooting checklist
1. `idf.py --version` shows ESP‑IDF v6.1 (or your configured version).
2. `python -c "import serial"` succeeds (pyserial installed).
3. `riscv32-esp-elf-addr2line --version` available in PATH (for addr2line decoding).
4. Serial watcher can open the port and writes `build/bootlog.txt`.

## Validation defaults (locked)

For the hardware-in-the-loop validator profile used in this project:

- Serial port auto-detection is the default behavior.
- Manual port selection remains supported as an override.
- Default full validation timeout is 10 seconds.
- Missing LED media evidence is warning-only for protocol validation.

Note:
- Protocol validation is token/log driven.
- LED/video/screenshot assets remain required for complete marketing evidence packaging.
