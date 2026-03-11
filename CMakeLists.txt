# ESP-IDF CMakeLists.txt for waveshare_esp32c3_zero_bootloader
cmake_minimum_required(VERSION 3.16)

# Include overriding bootloader main component in bootloader build
set(BOOTLOADER_EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bootloader_components/main")

# app_test replaces the default main/ directory; register it explicitly
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/app_test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(waveshare_esp32c3_zero_bootloader)

# Enforce CRC-OK app image on every build so ESP-IDF extension Flash uses a valid descriptor.
set(APP_BIN_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin")

add_custom_target(crc_ok_build ALL
	COMMAND ${PYTHON} ${CMAKE_CURRENT_LIST_DIR}/scripts/append_crc.py
		${APP_BIN_PATH}
		--out-image ${APP_BIN_PATH}
	DEPENDS app
	COMMENT "Regenerating CRC-OK app image for extension Flash"
	VERBATIM
)
