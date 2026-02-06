// Bootloader logging functions
#ifndef BOOTLOADER_LOGGING_H
#define BOOTLOADER_LOGGING_H
void bootlog_append(const char* msg);
void bootlog_flush(void);
void bootlog_buffer(const char* msg);
void bootlog_print(const char* msg);
#endif // BOOTLOADER_LOGGING_H
