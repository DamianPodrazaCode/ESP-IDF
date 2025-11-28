/*
Przycisk i led metodą pooling z debouncing.
Przyciśnięcie przycisku powoduje zmianę stanu na led.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#define BUTTON_GPIO 4
#define LED_GPIO 12

void app_main(void)
{
    printf("Start app_main !!!!\n");

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << LED_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
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
                gpio_set_level(LED_GPIO, sw_button);
                sw_button = !sw_button;
                printf("push down %d\n", (int)sw_button);
                while (gpio_get_level(BUTTON_GPIO) == 0)
                {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // pozwól żyć innym taskom
    }
}