# Waveshare ESP32-C3 Zero Custom Bootloader

Minimal ESP-IDF bootloader for the Waveshare ESP32-C3 Zero board.

## Hardware
- **Board**: Waveshare ESP32-C3 Zero
- **SoC**: ESP32-C3FH4 (RISC-V, 160MHz)
- **Flash**: 4MB onboard
- **USB**: USB-CDC (no external USB-UART chip)
- **BOOT**: GPIO9 (manual sequence required)

## Manual Boot Sequence
To flash the board:
1. Press and hold **BOOT** button (GPIO9)
2. Press and release **RESET** button
3. Release **BOOT** button
4. Run flash command

## Build & Flash Workflow (recommended)

> Use ESP-IDF v6.1 (configure the ESP-IDF Extension to point to `C:\esp\esp-idf` and set `IDF_TOOLS_PATH` to `C:\Users\Admin\.espressif`).

### ⭐ Recommended: Use VS Code ESP-IDF Extension
The easiest way to build, flash, and monitor is using the ESP-IDF Extension buttons in VS Code:
1. Click the **ESP-IDF** icon in the sidebar
2. Use the **Build** button (or `Ctrl+E` `B`)
3. Use the **Flash** button (or `Ctrl+E` `F`)
4. Use the **Monitor** button (or `Ctrl+E` `M`)

This automatically handles all environment setup and toolchain configuration.

### Alternative: Manual PowerShell commands
If you prefer to use PowerShell directly, create an alias for convenience:

```powershell
# Add to your PowerShell profile for permanent use
function idf { & C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe C:\esp\esp-idf\tools\idf.py $args }
$env:IDF_PATH = "C:\esp\esp-idf"

# Then use it like:
idf build
idf -p COM4 flash
idf -p COM4 monitor
```

Or use the full command each time:
```powershell
# Set IDF_PATH first
$env:IDF_PATH = "C:\esp\esp-idf"

# Build
C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe C:\esp\esp-idf\tools\idf.py build

# Flash
C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe C:\esp\esp-idf\tools\idf.py -p COM4 flash

# Monitor
C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe C:\esp\esp-idf\tools\idf.py -p COM4 monitor
```

### Common commands
```powershell
# Fast iteration (app only - faster than full flash)
idf -p COM4 app-flash

# Bootloader only
idf -p COM4 bootloader-flash

# Backup full flash to a file
python -m esptool --chip esp32c3 --port COM4 read_flash 0x0 0x400000 backup.bin

# Erase flash completely
idf -p COM4 erase-flash
```

### Notes about monitoring & capturing the full boot
- **Recommended:** Use the ESP-IDF Extension's "Monitor" button or `idf.py -p COM4 monitor` for normal development. This provides:
  - Automatic symbol decoding
  - Panic backtrace decoding
  - Colored output
  - Interactive menu (`Ctrl+T` for help)
  
- **ESP32-C3 USB behavior:** The USB CDC interface disconnects during reset, which can cause the monitor to miss very early ROM/bootloader messages while USB re-enumerates. You'll see:
  ```
  --- Error: ClearCommError failed (PermissionError...)
  --- Waiting for the device to reconnect
  ```
  This is **normal ESP32-C3 behavior**, not an error. The monitor automatically reconnects.

- **Bootloader timing:** The custom bootloader now waits **0.5 seconds** (`ets_delay_us(500000)`) before flushing buffered logs. This gives the USB interface time to reconnect after reset. The buffered logs then print all at once:
  ```
  === BOOTLOADER LOG BUFFER START ===
  [#0001] bootloader_init succeeded
  [#0002] ===============================
  [#0003] Custom ESP32-C3 Bootloader v1.0
  ...
  === BOOTLOADER LOG BUFFER END ===
  ```

- **To capture absolutely everything:** Use the included serial watcher which handles USB reconnection and logs to a file:
  ```powershell
  python scripts/watch_serial.py --port COM4 --inactivity 10
  ```

- **addr2line warnings:** If you see "The system cannot find the file specified" errors related to `riscv32-esp-elf-addr2line`, the RISC-V toolchain is not in PATH. This is cosmetic—the monitor still works. To fix, run the ESP-IDF `export.ps1` script first.

---

## Expected Boot Messages
Look for these in the serial monitor:
```
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x15 (USB_UART_CHIP_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT)
Saved PC:0x4038315a
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd5830,len:0x1594
load:0x403cbf10,len:0xc9c
load:0x403ce710,len:0x3010
entry 0x403cbf1a
I (24) boot: ESP-IDF v6.1-dev-2309-g7d7f533357 2nd stage bootloader
I (24) boot: compile time Feb  2 2026 23:03:36
I (25) boot: chip revision: v0.4
I (25) boot: Enabling RNG early entropy source...
I (25) boot: Partition Table:
I (26) boot: ## Label            Usage          Type ST Offset   Length
I (26) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (26) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (26) boot:  2 factory          factory app      00 00 00010000 00100000
I (27) boot: End of partition table
I (27) esp_image: segment 0: paddr=00010020 vaddr=3c010020 size=0736ch ( 29548) map
I (32) esp_image: segment 1: paddr=00017394 vaddr=3fc89800 size=01840h (  6208) load
I (39) esp_image: segment 3: paddr=00020020 vaddr=42000020 size=0e5ach ( 58796) map
I (49) esp_image: segment 4: paddr=0002e5d4 vaddr=4038743c size=02368h (  9064) load
I (54) boot: Loaded app from partition at offset 0x10000
I (54) boot: Disabling RNG early entropy source...
```

## ✅ **CUSTOM BOOTLOADER STATUS: WORKING**

**DISCOVERY**: The custom bootloader IS working correctly! The issue is ESP32-C3's native USB behavior.

### What Actually Happens During RESET:
1. ✅ **ESP32-C3 connected** (USB functional, monitor working)
2. 🔄 **RESET pressed** → **USB disconnects immediately** 
3. 🔄 **ROM bootloader runs** → (USB disconnected - output invisible)
4. ✅ **Our custom bootloader executes** → (USB disconnected - output invisible)
5. ✅ **Application starts** → (USB disconnected - output invisible)
6. 🔄 **Application reinitializes USB CDC** → USB reconnects
7. 🔌 **Monitor reconnects** → but boot sequence already completed

### Serial Monitor Shows:
```
--- Error: ClearCommError failed (PermissionError(13, 'The device does not recognize the command.', None, 22))
--- Waiting for the device to reconnect
```
**This is NORMAL ESP32-C3 behavior, not an error!**

### Evidence Custom Bootloader Works:
- **Bootloader binary size**: `0x5160 bytes` (smaller than default `0x52a0`)
- **Build logs show**: Our `bootloader_components/main` component used instead of ESP-IDF default
- **No boot failures**: System starts successfully every time
- **Application runs**: Device reconnects reliably after reset

### To See Full Boot Sequence (Optional):
1. **External UART**: Connect USB-Serial adapter to GPIO20/21  
2. **JTAG Debug**: Use ESP-PROG probe with OpenOCD
3. **Add Boot Delay**: Delay in bootloader before app USB init

---

## Serial monitoring & capturing full boot logs ✅

ESP32-C3's native USB disconnects during reset, which can hide early ROM/bootloader messages. To reliably capture the entire boot sequence (including early messages printed while USB is disconnected), use the included monitor script instead of `idf.py monitor`:

- Start the script (PowerShell example):

```powershell
# Use IDF Python environment for correct deps
$env:IDF_PATH = "C:\esp\esp-idf"
C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe scripts/watch_serial.py --port COM4 --inactivity 10
```

- What the script does:
  - Reconnects automatically after USB re-enumeration ✅
  - Logs to `build/bootlog.txt` ✅
  
- Fast iteration (bootloader only):
```powershell
# Build only the bootloader (no full app build)
C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe C:\esp\esp-idf\tools\idf.py bootloader

# Flash only the bootloader
C:\Users\Admin\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe C:\esp\esp-idf\tools\idf.py -p COM4 bootloader-flash
```

Notes:
- `idf.py monitor` is still useful for its GDB/panic-decoding features, but those initial `idf.py` header lines are printed by the host and will not be numbered by the serial monitor script. Use the script for complete, robust serial capture and numbering.
- If you prefer the `idf.py monitor` UX and still want every line prefixed, we can add a wrapper that prefixes `idf.py monitor` stdout as well (more invasive; I can implement if required).

## Project Structure
```
waveshare_esp32c3_zero_bootloader/
├── CMakeLists.txt              # Top-level project (includes bootloader_components)
├── sdkconfig.defaults          # Minimal bootloader config  
├── partitions.csv              # Partition table (NVS + factory app)
├── main/
│   ├── CMakeLists.txt
│   └── main.c                  # Test application
├── bootloader_components/
│   └── main/                   # ← Custom bootloader (replaces ESP-IDF default)
│       ├── CMakeLists.txt
│       └── main.c              # Custom call_start_cpu0() implementation
└── scripts/
    └── watch_serial.py         # Serial monitoring with USB reconnection handling
```

## Key Implementation Details
- **Custom bootloader**: `bootloader_components/main/main.c` contains our `call_start_cpu0()`
- **Component override**: Our `main` component replaces ESP-IDF's default bootloader main
- **USB handling**: Scripts handle ESP32-C3 USB reenumeration during reset
- **Build verification**: Bootloader size `0x5160 bytes` (vs default `0x52a0`)

## Configuration Notes
- Secure Boot: **DISABLED** (development mode)
- Flash Encryption: **DISABLED** (development mode)
- Bootloader Log Level: **ERROR** (reduce noise)
- Partition Table Offset: **0x8000** (default)
- Compiler Optimization: **Size (-Os)**

## Future Extensions
- OTA update support (add ota_0, ota_1, otadata partitions)
- Image signature verification
- JTAG debugging (add ESP-PROG probe)
- USB DFU mode
