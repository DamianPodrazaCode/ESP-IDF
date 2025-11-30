/*
Żeby przetestować błędne działanie kodu bez mutexsów, czyli nakładanie się uart między taskami trzeba było zrobić własną funkcję drukującą zamiast printf.
Funkcja printf ma wbudowane zabezpieczenie (jak by własnego mutexa), poza tym prędkość wysyłania danych na uart jest szybsze niż 1 ms, a rozdzielczość
rtos przeważnie to właśnie 1 - 10 ms.

MUTEX - to przejęcie zmiennej globalnej, zasobu sprzętowego (uart, pamięć, i2c, spi ...) na własność danego taska, i dopóki go nie zwolni inni nie mają dostępu]
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/semphr.h" // mutex

SemaphoreHandle_t uart_mutex; //zmienna globalna

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
        if (xSemaphoreTake(uart_mutex, portMAX_DELAY) == pdTRUE) // czekamy w nieskończoność aż mutex będzie wolny
        {
            uint64_t tick = xTaskGetTickCount();
            // printf("********************************** to jest tekst z task_print1 ******************************** %llu\n", tick);
            drukuj_bez_printf(tekst1, tick);
            xSemaphoreGive(uart_mutex); // oddajemy mutexa tak żeby inni mogli z niego skorzystasć
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void task_print2(void *pvParameter)
{
    while (1)
    {
        if (xSemaphoreTake(uart_mutex, portMAX_DELAY) == pdTRUE) // czekamy w nieskończoność aż mutex będzie wolny
        {
            uint64_t tick = xTaskGetTickCount();
            // printf("__________________________________ to jest tekst z task_print2 ________________________________ %llu\n", tick);
            drukuj_bez_printf(tekst2, tick);
            xSemaphoreGive(uart_mutex); // oddajemy mutexa tak żeby inni mogli z niego skorzystasć
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    printf("Start app_main !!!!\n");

    uart_mutex = xSemaphoreCreateMutex(); // utworzenie mutexa

    xTaskCreate(task_print1, "Task Print1", 2048, NULL, 1, NULL);
    xTaskCreate(task_print2, "Task Print2", 2048, NULL, 1, NULL);

    printf("End app_main !!!!\n");
}