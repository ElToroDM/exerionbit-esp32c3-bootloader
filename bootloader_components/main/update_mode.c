#include "update_mode.h"

#include <string.h>

#include "bootloader_flash_priv.h"
#include "esp_rom_serial_output.h"

#define UPDATE_CHUNK_MAX_SIZE      1024U
#define UPDATE_HEADER_MAX_LEN       96U
#define UPDATE_CMD_MAX_LEN          32U
#define UPDATE_CHUNK_TIMEOUT_MS   120000U
#define UPDATE_MAX_ERRORS            10U
#define UPDATE_MAX_TRACKED_SECTORS 1024U

#define UPDATE_WAIT_BLINK_MS       250U

typedef struct {
    const update_mode_hooks_t *hooks;
    uint32_t blink_elapsed_ms;
    bool led_bright;
} update_runtime_t;

static void set_led(update_runtime_t *runtime, uint8_t r, uint8_t g, uint8_t b)
{
    runtime->hooks->set_led_rgb(r, g, b);
}

static void apply_wait_led_state(update_runtime_t *runtime)
{
    if (runtime->led_bright) {
        set_led(runtime, 16U, 10U, 0U);
    } else {
        set_led(runtime, 4U, 2U, 0U);
    }
}

static void tick_wait_blink(update_runtime_t *runtime)
{
    runtime->blink_elapsed_ms += 1U;
    if (runtime->blink_elapsed_ms >= UPDATE_WAIT_BLINK_MS) {
        runtime->blink_elapsed_ms = 0U;
        runtime->led_bright = !runtime->led_bright;
        apply_wait_led_state(runtime);
    }
}

static void pulse_led(update_runtime_t *runtime, uint8_t r, uint8_t g, uint8_t b, uint32_t hold_ms)
{
    set_led(runtime, r, g, b);
    runtime->hooks->delay_ms(hold_ms);
    apply_wait_led_state(runtime);
}

static uint16_t crc16_ccitt(uint16_t seed, const uint8_t *data, uint32_t len)
{
    uint16_t crc = seed;

    for (uint32_t i = 0U; i < len; ++i) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static bool parse_u32_decimal(const char *text, uint32_t *out)
{
    uint32_t value = 0U;

    if (text == NULL || out == NULL || *text == '\0') {
        return false;
    }

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        uint32_t digit = (uint32_t)(*p - '0');
        if (value > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }

    *out = value;
    return true;
}

static bool uart_read_byte_timeout(update_runtime_t *runtime, uint8_t *out, uint32_t timeout_ms)
{
    uint32_t waited_ms = 0U;

    while (waited_ms < timeout_ms) {
        if (esp_rom_output_rx_one_char(out) == 0) {
            return true;
        }
        runtime->hooks->delay_ms(1U);
        tick_wait_blink(runtime);
        ++waited_ms;
    }

    return false;
}

static bool uart_read_line_timeout(
    update_runtime_t *runtime,
    char *line,
    uint32_t line_capacity,
    uint32_t timeout_ms)
{
    uint32_t index = 0U;

    if (line == NULL || line_capacity < 2U) {
        return false;
    }

    while (index < (line_capacity - 1U)) {
        uint8_t byte = 0U;
        if (!uart_read_byte_timeout(runtime, &byte, timeout_ms)) {
            return false;
        }

        if (byte == '\n') {
            line[index] = '\0';
            return true;
        }

        if (byte == '\r') {
            continue;
        }

        line[index++] = (char)byte;
    }

    line[line_capacity - 1U] = '\0';
    return false;
}

static bool parse_chunk_header(const char *line, uint32_t *len, uint32_t *offset, uint32_t *crc16)
{
    char len_str[16] = {0};
    char offset_str[16] = {0};
    char crc16_str[16] = {0};
    size_t line_len = 0U;
    size_t i = 0U;
    size_t field_pos = 0U;

    if (line == NULL || len == NULL || offset == NULL || crc16 == NULL) {
        return false;
    }

    line_len = strlen(line);
    if (line_len < 7U || line[0] != '[' || line[line_len - 1U] != ']') {
        return false;
    }

    for (i = 1U; i < (line_len - 1U) && line[i] != ':'; ++i) {
        if (field_pos >= (sizeof(len_str) - 1U)) {
            return false;
        }
        len_str[field_pos++] = line[i];
    }
    if (i >= (line_len - 1U) || line[i] != ':') {
        return false;
    }

    field_pos = 0U;
    for (++i; i < (line_len - 1U) && line[i] != ':'; ++i) {
        if (field_pos >= (sizeof(offset_str) - 1U)) {
            return false;
        }
        offset_str[field_pos++] = line[i];
    }
    if (i >= (line_len - 1U) || line[i] != ':') {
        return false;
    }

    field_pos = 0U;
    for (++i; i < (line_len - 1U); ++i) {
        if (field_pos >= (sizeof(crc16_str) - 1U)) {
            return false;
        }
        crc16_str[field_pos++] = line[i];
    }

    return parse_u32_decimal(len_str, len)
        && parse_u32_decimal(offset_str, offset)
        && parse_u32_decimal(crc16_str, crc16);
}

static bool read_chunk_payload(update_runtime_t *runtime, uint8_t *buffer, uint32_t len, uint32_t timeout_ms)
{
    uint32_t waited_ms = 0U;
    uint32_t received = 0U;

    if (buffer == NULL) {
        return false;
    }

    while (received < len) {
        if (esp_rom_output_rx_one_char(&buffer[received]) == 0) {
            ++received;
            continue;
        }

        runtime->hooks->delay_ms(1U);
        tick_wait_blink(runtime);
        ++waited_ms;
        if (waited_ms >= timeout_ms) {
            return false;
        }
    }

    return true;
}

bool handle_update_mode(const bootloader_state_t *boot_state, const update_mode_hooks_t *hooks)
{
    static bool s_erased_sector[UPDATE_MAX_TRACKED_SECTORS];
    static uint8_t s_update_chunk[UPDATE_CHUNK_MAX_SIZE];
    update_runtime_t runtime = {
        .hooks = hooks,
        .blink_elapsed_ms = 0U,
        .led_bright = true,
    };
    char line[UPDATE_HEADER_MAX_LEN] = {0};
    uint32_t errors = 0U;
    const uint32_t partition_offset = boot_state->factory.offset;
    const uint32_t partition_size = boot_state->factory.size;
    const uint32_t partition_sectors = partition_size / FLASH_SECTOR_SIZE;

    if (boot_state == NULL || hooks == NULL) {
        return false;
    }

    if ((partition_size % FLASH_SECTOR_SIZE) != 0U || partition_sectors > UPDATE_MAX_TRACKED_SECTORS) {
        hooks->emit_evt("UPDATE_SETUP_FAIL");
        return false;
    }

    memset(s_erased_sector, 0, sizeof(s_erased_sector));

    hooks->emit_evt("DECISION_UPDATE");
    hooks->emit_evt("READY_FOR_UPDATE");
    apply_wait_led_state(&runtime);

    if (!uart_read_line_timeout(&runtime, line, UPDATE_CMD_MAX_LEN, UPDATE_CHUNK_TIMEOUT_MS) || strcmp(line, "START_UPDATE") != 0) {
        hooks->emit_evt("UPDATE_START_FAIL");
        pulse_led(&runtime, 20U, 0U, 0U, 120U);
        return false;
    }

    for (;;) {
        uint32_t chunk_len = 0U;
        uint32_t chunk_offset = 0U;
        uint32_t chunk_crc16 = 0U;
        uint32_t frame_crc16 = 0U;
        uint32_t write_addr = 0U;
        uint32_t first_sector_rel = 0U;
        uint32_t last_sector_rel = 0U;

        hooks->emit_evt("READY_FOR_CHUNK");

        if (!uart_read_line_timeout(&runtime, line, UPDATE_HEADER_MAX_LEN, UPDATE_CHUNK_TIMEOUT_MS)) {
            ++errors;
            hooks->emit_evt_with_value("CHUNK_FAIL", 0U);
            pulse_led(&runtime, 20U, 0U, 0U, 80U);
            if (errors >= UPDATE_MAX_ERRORS) {
                return false;
            }
            continue;
        }

        if (strcmp(line, "END_UPDATE") == 0) {
            hooks->emit_evt("UPDATE_SESSION_END");
            return hooks->run_app_crc_check(boot_state);
        }

        if (!parse_chunk_header(line, &chunk_len, &chunk_offset, &chunk_crc16)) {
            ++errors;
            hooks->emit_evt_with_value("CHUNK_FAIL", 0U);
            pulse_led(&runtime, 20U, 0U, 0U, 80U);
            if (errors >= UPDATE_MAX_ERRORS) {
                return false;
            }
            continue;
        }

        if (chunk_len == 0U
            || chunk_len > UPDATE_CHUNK_MAX_SIZE
            || (chunk_len % 4U) != 0U
            || (chunk_offset % 4U) != 0U
            || chunk_offset >= partition_size
            || chunk_len > (partition_size - chunk_offset)) {
            ++errors;
            hooks->emit_evt_with_value("CHUNK_FAIL", chunk_offset);
            pulse_led(&runtime, 20U, 0U, 0U, 80U);
            if (errors >= UPDATE_MAX_ERRORS) {
                return false;
            }
            continue;
        }

        if (!read_chunk_payload(&runtime, s_update_chunk, chunk_len, UPDATE_CHUNK_TIMEOUT_MS)) {
            ++errors;
            hooks->emit_evt_with_value("CHUNK_FAIL", chunk_offset);
            pulse_led(&runtime, 20U, 0U, 0U, 80U);
            if (errors >= UPDATE_MAX_ERRORS) {
                return false;
            }
            continue;
        }

        frame_crc16 = (uint32_t)crc16_ccitt(0xFFFFU, s_update_chunk, chunk_len);
        if ((frame_crc16 & 0xFFFFU) != (chunk_crc16 & 0xFFFFU)) {
            ++errors;
            hooks->emit_evt_with_value("CHUNK_FAIL", chunk_offset);
            pulse_led(&runtime, 20U, 0U, 0U, 80U);
            if (errors >= UPDATE_MAX_ERRORS) {
                return false;
            }
            continue;
        }

        first_sector_rel = chunk_offset / FLASH_SECTOR_SIZE;
        last_sector_rel = (chunk_offset + chunk_len - 1U) / FLASH_SECTOR_SIZE;

        for (uint32_t sector = first_sector_rel; sector <= last_sector_rel; ++sector) {
            if (!s_erased_sector[sector]) {
                const uint32_t absolute_sector = (partition_offset / FLASH_SECTOR_SIZE) + sector;
                if (bootloader_flash_erase_sector(absolute_sector) != ESP_OK) {
                    ++errors;
                    hooks->emit_evt_with_value("CHUNK_FAIL", chunk_offset);
                    pulse_led(&runtime, 20U, 0U, 0U, 80U);
                    if (errors >= UPDATE_MAX_ERRORS) {
                        return false;
                    }
                    goto next_chunk;
                }
                s_erased_sector[sector] = true;
            }
        }

        write_addr = partition_offset + chunk_offset;
        if (bootloader_flash_write(write_addr, s_update_chunk, chunk_len, false) != ESP_OK) {
            ++errors;
            hooks->emit_evt_with_value("CHUNK_FAIL", chunk_offset);
            pulse_led(&runtime, 20U, 0U, 0U, 80U);
            if (errors >= UPDATE_MAX_ERRORS) {
                return false;
            }
            continue;
        }

        errors = 0U;
        hooks->emit_evt_with_value("CHUNK_OK", chunk_offset);
        pulse_led(&runtime, 0U, 20U, 0U, 60U);
        continue;

    next_chunk:
        continue;
    }
}
