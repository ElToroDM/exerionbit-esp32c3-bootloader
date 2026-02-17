#include <stdio.h>

#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_system.h"

#define BOOT_CTX_MAGIC             0x424C4358U
#define BOOT_CTX_UNKNOWN           0U
#define BOOT_CTX_NORMAL            1U
#define BOOT_CTX_RECOVERY          2U
#define BOOT_CTX_UPDATE            3U
#define BOOT_CTX_MAGIC_REG         RTC_CNTL_STORE6_REG
#define BOOT_CTX_VALUE_REG         RTC_CNTL_STORE7_REG

#define APP_HEARTBEAT_PERIOD_MS    2000U

#ifdef CONFIG_APP_ENABLE_FAULT_HOOK
#define APP_FAULT_HOOK_GPIO        CONFIG_APP_FAULT_HOOK_GPIO
#endif

static uint32_t read_boot_context(void)
{
    uint32_t magic = REG_READ(BOOT_CTX_MAGIC_REG);
    uint32_t context = REG_READ(BOOT_CTX_VALUE_REG);

    if (magic != BOOT_CTX_MAGIC) {
        return BOOT_CTX_UNKNOWN;
    }

    REG_WRITE(BOOT_CTX_MAGIC_REG, 0U);
    REG_WRITE(BOOT_CTX_VALUE_REG, 0U);

    if (context > BOOT_CTX_UPDATE) {
        return BOOT_CTX_UNKNOWN;
    }

    return context;
}

static const char *boot_context_to_token(uint32_t context)
{
    switch (context) {
    case BOOT_CTX_NORMAL:
        return "NORMAL";
    case BOOT_CTX_RECOVERY:
        return "RECOVERY";
    case BOOT_CTX_UPDATE:
        return "UPDATE";
    default:
        return "UNKNOWN";
    }
}

#ifdef CONFIG_APP_ENABLE_FAULT_HOOK
static void configure_fault_hook_gpio(void)
{
    esp_rom_gpio_pad_select_gpio(APP_FAULT_HOOK_GPIO);
    REG_WRITE(GPIO_ENABLE_W1TC_REG, (1U << APP_FAULT_HOOK_GPIO));
}

static bool fault_hook_triggered(void)
{
    for (int sample = 0; sample < 5; ++sample) {
        if ((REG_READ(GPIO_IN_REG) & (1U << APP_FAULT_HOOK_GPIO)) != 0U) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    return true;
}
#endif

void app_main(void)
{
    uint32_t boot_context = read_boot_context();

    printf("APP_EVT:START\n");
    printf("APP_EVT:BOOTLOADER_HANDOFF_OK\n");
    printf("APP_EVT:BOOT_CONTEXT:%s\n", boot_context_to_token(boot_context));

#ifdef CONFIG_APP_ENABLE_FAULT_HOOK
    configure_fault_hook_gpio();
    printf("APP_EVT:FAULT_HOOK:ENABLED:GPIO=%u\n", APP_FAULT_HOOK_GPIO);
#else
    printf("APP_EVT:FAULT_HOOK:DISABLED\n");
#endif
    fflush(stdout);

    uint32_t heartbeat = 0U;
    while (1) {
        ++heartbeat;
        printf("APP_EVT:HEARTBEAT:%lu\n", (unsigned long)heartbeat);
        fflush(stdout);

#ifdef CONFIG_APP_ENABLE_FAULT_HOOK
        if (fault_hook_triggered()) {
            printf("APP_EVT:FAULT_HOOK:TRIGGERED:RESTART\n");
            fflush(stdout);
            esp_restart();
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(APP_HEARTBEAT_PERIOD_MS));
    }
}
