#include <stdint.h>
#include "sdkconfig.h"
#include "soc/soc.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "rom/ets_sys.h"

#include "ws2812.h"

#ifndef GPIO_FUNC_OUT_SEL_CFG_REG
#define GPIO_FUNC_OUT_SEL_CFG_REG(i) (GPIO_FUNC0_OUT_SEL_CFG_REG + ((i) * 4))
#endif

// WS2812 timing - using NOPs for precise bit-banging
// At 160MHz: 1 cycle = 6.25ns
// WS2812 spec: T0H=0.4us±0.15us, T0L=0.85us±0.15us, T1H=0.8us±0.15us, T1L=0.45us±0.15us
#define WS2812_RESET_US 300

static int s_ws2812_gpio = -1;

#define NOP() __asm__ __volatile__("nop" ::: "memory")
#define NOP4() do { NOP(); NOP(); NOP(); NOP(); } while(0)
#define NOP8() do { NOP4(); NOP4(); } while(0)
#define NOP16() do { NOP8(); NOP8(); } while(0)

static inline void gpio_set_high(int pin)
{
    REG_WRITE(GPIO_OUT_W1TS_REG, (1U << pin));
}

static inline void gpio_set_low(int pin)
{
    REG_WRITE(GPIO_OUT_W1TC_REG, (1U << pin));
}

static inline void ws2812_send_bit_0(int pin)
{
    gpio_set_high(pin);
    // T0H: ~0.4us - more NOPs for safety
    NOP8(); NOP4();
    gpio_set_low(pin);
    // T0L: ~0.85us
    NOP16(); NOP16();
}

static inline void ws2812_send_bit_1(int pin)
{
    gpio_set_high(pin);
    // T1H: ~0.8us
    NOP16(); NOP16();
    gpio_set_low(pin);
    // T1L: ~0.45us
    NOP8(); NOP4();
}

static inline void ws2812_send_bit(int pin, int bit)
{
    if (bit) {
        ws2812_send_bit_1(pin);
    } else {
        ws2812_send_bit_0(pin);
    }
}

void ws2812_init(int gpio)
{
    s_ws2812_gpio = gpio;

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
    REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(gpio), SIG_GPIO_OUT_IDX);
    REG_WRITE(GPIO_ENABLE_W1TS_REG, (1U << gpio));
    gpio_set_low(gpio);
    ets_delay_us(WS2812_RESET_US);
}

void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    int pin = s_ws2812_gpio;
    if (pin < 0) {
        return;
    }

    ets_intr_lock();

    // This LED uses RGB order (not the standard GRB)
    uint8_t colors[3] = { r, g, b };

    for (int ci = 0; ci < 3; ++ci) {
        uint8_t v = colors[ci];
        for (int i = 7; i >= 0; --i) {
            ws2812_send_bit(pin, (v >> i) & 0x01);
        }
    }

    ets_intr_unlock();
    ets_delay_us(WS2812_RESET_US);
}
