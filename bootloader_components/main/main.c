// Custom bootloader component for ESP32-C3 Waveshare Zero
 
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "bootloader_common.h"
#include "rom/ets_sys.h"
#include "soc/gpio_reg.h"
#include "ws2812.h"
#include "logging.h"

// Main Bootloader Entry Point
 void __attribute__((noreturn)) call_start_cpu0(void)
{
    // Buffer initial logs (USB may not be connected yet)
    bootlog_buffer("call_start_cpu0 entered");
    bootlog_buffer("Initializing bootloader...");

    // Hardware initialization
    if (bootloader_init() != ESP_OK) {
        bootlog_buffer("ERROR: bootloader_init failed!");
        bootloader_reset();
    }
    bootlog_buffer("bootloader_init succeeded");

    // Print bootloader banner
    bootlog_buffer("===============================");
    bootlog_buffer("Custom ESP32-C3 Bootloader v1.0");
    bootlog_buffer("Waveshare ESP32-C3 Zero Board");
    bootlog_buffer("===============================");

    // Initialize WS2812 LED (GPIO10)
    bootlog_buffer("Initializing WS2812 LED on GPIO10...");
    ws2812_init(WS2812_DEFAULT_GPIO);
    bootlog_buffer("WS2812 initialized");
    ws2812_set_rgb(0, 0, 0);
    bootlog_buffer("LED: RED");
    ws2812_set_rgb(12, 0, 0);

    // Load partition table
    bootlog_buffer("Loading partition table...");
    
    bootloader_state_t bs = {0};
    if (!bootloader_utility_load_partition_table(&bs)) {
        bootlog_buffer("ERROR: Failed to load partition table!");
        bootloader_reset();
    }
    
    bootlog_buffer("Partition table loaded, loading application...");

    // Wait for USB connection (countdown)
    bootlog_buffer("Allow host to reconnect USB and monitor to attach...");
    bootlog_buffer("Application start in 1 second");
    
    // Allow host to reconnect USB and monitor to attach
    ets_delay_us(1000000);
    bootlog_flush();

    // LED color sequence (WS2812)
    bootlog_print("LED: YELLOW");
    ws2812_set_rgb(12, 4, 0);
    ets_delay_us(1000000);
    
    bootlog_print("LED: GREEN");
    ws2812_set_rgb(0, 12, 0);
    ets_delay_us(1000000);
    
    bootlog_print("LED: OFF");
    ws2812_set_rgb(0, 0, 0);
    
    // Load and execute application
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

    // Error path (should never reach here)
    bootlog_print("ERROR: Failed to load application!");
    bootloader_reset();
}
