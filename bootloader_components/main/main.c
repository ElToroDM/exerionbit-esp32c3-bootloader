// Custom bootloader component for ESP32-C3 Waveshare Zero

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sdkconfig.h"
#include "bootloader_flash_priv.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "esp_rom_crc.h"
#include "esp_rom_gpio.h"
#include "esp_rom_serial_output.h"
#include "rom/ets_sys.h"
#include "soc/gpio_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "hal/wdt_hal.h"
#include "update_mode.h"
#include "ws2812.h"

#define TOTAL_VISUAL_MS            4000U
#define USB_RECONNECT_MS           1000U

#define BOOT_GPIO                  9

// Mode selector timing (see BOOT_SEQUENCE.md §4 and §8)
#define ARM_DETECT_MS              60U
#define SHORT_PRESS_MIN_MS         60U
#define SHORT_PRESS_MAX_MS         250U
#define LONG_PRESS_MIN_MS          700U

#define UPDATE_PULSE_HZ            2U
#define UPDATE_VERIFY_OK_MS        200U
#define UPDATE_VERIFY_FAIL_MS      300U

#define FATAL_BLINK_PERIOD_MS      333U
#define FATAL_BLINK_ON_MS          (FATAL_BLINK_PERIOD_MS / 2U)
#define FATAL_BLINK_OFF_MS         (FATAL_BLINK_PERIOD_MS - FATAL_BLINK_ON_MS)

#define RECOVERY_POLL_MS             10U
#define RECOVERY_CMD_MAX_LEN         32U
#define RECOVERY_IDLE_BLINK_MS     1200U

#define RECOVERY_LED_BRIGHT_R        20U
#define RECOVERY_LED_BRIGHT_G         0U
#define RECOVERY_LED_BRIGHT_B        20U
#define RECOVERY_LED_DIM_R            6U
#define RECOVERY_LED_DIM_G            0U
#define RECOVERY_LED_DIM_B            6U

#define BOOT_CTX_MAGIC             0x424C4358U
#define BOOT_CTX_UNKNOWN           0U
#define BOOT_CTX_NORMAL            1U
#define BOOT_CTX_RECOVERY          2U
#define BOOT_CTX_UPDATE            3U
#define BOOT_CTX_MAGIC_REG         RTC_CNTL_STORE6_REG
#define BOOT_CTX_VALUE_REG         RTC_CNTL_STORE7_REG

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_rgb_t;

typedef struct {
    const char *token;
    led_rgb_t color;
    uint8_t visual_percent;
} phase_spec_t;

static const phase_spec_t s_normal_phases[] = {
    {"INIT", {8, 0, 8}, 10},
    {"HW_READY", {0, 8, 16}, 10},
    {"PARTITION_TABLE_OK", {0, 12, 12}, 15},
    {"DECISION_NORMAL", {16, 10, 0}, 15},
    {"LOAD_APP", {0, 16, 4}, 20},
    {"HANDOFF", {0, 20, 0}, 30},
};

// Boot mode ring: UPDATE -> RECOVERY -> NORMAL -> UPDATE (circular)
typedef enum {
    BOOT_MODE_NORMAL   = 0,
    BOOT_MODE_UPDATE   = 1,
    BOOT_MODE_RECOVERY = 2,
} boot_mode_t;

static const boot_mode_t s_mode_ring[] = {
    BOOT_MODE_UPDATE,
    BOOT_MODE_RECOVERY,
    BOOT_MODE_NORMAL,
};

static const char *const s_mode_ring_names[] = {
    "UPDATE",
    "RECOVERY",
    "NORMAL",
};

static const led_rgb_t s_mode_ring_colors[] = {
    {16, 10, 0},
    {20, 0, 20},
    {0, 20, 0},
};

#define MODE_RING_SIZE (sizeof(s_mode_ring) / sizeof(s_mode_ring[0]))

typedef enum {
    RECOVERY_ACTION_STAY = 0,
    RECOVERY_ACTION_BOOT = 1,
} recovery_action_t;

static void feed_watchdog(void);
static bool run_app_crc_check(const bootloader_state_t *boot_state);
static bool erase_factory_partition(const bootloader_state_t *boot_state);
static recovery_action_t handle_recovery_command(
    const char *line,
    const bootloader_state_t *boot_state,
    const update_mode_hooks_t *update_hooks);

static void delay_ms(uint32_t delay_ms_value)
{
    while (delay_ms_value >= 10U) {
        ets_delay_us(10000U);
        feed_watchdog();
        delay_ms_value -= 10U;
    }

    if (delay_ms_value > 0U) {
        ets_delay_us(delay_ms_value * 1000U);
        feed_watchdog();
    }
}

static void feed_watchdog(void)
{
#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
    wdt_hal_context_t rwdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rwdt_ctx);
    wdt_hal_feed(&rwdt_ctx);
    wdt_hal_write_protect_enable(&rwdt_ctx);
#endif
}

static uint32_t phase_duration_ms(uint8_t percentage)
{
    return (TOTAL_VISUAL_MS * percentage) / 100U;
}

static void emit_evt(const char *token)
{
    ets_printf("BL_EVT:%s\n", token);
}

static void emit_evt_with_value(const char *token, uint32_t value)
{
    ets_printf("BL_EVT:%s:%u\n", token, value);
}

static void publish_boot_context(uint32_t context)
{
    REG_WRITE(BOOT_CTX_MAGIC_REG, BOOT_CTX_MAGIC);
    REG_WRITE(BOOT_CTX_VALUE_REG, context);
}

static void set_led(led_rgb_t color)
{
    ws2812_set_rgb(color.r, color.g, color.b);
}

static void set_led_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_set_rgb(r, g, b);
}

static void configure_input_gpio(int gpio)
{
    esp_rom_gpio_pad_select_gpio(gpio);
    REG_WRITE(GPIO_ENABLE_W1TC_REG, (1U << gpio));
    esp_rom_gpio_pad_pullup_only(gpio);
}

static bool gpio_level_low(int gpio)
{
    return ((REG_READ(GPIO_IN_REG) & (1U << gpio)) == 0U);
}

// Descriptor stored in the last 8 bytes of the app partition:
//   [image_size: uint32_t LE][crc32: uint32_t LE]
// CRC32 matches esp_rom_crc32_le(0, ...) == zlib.crc32(data) & 0xFFFFFFFF.
typedef struct {
    uint32_t image_size;
    uint32_t crc32;
} __attribute__((packed)) app_crc_descriptor_t;

#define APP_CRC_DESCRIPTOR_SIZE  sizeof(app_crc_descriptor_t)

// Read 'length' bytes from flash at 'offset' and compute CRC-32.
// esp_rom_crc32_le(0, buf, len) == zlib.crc32(data) & 0xFFFFFFFF.
// Init with 0 and no final XOR to match the host tool (scripts/append_crc.py).
// Returns 0 on any flash read error (forces mismatch).
#define CRC_READ_CHUNK_SIZE    256U

static uint32_t compute_flash_crc32(uint32_t offset, uint32_t length)
{
    static uint8_t s_crc_chunk[CRC_READ_CHUNK_SIZE];
    uint32_t crc = 0U;
    uint32_t remaining = length;
    uint32_t addr = offset;

    while (remaining > 0U) {
        uint32_t to_read = (remaining < CRC_READ_CHUNK_SIZE) ? remaining : CRC_READ_CHUNK_SIZE;
        esp_err_t err = bootloader_flash_read(addr, s_crc_chunk, to_read, false);
        if (err != ESP_OK) {
            return 0U;
        }
        crc = esp_rom_crc32_le(crc, s_crc_chunk, to_read);
        addr += to_read;
        remaining -= to_read;
    }

    return crc;
}

static void emit_recovery_response(const char *line)
{
    ets_printf("BL_RSP:%s\n", line);
}

static void normalize_ascii_lowercase(char *text)
{
    if (text == NULL) {
        return;
    }

    for (char *p = text; *p != '\0'; ++p) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = (char)(*p - 'A' + 'a');
        }
    }
}

static bool erase_factory_partition(const bootloader_state_t *boot_state)
{
    const uint32_t partition_offset = boot_state->factory.offset;
    const uint32_t partition_size = boot_state->factory.size;

    if (partition_size == 0U || (partition_size % FLASH_SECTOR_SIZE) != 0U) {
        return false;
    }

    const uint32_t first_sector = partition_offset / FLASH_SECTOR_SIZE;
    const uint32_t sector_count = partition_size / FLASH_SECTOR_SIZE;
    for (uint32_t sector = 0U; sector < sector_count; ++sector) {
        if (bootloader_flash_erase_sector(first_sector + sector) != ESP_OK) {
            return false;
        }
    }

    return true;
}

static recovery_action_t handle_recovery_command(
    const char *line,
    const bootloader_state_t *boot_state,
    const update_mode_hooks_t *update_hooks)
{
    char normalized[RECOVERY_CMD_MAX_LEN] = {0};

    if (line == NULL || boot_state == NULL || update_hooks == NULL) {
        emit_recovery_response("error:invalid_request");
        return RECOVERY_ACTION_STAY;
    }

    strncpy(normalized, line, sizeof(normalized) - 1U);
    normalize_ascii_lowercase(normalized);

    if (strcmp(normalized, "status") == 0) {
        emit_evt("RECOVERY_CMD_STATUS");
        emit_recovery_response("status:recovery_active:ready");
        return RECOVERY_ACTION_STAY;
    }

    if (strcmp(normalized, "?") == 0 || strcmp(normalized, "h") == 0 || strcmp(normalized, "help") == 0) {
        emit_evt("RECOVERY_CMD_HELP");
        emit_recovery_response("help:status,reboot,update,erase,boot,help");
        return RECOVERY_ACTION_STAY;
    }

    if (strcmp(normalized, "reboot") == 0) {
        emit_evt("RECOVERY_CMD_REBOOT");
        emit_recovery_response("reboot:ok");
        bootloader_reset();
        for (;;) {
        }
    }

    if (strcmp(normalized, "update") == 0) {
        emit_evt("RECOVERY_CMD_UPDATE");
        emit_recovery_response("update:enter");
        if (handle_update_mode(boot_state, update_hooks)) {
            emit_evt("RECOVERY_CMD_UPDATE_OK");
            emit_recovery_response("update:ok");
            return RECOVERY_ACTION_BOOT;
        }

        emit_evt("RECOVERY_CMD_UPDATE_FAIL");
        emit_recovery_response("update:fail");
        return RECOVERY_ACTION_STAY;
    }

    if (strcmp(normalized, "erase") == 0) {
        emit_evt("RECOVERY_CMD_ERASE");
        if (erase_factory_partition(boot_state)) {
            emit_evt("RECOVERY_CMD_ERASE_OK");
            emit_recovery_response("erase:ok");
        } else {
            emit_evt("RECOVERY_CMD_ERASE_FAIL");
            emit_recovery_response("erase:fail");
        }
        return RECOVERY_ACTION_STAY;
    }

    if (strcmp(normalized, "boot") == 0) {
        emit_evt("RECOVERY_CMD_BOOT");
        if (run_app_crc_check(boot_state)) {
            emit_evt("RECOVERY_CMD_BOOT_OK");
            emit_recovery_response("boot:crc_ok");
            return RECOVERY_ACTION_BOOT;
        }

        emit_evt("RECOVERY_CMD_BOOT_FAIL");
        emit_recovery_response("boot:crc_fail");
        return RECOVERY_ACTION_STAY;
    }

    emit_evt("RECOVERY_CMD_UNKNOWN");
    emit_recovery_response("error:unknown_command");
    return RECOVERY_ACTION_STAY;
}

static bool enter_recovery_loop(
    const bootloader_state_t *boot_state,
    const update_mode_hooks_t *update_hooks)
{
    char line[RECOVERY_CMD_MAX_LEN] = {0};
    uint32_t line_index = 0U;
    uint32_t blink_elapsed_ms = 0U;
    bool blink_bright = true;

    if (boot_state == NULL || update_hooks == NULL) {
        return false;
    }

    emit_evt("DECISION_RECOVERY");
    set_led((led_rgb_t){RECOVERY_LED_BRIGHT_R, RECOVERY_LED_BRIGHT_G, RECOVERY_LED_BRIGHT_B});

    for (;;) {
        uint8_t byte = 0U;
        bool consumed_byte = false;

        while (esp_rom_output_rx_one_char(&byte) == 0) {
            consumed_byte = true;

            if (byte == '\n' || byte == '\r') {
                line[line_index] = '\0';
                line_index = 0U;

                if (line[0] != '\0') {
                    recovery_action_t action = handle_recovery_command(line, boot_state, update_hooks);
                    if (action == RECOVERY_ACTION_BOOT) {
                        return true;
                    }

                    set_led((led_rgb_t){
                        blink_bright ? RECOVERY_LED_BRIGHT_R : RECOVERY_LED_DIM_R,
                        blink_bright ? RECOVERY_LED_BRIGHT_G : RECOVERY_LED_DIM_G,
                        blink_bright ? RECOVERY_LED_BRIGHT_B : RECOVERY_LED_DIM_B,
                    });
                    blink_elapsed_ms = 0U;
                }

                continue;
            }

            if (line_index < (RECOVERY_CMD_MAX_LEN - 1U)) {
                line[line_index++] = (char)byte;
                continue;
            }

            // Overflowed command line: flush until newline and report once.
            line_index = 0U;
            emit_evt("RECOVERY_CMD_TOO_LONG");
            emit_recovery_response("error:command_too_long");
        }

        if (!consumed_byte) {
            delay_ms(RECOVERY_POLL_MS);
            blink_elapsed_ms += RECOVERY_POLL_MS;
            if (blink_elapsed_ms >= RECOVERY_IDLE_BLINK_MS) {
                blink_bright = !blink_bright;
                set_led((led_rgb_t){
                    blink_bright ? RECOVERY_LED_BRIGHT_R : RECOVERY_LED_DIM_R,
                    blink_bright ? RECOVERY_LED_BRIGHT_G : RECOVERY_LED_DIM_G,
                    blink_bright ? RECOVERY_LED_BRIGHT_B : RECOVERY_LED_DIM_B,
                });
                blink_elapsed_ms = 0U;
            }
        }
    }
}

static void fatal_reset_loop(uint32_t code)
{
    emit_evt("FATAL_RESET");
    emit_evt_with_value("FATAL_RESET_CODE", code);

    for (uint32_t cycles = 0U;; ++cycles) {
        set_led((led_rgb_t){20, 0, 0});
        delay_ms(FATAL_BLINK_ON_MS);
        set_led((led_rgb_t){0, 0, 0});
        delay_ms(FATAL_BLINK_OFF_MS);

        if ((cycles % 12U) == 11U) {
            bootloader_reset();
        }
    }
}

// App image CRC check — runs in all boot paths between decision and LOAD_APP.
//
// Partition layout (last 8 bytes are the CRC descriptor):
//   [ app image: image_size bytes ][ 0xFF padding ][ image_size: 4 B LE ][ crc32: 4 B LE ]
//
// CRC algorithm: CRC-32/ISO-HDLC over the app image bytes only.
// Host tool: scripts/append_crc.py
// Forced fail: CONFIG_BOOTLOADER_TEST_CRC_FORCE_FAIL (Kconfig, validation only)
static bool run_app_crc_check(const bootloader_state_t *boot_state)
{
    const uint32_t half_period_ms = 1000U / (UPDATE_PULSE_HZ * 2U);
    const uint32_t flash_offset   = boot_state->factory.offset;
    const uint32_t partition_size = boot_state->factory.size;

    emit_evt("APP_CRC_CHECK");
    for (int pulse = 0; pulse < 4; ++pulse) {
        set_led((led_rgb_t){16, 10, 0});
        delay_ms(half_period_ms);
        set_led((led_rgb_t){8, 5, 0});
        delay_ms(half_period_ms);
    }

    // Partition must be large enough to hold at least the 8-byte descriptor
    if (partition_size < APP_CRC_DESCRIPTOR_SIZE) {
        emit_evt("APP_CRC_FAIL");
        set_led((led_rgb_t){20, 0, 0});
        delay_ms(UPDATE_VERIFY_FAIL_MS);
        return false;
    }

    // Read the 8-byte descriptor from the end of the partition
    app_crc_descriptor_t descriptor = {0};
    const uint32_t desc_offset = flash_offset + partition_size - APP_CRC_DESCRIPTOR_SIZE;
    esp_err_t err = bootloader_flash_read(desc_offset, &descriptor, APP_CRC_DESCRIPTOR_SIZE, false);
    if (err != ESP_OK) {
        emit_evt("APP_CRC_FAIL");
        set_led((led_rgb_t){20, 0, 0});
        delay_ms(UPDATE_VERIFY_FAIL_MS);
        return false;
    }

    // Validate stored image_size is within sane bounds
    const uint32_t max_image_size = partition_size - APP_CRC_DESCRIPTOR_SIZE;
    if (descriptor.image_size == 0U || descriptor.image_size > max_image_size) {
        emit_evt("APP_CRC_FAIL");
        set_led((led_rgb_t){20, 0, 0});
        delay_ms(UPDATE_VERIFY_FAIL_MS);
        return false;
    }

    // Compute CRC-32/ISO-HDLC over only the actual app image bytes
    uint32_t observed_crc = compute_flash_crc32(flash_offset, descriptor.image_size);

#ifdef CONFIG_BOOTLOADER_TEST_CRC_FORCE_FAIL
    // Corrupt observed value to force a mismatch — validation testing only
    observed_crc ^= UINT32_MAX;
#endif

    if (observed_crc != descriptor.crc32) {
        emit_evt("APP_CRC_FAIL");
        set_led((led_rgb_t){20, 0, 0});
        delay_ms(UPDATE_VERIFY_FAIL_MS);
        return false;
    }

    emit_evt("APP_CRC_OK");
    set_led((led_rgb_t){0, 20, 0});
    delay_ms(UPDATE_VERIFY_OK_MS);
    return true;
}

// Single-button mode selector on BOOT_GPIO (GPIO9).
// Timing constants match the baseline profile in BOOT_SEQUENCE.md §8.
static boot_mode_t run_mode_selector(void)
{
    uint32_t hold_ms;
    uint32_t mode_idx;

    if (!gpio_level_low(BOOT_GPIO)) {
        return BOOT_MODE_NORMAL;
    }

    // Button held at guard point - wait a short stable hold to arm
    hold_ms = 0U;
    while (hold_ms < ARM_DETECT_MS) {
        if (!gpio_level_low(BOOT_GPIO)) {
            // Released before arm threshold - proceed with normal boot
            return BOOT_MODE_NORMAL;
        }
        delay_ms(10U);
        hold_ms += 10U;
    }

    // Selector armed: enter first mode and show its color
    emit_evt("MODE_SELECT_ARMED");
    mode_idx = 0U;
    set_led(s_mode_ring_colors[mode_idx]);
    ets_printf("BL_EVT:MODE_SELECTED:%s\n", s_mode_ring_names[mode_idx]);

    // Wait for button release; release alone does not change selection
    while (gpio_level_low(BOOT_GPIO)) {
        delay_ms(10U);
        feed_watchdog();
    }

    // Interaction loop: short press = cycle, long press = execute
    for (;;) {
        feed_watchdog();
        if (!gpio_level_low(BOOT_GPIO)) {
            delay_ms(10U);
            continue;
        }

        // Button pressed - classify as short or long press
        uint32_t press_ms = 0U;
        while (gpio_level_low(BOOT_GPIO)) {
            delay_ms(10U);
            feed_watchdog();
            press_ms += 10U;
            if (press_ms >= LONG_PRESS_MIN_MS) {
                // Long press confirmed - execute current mode immediately
                ets_printf("BL_EVT:MODE_EXECUTE:%s\n", s_mode_ring_names[mode_idx]);
                return s_mode_ring[mode_idx];
            }
        }

        if (press_ms >= SHORT_PRESS_MIN_MS && press_ms <= SHORT_PRESS_MAX_MS) {
            // Short press: advance to next mode in ring
            mode_idx = (mode_idx + 1U) % MODE_RING_SIZE;
            set_led(s_mode_ring_colors[mode_idx]);
            ets_printf("BL_EVT:MODE_SELECTED:%s\n", s_mode_ring_names[mode_idx]);
        }
        // else: bounce or sub-minimum press - ignore
    }
}

// Main Bootloader Entry Point
void __attribute__((noreturn)) call_start_cpu0(void)
{
    publish_boot_context(BOOT_CTX_UNKNOWN);

    configure_input_gpio(BOOT_GPIO);

    if (bootloader_init() != ESP_OK) {
        fatal_reset_loop(1U);
    }

    ws2812_init(WS2812_DEFAULT_GPIO);
    set_led((led_rgb_t){0, 0, 0});

    emit_evt(s_normal_phases[0].token);  // INIT
    set_led(s_normal_phases[0].color);
    delay_ms(phase_duration_ms(s_normal_phases[0].visual_percent));

    emit_evt(s_normal_phases[1].token);  // HW_READY
    set_led(s_normal_phases[1].color);
    delay_ms(phase_duration_ms(s_normal_phases[1].visual_percent));

    bootloader_state_t boot_state = {0};
    if (!bootloader_utility_load_partition_table(&boot_state)) {
        fatal_reset_loop(2U);
    }

    emit_evt(s_normal_phases[2].token);  // PARTITION_TABLE_OK
    set_led(s_normal_phases[2].color);
    delay_ms(phase_duration_ms(s_normal_phases[2].visual_percent));

    delay_ms(USB_RECONNECT_MS);

    const update_mode_hooks_t update_hooks = {
        .delay_ms = delay_ms,
        .emit_evt = emit_evt,
        .emit_evt_with_value = emit_evt_with_value,
        .set_led_rgb = set_led_rgb,
        .run_app_crc_check = run_app_crc_check,
    };

    boot_mode_t selected_mode = run_mode_selector();

    if (selected_mode == BOOT_MODE_RECOVERY) {
        publish_boot_context(BOOT_CTX_RECOVERY);
        if (!enter_recovery_loop(&boot_state, &update_hooks)) {
            fatal_reset_loop(4U);
        }
        selected_mode = BOOT_MODE_NORMAL;
    }

    if (selected_mode == BOOT_MODE_UPDATE) {
        set_led(s_normal_phases[3].color);
        delay_ms(phase_duration_ms(s_normal_phases[3].visual_percent));
        if (!handle_update_mode(&boot_state, &update_hooks)) {
            publish_boot_context(BOOT_CTX_RECOVERY);
            if (!enter_recovery_loop(&boot_state, &update_hooks)) {
                fatal_reset_loop(5U);
            }
            selected_mode = BOOT_MODE_NORMAL;
        }
    } else {
        emit_evt(s_normal_phases[3].token);  // DECISION_NORMAL
        set_led(s_normal_phases[3].color);
        delay_ms(phase_duration_ms(s_normal_phases[3].visual_percent));

        // CRC check runs in all non-recovery paths.
        if (!run_app_crc_check(&boot_state)) {
            publish_boot_context(BOOT_CTX_RECOVERY);
            if (!enter_recovery_loop(&boot_state, &update_hooks)) {
                fatal_reset_loop(6U);
            }
            selected_mode = BOOT_MODE_NORMAL;
        }
    }

    if (selected_mode == BOOT_MODE_UPDATE) {
        publish_boot_context(BOOT_CTX_UPDATE);
    } else {
        publish_boot_context(BOOT_CTX_NORMAL);
    }

    emit_evt(s_normal_phases[4].token);  // LOAD_APP
    set_led(s_normal_phases[4].color);
    delay_ms(phase_duration_ms(s_normal_phases[4].visual_percent));

    emit_evt(s_normal_phases[5].token);  // HANDOFF
    set_led(s_normal_phases[5].color);
    delay_ms(phase_duration_ms(s_normal_phases[5].visual_percent));

    set_led((led_rgb_t){0, 0, 0});

    emit_evt("HANDOFF_APP");
    bootloader_utility_load_boot_image(&boot_state, 0);

    fatal_reset_loop(3U);
}
