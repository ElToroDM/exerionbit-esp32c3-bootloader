#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bootloader_utility.h"

typedef struct {
    void (*delay_ms)(uint32_t delay_ms_value);
    void (*emit_evt)(const char *token);
    void (*emit_evt_with_value)(const char *token, uint32_t value);
    void (*set_led_rgb)(uint8_t r, uint8_t g, uint8_t b);
    bool (*run_app_crc_check)(const bootloader_state_t *boot_state);
} update_mode_hooks_t;

bool handle_update_mode(const bootloader_state_t *boot_state, const update_mode_hooks_t *hooks);
