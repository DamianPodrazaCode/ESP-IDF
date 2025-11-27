#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h" 

#define BUTTON_GPIO 4

void app_main(void)
{

    gpio_config_t io_conf = {}; 
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    int counter = 0;

    while (1)
    {
        if (!(gpio_get_level(BUTTON_GPIO)))
        {
            counter++;
            printf("licznik %d \n", counter);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); //pozwól żyć innym taskom
    }
}