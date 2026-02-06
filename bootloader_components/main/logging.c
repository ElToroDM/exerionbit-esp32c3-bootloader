// Bootloader logging implementation
#include <string.h>
#include "rom/ets_sys.h"
#include "logging.h"

#define BOOTLOG_BUFFER_SIZE 2048

static char bootlog_buf[BOOTLOG_BUFFER_SIZE];
static size_t bootlog_index = 0;

void bootlog_append(const char* msg)
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

void bootlog_flush(void)
{
    if (bootlog_index > 0) {
        bootlog_buf[bootlog_index] = '\0';
        ets_printf("=== BOOTLOADER LOG BUFFER START ===\n");
        ets_printf("%s", bootlog_buf);
        ets_printf("=== BOOTLOADER LOG BUFFER END ===\n");
    }
}

void bootlog_buffer(const char* msg)
{
    // reuse existing append logic (adds newline)
    bootlog_append(msg);
}

void bootlog_print(const char* msg)
{
    ets_printf("%s\n", msg);
}
