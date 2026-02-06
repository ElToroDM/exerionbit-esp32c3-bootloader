#pragma once

#include <stdint.h>

// Default GPIO for Waveshare ESP32-C3 Zero onboard WS2812
// Adjust if your board uses a different pin.
#define WS2812_DEFAULT_GPIO 10

void ws2812_init(int gpio);
void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b);
