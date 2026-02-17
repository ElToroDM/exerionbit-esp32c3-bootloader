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
#include "ws2812.h"

#define TOTAL_VISUAL_MS            4000U
#define USB_RECONNECT_MS           1000U

#define RECOVERY_TRIGGER_GPIO      9
#define UPDATE_TRIGGER_GPIO        8
#define CRC_FAIL_TRIGGER_GPIO      7

#define UPDATE_PULSE_HZ            2U
#define UPDATE_VERIFY_OK_MS        200U
#define UPDATE_VERIFY_FAIL_MS      300U

#define FATAL_BLINK_PERIOD_MS      333U
#define FATAL_BLINK_ON_MS          (FATAL_BLINK_PERIOD_MS / 2U)
#define FATAL_BLINK_OFF_MS         (FATAL_BLINK_PERIOD_MS - FATAL_BLINK_ON_MS)

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

static void set_led(led_rgb_t color)
{
    ws2812_set_rgb(color.r, color.g, color.b);
}

static void configure_input_gpio(int gpio)
{
    esp_rom_gpio_pad_select_gpio(gpio);
    REG_WRITE(GPIO_ENABLE_W1TC_REG, (1U << gpio));
}

static bool gpio_level_low(int gpio)
{
    return ((REG_READ(GPIO_IN_REG) & (1U << gpio)) == 0U);
}

static bool gpio_level_low_stable(int gpio)
{
    for (int sample = 0; sample < 5; ++sample) {
        if (!gpio_level_low(gpio)) {
            return false;
        }
        delay_ms(2U);
    }
    return true;
}

static bool evaluate_recovery_trigger(void)
{
    return gpio_level_low_stable(RECOVERY_TRIGGER_GPIO);
}

static bool evaluate_update_candidate(void)
{
    return gpio_level_low_stable(UPDATE_TRIGGER_GPIO);
}

static bool evaluate_crc_fail_trigger(void)
{
    return gpio_level_low_stable(CRC_FAIL_TRIGGER_GPIO);
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

static void run_update_check_visual(void)
{
    const uint32_t half_period_ms = 1000U / (UPDATE_PULSE_HZ * 2U);

    emit_evt("UPDATE_CHECK");
    for (int pulse = 0; pulse < 4; ++pulse) {
        set_led((led_rgb_t){16, 10, 0});
        delay_ms(half_period_ms);
        set_led((led_rgb_t){8, 5, 0});
        delay_ms(half_period_ms);
    }
}

// Main Bootloader Entry Point
void __attribute__((noreturn)) call_start_cpu0(void)
{
    configure_input_gpio(RECOVERY_TRIGGER_GPIO);
    configure_input_gpio(UPDATE_TRIGGER_GPIO);
    configure_input_gpio(CRC_FAIL_TRIGGER_GPIO);

    if (bootloader_init() != ESP_OK) {
        fatal_reset_loop(1U);
    }

    ws2812_init(WS2812_DEFAULT_GPIO);
    set_led((led_rgb_t){0, 0, 0});

    emit_evt(s_normal_phases[0].token);
    set_led(s_normal_phases[0].color);
    delay_ms(phase_duration_ms(s_normal_phases[0].visual_percent));

    emit_evt(s_normal_phases[1].token);
    set_led(s_normal_phases[1].color);
    delay_ms(phase_duration_ms(s_normal_phases[1].visual_percent));

    bootloader_state_t boot_state = {0};
    if (!bootloader_utility_load_partition_table(&boot_state)) {
        fatal_reset_loop(2U);
    }

    emit_evt(s_normal_phases[2].token);
    set_led(s_normal_phases[2].color);
    delay_ms(phase_duration_ms(s_normal_phases[2].visual_percent));

    delay_ms(USB_RECONNECT_MS);

    if (evaluate_recovery_trigger()) {
        enter_recovery_loop();
    }

    emit_evt(s_normal_phases[3].token);
    set_led(s_normal_phases[3].color);
    delay_ms(phase_duration_ms(s_normal_phases[3].visual_percent));

    run_update_check_visual();

    if (evaluate_update_candidate()) {
        uint32_t observed_crc = calculate_partition_descriptor_crc(&boot_state);
        uint32_t expected_crc = observed_crc;

        if (evaluate_crc_fail_trigger()) {
            expected_crc ^= UINT32_MAX;
        }

        if (observed_crc != expected_crc) {
            emit_evt("UPDATE_VERIFY_FAIL");
            set_led((led_rgb_t){20, 0, 0});
            delay_ms(UPDATE_VERIFY_FAIL_MS);
            enter_recovery_loop();
        }
    }

    emit_evt("UPDATE_VERIFY_OK");
    set_led((led_rgb_t){0, 20, 0});
    delay_ms(UPDATE_VERIFY_OK_MS);

    emit_evt(s_normal_phases[4].token);
    set_led(s_normal_phases[4].color);
    delay_ms(phase_duration_ms(s_normal_phases[4].visual_percent));

    emit_evt(s_normal_phases[5].token);
    set_led(s_normal_phases[5].color);
    delay_ms(phase_duration_ms(s_normal_phases[5].visual_percent));

    set_led((led_rgb_t){0, 0, 0});

    emit_evt("HANDOFF_APP");
    bootloader_utility_load_boot_image(&boot_state, 0);

    fatal_reset_loop(3U);
}
