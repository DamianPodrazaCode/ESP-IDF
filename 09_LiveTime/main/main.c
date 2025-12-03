/*
Odczyt czasu od początku działania ESP
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_timer.h"

void app_main(void)
{
    while (1)
    {
        uint64_t uptime_us = esp_timer_get_time();
        uint64_t uptime_ms = uptime_us / 1000;
        uint64_t tick =  xTaskGetTickCount();
        printf("time stamp -> %llu[ms] %llu[us] %llu[tick]\n", uptime_ms, uptime_us, tick);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}