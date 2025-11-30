#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"

void task_print1(void *pvParameter)
{
    while (1)
    {
        uint64_t tick = xTaskGetTickCount();
        printf("********************************** to jest tekst z task_print1 ******************************** %llu\n", tick);
        vTaskDelay(111 / portTICK_PERIOD_MS);
    }
}

void task_print2(void *pvParameter)
{
    while (1)
    {
        uint64_t tick = xTaskGetTickCount();
        printf("__________________________________ to jest tekst z task_print2 ________________________________ %llu\n", tick);
        vTaskDelay(873 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
        printf("Start app_main !!!!\n");

        xTaskCreatePinnedToCore(task_print1, "Task Print1", 2048, NULL, 5, NULL, 0);
        xTaskCreatePinnedToCore(task_print2, "Task Print2", 2048, NULL, 5, NULL, 0);

        printf("End app_main !!!!\n");
}