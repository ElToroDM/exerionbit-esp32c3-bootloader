# ESP-IDF CMakeLists.txt for waveshare_esp32c3_zero_bootloader
cmake_minimum_required(VERSION 3.16)

# Include custom bootloader component directory in bootloader build
set(BOOTLOADER_EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bootloader_components/boot_core")

# app_test replaces the default main/ directory; register it explicitly
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/app_test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(waveshare_esp32c3_zero_bootloader)
