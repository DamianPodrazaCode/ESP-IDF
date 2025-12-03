/*
Przycisk i led, FreeRTOS z debouncing i queue.
Przycisk z prostym debouncing pisze na uart0 stan zmiennej,
i komunikuje się taskiem led poprzez kolejkę żeby zmienić stan led
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"

#define BUTTON_GPIO 4
#define LED_GPIO 12

QueueHandle_t qButtonToLed;

void task_led(void *pvParameter)
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << LED_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    uint32_t received_command;

    while (1)
    {
        // portMAX_DELAY oznacza: "Śpij tak długo, aż coś wpadnie. Nawet rok."
        // Procesor w tym czasie w ogóle tu nie zagląda!
        if (xQueueReceive(qButtonToLed, &received_command, portMAX_DELAY))
        {
            printf("Odebrano rozkaz: %lu\n", received_command);
            gpio_set_level(LED_GPIO, received_command);
        }
    }
}

void task_button(void *pvParameter)
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    uint32_t sw_button = 0;
    gpio_set_level(LED_GPIO, sw_button);

    while (1)
    {
        if (!(gpio_get_level(BUTTON_GPIO))) // jeżeli zwarto do masy
        {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            if (!(gpio_get_level(BUTTON_GPIO))) // jeżeli po 50ms dalej zwarte
            {
                sw_button = !sw_button;
                int64_t uptime_us = esp_timer_get_time();
                int64_t uptime_ms = uptime_us / 1000;
                printf("push down %lu - time stamp -> %llu\n", sw_button, uptime_ms);
                // Wyślij wiadomość do kolejki, 0 to czas oczekiwania, jeśli kolejka pełna (nie czekaj)
                xQueueSend(qButtonToLed, &sw_button, 0);
                while (gpio_get_level(BUTTON_GPIO) == 0) //czekaj aż przycisk puszczony
                {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    printf("Start app_main !!!!\n");

    qButtonToLed = xQueueCreate(10, sizeof(int));
    if (qButtonToLed == NULL) {
        printf("Błąd tworzenia kolejki!\n");
        return;
    }

    // xTaskCreate(Funkcja, "Nazwa", RozmiarStosu, Parametry, Priorytet, Uchwyt)
    xTaskCreate(task_led, "Task led", 2048, NULL, 5, NULL);
    xTaskCreate(task_button, "Task button", 2048, NULL, 5, NULL);
    printf("End app_main !!!!\n");
}