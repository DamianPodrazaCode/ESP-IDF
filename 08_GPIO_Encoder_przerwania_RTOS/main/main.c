/*
Encoder przerwania RTOS
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"

#define ENC_A 19
#define ENC_B 18

QueueHandle_t qEncoder;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(qEncoder, &gpio_num, NULL);
}

void task_encoder(void *pvParameter)
{
    static int encoder_counter = 0;
    static int last_encoder_state = 0;
    uint32_t io_num;

    while (1)
    {
        if (xQueueReceive(qEncoder, &io_num, portMAX_DELAY))
        {
            int state_A = gpio_get_level(ENC_A);
            int state_B = gpio_get_level(ENC_B);
            int current_state = (state_A << 1) | state_B;

            if (current_state != last_encoder_state)
            {
                if ((last_encoder_state == 0b00 && current_state == 0b01) ||
                    (last_encoder_state == 0b01 && current_state == 0b11) ||
                    (last_encoder_state == 0b11 && current_state == 0b10) ||
                    (last_encoder_state == 0b10 && current_state == 0b00))
                {
                    encoder_counter++;
                    printf("Enkoder -> Prawo, Licznik: %d\n", encoder_counter);
                }
                else if (
                    (last_encoder_state == 0b00 && current_state == 0b10) ||
                    (last_encoder_state == 0b10 && current_state == 0b11) ||
                    (last_encoder_state == 0b11 && current_state == 0b01) ||
                    (last_encoder_state == 0b01 && current_state == 0b00))
                {
                    encoder_counter--;
                    printf("Enkoder <- Lewo, Licznik: %d\n", encoder_counter);
                }
                last_encoder_state = current_state;
            }
        }
    }
}

void app_main(void)
{
    printf("Start app_main !!!!\n");

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL << ENC_A) | (1ULL << ENC_B));
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);

    qEncoder = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(task_encoder, "Task encoder", 2048, NULL, 5, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENC_A, gpio_isr_handler, (void *)ENC_A);
    gpio_isr_handler_add(ENC_B, gpio_isr_handler, (void *)ENC_B);

    printf("End app_main !!!!\n");
}
