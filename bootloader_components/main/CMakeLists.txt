# Bootloader main component
idf_component_register(
    SRCS
        "main.c"
        "ws2812.c"
        "logging.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        bootloader_support
        esp_hal_wdt
        log
        spi_flash
)
