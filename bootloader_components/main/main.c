/**
 * Custom bootloader component for ESP32-C3 Waveshare Zero
 * 
 * Features:
 * - Custom bootloader banner and logging
 * - Partition table loading and validation
 * - Application image validation and boot
 * - Buffered logging to capture early boot messages before USB connection
 */

#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "bootloader_common.h"
#include "rom/ets_sys.h"
#include "soc/gpio_reg.h"
#include "ws2812.h"

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */
#define BOOTLOG_BUFFER_SIZE       2048

/* ============================================================================
 * Bootloader State Variables
 * ============================================================================ */
static char bootlog_buf[BOOTLOG_BUFFER_SIZE];
static size_t bootlog_index = 0;

/* ============================================================================
 * Timer Utilities (RISC-V mcycle counter)
 * ============================================================================ */

/**
 * Read 64-bit mcycle counter (RISC-V specific).
 * Uses atomic read pattern to handle 32-bit CPU reading 64-bit counter.
 */
static inline uint64_t read_mcycle64(void)
{
    uint32_t hi, lo, hi2;
    do {
        asm volatile ("rdcycleh %0" : "=r" (hi));
        asm volatile ("rdcycle %0" : "=r" (lo));
        asm volatile ("rdcycleh %0" : "=r" (hi2));
    } while (hi != hi2);
    return (((uint64_t)hi) << 32) | lo;
}

/**
 * Get boot time in microseconds from mcycle counter.
 * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ: CPU frequency in MHz
 */
static inline uint64_t boot_time_us(void)
{
    uint64_t cycles = read_mcycle64();
    return cycles / CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
}

/* ============================================================================
 * Bootloader Logging Functions
 * ============================================================================ */

/**
 * Append message to log buffer.
 * When USB is disconnected, messages are buffered instead of printed.
 * 
 * @param msg      Message string to append
 */
static void bootlog_append(const char* msg)
{
    if (!msg) return;
    
    size_t len = strlen(msg);
    if (bootlog_index + len + 1 >= BOOTLOG_BUFFER_SIZE) {
        return;  // Buffer full, drop message
    }
    
    strcpy(&bootlog_buf[bootlog_index], msg);
    bootlog_index += len;
    bootlog_buf[bootlog_index++] = '\n';
}

/**
 * Flush buffered log messages to console.
 * Prints all accumulated messages wrapped with delimiters.
 */
static void bootlog_flush(void)
{
    if (bootlog_index > 0) {
        bootlog_buf[bootlog_index] = '\0';
        ets_printf("=== BOOTLOADER LOG BUFFER START ===\n");
        ets_printf("%s", bootlog_buf);
        ets_printf("=== BOOTLOADER LOG BUFFER END ===\n");
    }
}

/**
 * Buffer-only logging.
 * Appends message to buffer.
 */
static void bootlog_buffer(const char* msg)
{
    // reuse existing append logic (adds newline)
    bootlog_append(msg);
}

/**
 * Print-only logging.
 * Prints message immediately to console.
 */
static void bootlog_print(const char* msg)
{
    ets_printf("%s\n", msg);
}


/* ============================================================================
 * Main Bootloader Entry Point
 * ============================================================================ */

/**
 * Custom bootloader start function for ESP32-C3.
 * 
 * Called by the bootloader framework after hardware initialization.
 * 
 * Execution flow:
 * 1. Buffer early logs (USB may not be ready)
 * 2. Initialize bootloader hardware
 * 3. Print custom banner
 * 4. Load and validate partition table
 * 5. Delay to allow USB connection
 * 6. Flush buffered logs to console
 * 7. Load and jump to application
 * 
 * This function never returns normally (loads the app and jumps to it).
 */
void __attribute__((noreturn)) call_start_cpu0(void)
{
    // ========================================================================
    // Phase 1: Buffer initial logs (USB may not be connected yet)
    // ========================================================================
    bootlog_buffer("call_start_cpu0 entered");
    bootlog_buffer("Initializing bootloader...");
    
    // ========================================================================
    // Phase 2: Hardware initialization
    // ========================================================================
    if (bootloader_init() != ESP_OK) {
        bootlog_buffer("ERROR: bootloader_init failed!");
        bootloader_reset();
    }
    bootlog_buffer("bootloader_init succeeded");

    // ========================================================================
    // Phase 3: Print bootloader banner
    // ========================================================================
    bootlog_buffer("===============================");
    bootlog_buffer("Custom ESP32-C3 Bootloader v1.0");
    bootlog_buffer("Waveshare ESP32-C3 Zero Board");
    bootlog_buffer("===============================");

    // ========================================================================
    // Phase 4: Load partition table
    // ========================================================================
    bootlog_buffer("Loading partition table...");
    
    bootloader_state_t bs = {0};
    if (!bootloader_utility_load_partition_table(&bs)) {
        bootlog_buffer("ERROR: Failed to load partition table!");
        bootloader_reset();
    }
    
    bootlog_buffer("Partition table loaded, loading application...");

    // ========================================================================
    // Phase 5: Wait for USB connection (countdown)
    // ========================================================================
    bootlog_buffer("Allow host to reconnect USB and monitor to attach...");
    bootlog_buffer("Application start in 1 second");
    
    // Allow host to reconnect USB and monitor to attach
    ets_delay_us(1000000);
    
    // ========================================================================
    // Phase 6: Flush buffer to console (now that USB should be connected)
    // ========================================================================
    bootlog_flush();

    // ========================================================================
    // Phase 6.1: LED color sequence (WS2812)
    // ========================================================================
    bootlog_print("Initializing WS2812 LED on GPIO10...");
    ws2812_init(WS2812_DEFAULT_GPIO);
    bootlog_print("WS2812 initialized");
    
    // Test GPIO control - set HIGH for visual test
    bootlog_print("Testing GPIO HIGH...");
    REG_WRITE(GPIO_OUT_W1TS_REG, (1U << WS2812_DEFAULT_GPIO));
    ets_delay_us(500000);  // 500ms
    REG_WRITE(GPIO_OUT_W1TC_REG, (1U << WS2812_DEFAULT_GPIO));
    ets_delay_us(500000);  // 500ms
    
    bootlog_print("Starting color sequence...");
    ws2812_set_rgb(0, 0, 0);     // clear
    ets_delay_us(100000);
    
    bootlog_print("LED: RED");
    ws2812_set_rgb(255, 0, 0);
    ets_delay_us(1000000);
    
    bootlog_print("LED: YELLOW");
    ws2812_set_rgb(255, 255, 0);
    ets_delay_us(1000000);
    
    bootlog_print("LED: GREEN");
    ws2812_set_rgb(0, 255, 0);
    ets_delay_us(1000000);
    
    bootlog_print("LED: OFF");
    ws2812_set_rgb(0, 0, 0);
    ets_delay_us(500000);
    
    // ========================================================================
    // Phase 7: Load and execute application
    // ========================================================================
    // Print a live marker now that host should be connected
    bootlog_print("Starting application NOW!");
    
    // bootloader_utility_load_boot_image():
    //   - Finds the factory partition (or OTA if configured)
    //   - Validates the app image
    //   - Loads it into RAM and jumps to entry point
    //
    // Parameters:
    //   - &bs: bootloader state with partition info
    //   - 0: start at index 0 (factory/OTA partition)
    bootloader_utility_load_boot_image(&bs, 0);

    // ========================================================================
    // Error path (should never reach here)
    // ========================================================================
    bootlog_print("ERROR: Failed to load application!");
    bootloader_reset();
}
