/*
 * Simple test application for custom bootloader
 * This app just prints a message to confirm the bootloader successfully loaded it
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

void app_main(void)
{
    printf("\n\n*** RAW PRINTF TEST - APP MAIN STARTED ***\n\n");
    fflush(stdout);
    
    printf("========================================\n");
    printf("Application started successfully!\n");
    printf("Custom bootloader loaded this app.\n");
    printf("========================================\n");
    fflush(stdout);
    
    int count = 0;
    while (1) {
        printf("[app_printf] count=%d\n", count);
        printf("App running... count=%d\n", count++);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
