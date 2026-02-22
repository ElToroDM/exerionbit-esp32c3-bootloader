// Bootloader logging with buffering
#include "rom/ets_sys.h"
#include "logging.h"

#define BOOTLOG_BUFFER_SIZE 1024

static char bootlog_buf[BOOTLOG_BUFFER_SIZE];
static uint16_t bootlog_index = 0;

void bootlog_buffer(const char* msg)
{
    if (!msg || bootlog_index >= BOOTLOG_BUFFER_SIZE - 2) return;
    
    const char* src = msg;
    while (*src && bootlog_index < BOOTLOG_BUFFER_SIZE - 1) {
        bootlog_buf[bootlog_index++] = *src++;
    }
    if (bootlog_index < BOOTLOG_BUFFER_SIZE - 1) {
        bootlog_buf[bootlog_index++] = '\n';
    }
}

void bootlog_flush(void)
{
    if (bootlog_index > 0) {
        bootlog_buf[bootlog_index] = '\0';
        ets_printf("=== BOOTLOADER LOG BUFFER START ===\n");
        ets_printf("%s", bootlog_buf);
        ets_printf("=== BOOTLOADER LOG BUFFER END ===\n");
        bootlog_index = 0;  // Reset buffer after flush
    }
}

void bootlog_print(const char* msg)
{
    ets_printf("%s\n", msg);
}
