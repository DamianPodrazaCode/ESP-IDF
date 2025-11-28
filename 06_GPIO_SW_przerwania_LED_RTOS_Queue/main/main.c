/*
Przycisk z przerwania do kolejki.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"

#define BUTTON_GPIO 4

QueueHandle_t qButtonToLed;

void task_led(void *pvParameter)
{
    uint32_t received_command;
    uint32_t counter = 0;
    uint32_t last_button_press_time = 0;
    const uint32_t debounce_ticks_delay = 200;
    uint32_t current_time = 0;

    while (1)
    {
        if (xQueueReceive(qButtonToLed, &received_command, portMAX_DELAY)) // odbiór z kolejki jeżeli coś jest
        {
            counter++; // licznik ilości przerwań włącznie z drganiami styków
            current_time = xTaskGetTickCount();
            if ((current_time - last_button_press_time) > debounce_ticks_delay) // jeżeli czas mniejszy niż drganie to pomiń
            {
                vTaskDelay(50 / portTICK_PERIOD_MS); //zabezpieczenie puszczenia przycisku gdzie też wystąpi drganie i czasem załączy przewrwanie
                if (!gpio_get_level(BUTTON_GPIO)) // ^^^^^
                {
                    printf("Odebrano rozkaz: %lu %lu %lu %lu\n", received_command, counter, current_time, last_button_press_time);
                    last_button_press_time = current_time;
                }
            }
        }
    }
}

// przerwanie
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(qButtonToLed, &gpio_num, NULL);
}

void app_main(void)
{
    printf("Start app_main !!!!\n");

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&io_conf);

    qButtonToLed = xQueueCreate(10, sizeof(int));

    // xTaskCreate(Funkcja, "Nazwa", RozmiarStosu, Parametry, Priorytet, Uchwyt)
    xTaskCreate(task_led, "Task led", 2048, NULL, 5, NULL);

    // Zainstaluj globalny serwis ISR dla GPIO
    gpio_install_isr_service(0); // 0 = domyślne flagi
    // Dodaj procedury obsługi pinu
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);

    printf("End app_main !!!!\n");
}