/*
Żeby przetestować błędnie działanie kodu bez mutexsów, czyli nakładanie się uart między taskami trzeba było zrobić własną funkcję drukującą zamiast printf.
Funkcja printf ma wbudowane zabezpieczenie (jak by własnego mutexa), poza tym prędkość wysyłania danych na uart jest szybszy niż 1 ms, a rozdzielczość 
rtos przeważnie to właśnie 1 - 10 ms.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"

const char tekst1[] = "********************************** to jest tekst z task_print1 ********************************";
const char tekst2[] = "__________________________________ to jest tekst z task_print2 ________________________________";

void drukuj_bez_printf(const char *tekst, uint64_t tick)
{
    char bufor[128];
    snprintf(bufor, sizeof(bufor), "%s %llu\n", tekst, tick);
    for (int i = 0; i < strlen(bufor); i++)
    {
        putchar(bufor[i]);
        for (int k = 0; k < 10000; k++)
        {
            __asm__("nop");
        }
    }
}

void task_print1(void *pvParameter)
{
    while (1)
    {
        uint64_t tick = xTaskGetTickCount();
        // printf("********************************** to jest tekst z task_print1 ******************************** %llu\n", tick);
        drukuj_bez_printf(tekst1, tick);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void task_print2(void *pvParameter)
{
    while (1)
    {
        uint64_t tick = xTaskGetTickCount();
        // printf("__________________________________ to jest tekst z task_print2 ________________________________ %llu\n", tick);
        drukuj_bez_printf(tekst2, tick);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    printf("Start app_main !!!!\n");

    xTaskCreate(task_print1, "Task Print1", 2048, NULL, 1, NULL);
    xTaskCreate(task_print2, "Task Print2", 2048, NULL, 1, NULL);

    printf("End app_main !!!!\n");
}