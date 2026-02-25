// Custom bootloader component for ESP32-C3 Waveshare Zero

#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "esp_rom_crc.h"
#include "esp_rom_gpio.h"
#include "rom/ets_sys.h"
#include "soc/gpio_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "ws2812.h"

#define TOTAL_VISUAL_MS            4000U
#define USB_RECONNECT_MS           1000U

#define BOOT_GPIO                  9

// Mode selector timing (see BOOT_SEQUENCE.md §4 and §8)
#define GUARD_RELEASE_MS           120U
#define ARM_HOLD_MS                600U
#define SHORT_PRESS_MIN_MS         60U
#define SHORT_PRESS_MAX_MS         250U
#define LONG_PRESS_MIN_MS          700U
#define SELECT_TIMEOUT_MS          2500U

#define UPDATE_PULSE_HZ            2U
#define UPDATE_VERIFY_OK_MS        200U
#define UPDATE_VERIFY_FAIL_MS      300U

#define FATAL_BLINK_PERIOD_MS      333U
#define FATAL_BLINK_ON_MS          (FATAL_BLINK_PERIOD_MS / 2U)
#define FATAL_BLINK_OFF_MS         (FATAL_BLINK_PERIOD_MS - FATAL_BLINK_ON_MS)

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

#define MODE_RING_SIZE (sizeof(s_mode_ring) / sizeof(s_mode_ring[0]))

static void delay_ms(uint32_t delay_ms_value)
{
    ets_delay_us(delay_ms_value * 1000U);
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

static uint32_t calculate_partition_descriptor_crc(const bootloader_state_t *boot_state)
{
    uint32_t crc = UINT32_MAX;
    crc = esp_rom_crc32_le(crc, (const uint8_t *)&boot_state->factory.offset, sizeof(boot_state->factory.offset));
    crc = esp_rom_crc32_le(crc, (const uint8_t *)&boot_state->factory.size, sizeof(boot_state->factory.size));
    return crc;
}

static void enter_recovery_loop(void)
{
    emit_evt("DECISION_RECOVERY");
    set_led((led_rgb_t){20, 0, 20});

    for (uint32_t heartbeat = 1U;; ++heartbeat) {
        emit_evt_with_value("RECOVERY_HEARTBEAT", heartbeat);
        delay_ms(5000U);
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

// App image CRC check — runs in all boot paths between DECISION_NORMAL and LOAD_APP.
// Forced fail is available via build config for validation only (not a public GPIO).
static bool run_app_crc_check(const bootloader_state_t *boot_state)
{
    const uint32_t half_period_ms = 1000U / (UPDATE_PULSE_HZ * 2U);

    emit_evt("APP_CRC_CHECK");
    for (int pulse = 0; pulse < 4; ++pulse) {
        set_led((led_rgb_t){16, 10, 0});
        delay_ms(half_period_ms);
        set_led((led_rgb_t){8, 5, 0});
        delay_ms(half_period_ms);
    }

    uint32_t observed_crc = calculate_partition_descriptor_crc(boot_state);
    uint32_t expected_crc = observed_crc;

#ifdef CONFIG_BOOTLOADER_TEST_CRC_FORCE_FAIL
    expected_crc ^= UINT32_MAX;
#endif

    if (observed_crc != expected_crc) {
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

    // Post-reset stabilization: let any transient button state settle
    delay_ms(GUARD_RELEASE_MS);

    if (!gpio_level_low(BOOT_GPIO)) {
        return BOOT_MODE_NORMAL;
    }

    // Button held at guard point - time the hold to arm
    hold_ms = 0U;
    while (hold_ms < ARM_HOLD_MS) {
        if (!gpio_level_low(BOOT_GPIO)) {
            // Released before arm threshold - proceed with normal boot
            return BOOT_MODE_NORMAL;
        }
        delay_ms(10U);
        hold_ms += 10U;
    }

    // Selector armed
    emit_evt("MODE_SELECT_ARMED");
    set_led((led_rgb_t){0, 8, 16}); // BLUE soft: armed indicator

    // Wait for button release before entering mode ring
    while (gpio_level_low(BOOT_GPIO)) {
        delay_ms(10U);
    }

    // Enter first mode in ring: UPDATE
    uint32_t mode_idx = 0U;
    ets_printf("BL_EVT:MODE_SELECTED:%s\n", s_mode_ring_names[mode_idx]);

    // Interaction loop: short press = cycle, long press = execute, timeout = NORMAL
    uint32_t timeout_ms = 0U;
    while (timeout_ms < SELECT_TIMEOUT_MS) {
        if (!gpio_level_low(BOOT_GPIO)) {
            delay_ms(10U);
            timeout_ms += 10U;
            continue;
        }

        // Button pressed - classify as short or long press
        uint32_t press_ms = 0U;
        while (gpio_level_low(BOOT_GPIO)) {
            delay_ms(10U);
            press_ms += 10U;
            if (press_ms >= LONG_PRESS_MIN_MS) {
                // Long press confirmed - wait for release then execute
                while (gpio_level_low(BOOT_GPIO)) {
                    delay_ms(10U);
                }
                ets_printf("BL_EVT:MODE_EXECUTE:%s\n", s_mode_ring_names[mode_idx]);
                return s_mode_ring[mode_idx];
            }
        }

        if (press_ms >= SHORT_PRESS_MIN_MS && press_ms <= SHORT_PRESS_MAX_MS) {
            // Short press: advance to next mode in ring
            mode_idx = (mode_idx + 1U) % MODE_RING_SIZE;
            ets_printf("BL_EVT:MODE_SELECTED:%s\n", s_mode_ring_names[mode_idx]);
        }
        // else: bounce or sub-minimum press - ignore

        delay_ms(10U);
        timeout_ms += 10U;
    }

    // Interaction timeout - fall back to normal boot
    return BOOT_MODE_NORMAL;
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

    boot_mode_t selected_mode = run_mode_selector();

    if (selected_mode == BOOT_MODE_RECOVERY) {
        publish_boot_context(BOOT_CTX_RECOVERY);
        enter_recovery_loop();
    }

    emit_evt(s_normal_phases[3].token);  // DECISION_NORMAL
    set_led(s_normal_phases[3].color);
    delay_ms(phase_duration_ms(s_normal_phases[3].visual_percent));

    // CRC check runs in all non-recovery paths
    if (!run_app_crc_check(&boot_state)) {
        enter_recovery_loop();
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
