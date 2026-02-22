# Custom bootloader main component that replaces the default ESP-IDF bootloader
idf_component_register(SRCS "ws2812.c" "main.c" "logging.c"
                       INCLUDE_DIRS "."
                       REQUIRES bootloader_support log spi_flash)

# This component provides the main entry point for the bootloader
# It replaces ESP-IDF's default bootloader/subproject/main component
