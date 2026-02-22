# Bootloader override component named "main" using sources from boot_core
idf_component_register(
    SRCS
        "../boot_core/main.c"
        "../boot_core/ws2812.c"
        "../boot_core/logging.c"
    INCLUDE_DIRS
        "../boot_core"
    REQUIRES
        bootloader_support
        log
        spi_flash
)
