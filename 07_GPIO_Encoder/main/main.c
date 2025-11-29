/*
Encoder bez przerwań z kolejką pomiędzy taskami.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <math.h>

#define ENC_A 19
#define ENC_B 18

QueueHandle_t qEncoder;

// -- śerdnia ruchoma
const int sizeAVR = 4;

int addAVR(int *pTabAVR, uint32_t data)
{
    static int pAVR;
    static int outAVR;
    if (pAVR >= sizeAVR)
    {
        pAVR = 0;
        int tempAVR = 0;
        for (int i = 0; i < sizeAVR; i++)
        {
            tempAVR += *(pTabAVR + i);
        }
        outAVR = tempAVR / sizeAVR;
    }
    pTabAVR[pAVR++] = data;
    return outAVR;
}

void task_print(void *pvParameter)
{
    int received_command;
    int tempLastCount = 0;
    int tabAVR[sizeAVR] = {};

    while (1)
    {
        if (xQueueReceive(qEncoder, &received_command, portMAX_DELAY))
        {
            int outAVR = addAVR(tabAVR, received_command) / 4;
            if (tempLastCount != outAVR)
            {
                printf("Counter: %d\n", outAVR);
                tempLastCount = outAVR;
            }
        }
    }
}

void task_encoder(void *pvParameter)
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL << ENC_A) | (1ULL << ENC_B));
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    static int encoder_counter = 0;
    static int last_encoder_state = 0;

    while (1)
    {
        volatile int state_A = gpio_get_level(ENC_A);
        volatile int state_B = gpio_get_level(ENC_B);
        int current_state = (state_A << 1) | state_B;

        if (current_state != last_encoder_state)
        {
            if ((gpio_get_level(ENC_A) == state_A) && (gpio_get_level(ENC_B) == state_B))
            {
                if ((last_encoder_state == 0b00 && current_state == 0b01) ||
                    (last_encoder_state == 0b01 && current_state == 0b11) ||
                    (last_encoder_state == 0b11 && current_state == 0b10) ||
                    (last_encoder_state == 0b10 && current_state == 0b00))
                {
                    encoder_counter++;
                    xQueueSend(qEncoder, &encoder_counter, 0);
                }

                else if (
                    (last_encoder_state == 0b00 && current_state == 0b10) ||
                    (last_encoder_state == 0b10 && current_state == 0b11) ||
                    (last_encoder_state == 0b11 && current_state == 0b01) ||
                    (last_encoder_state == 0b01 && current_state == 0b00))
                {
                    encoder_counter--;
                    xQueueSend(qEncoder, &encoder_counter, 0);
                }

                last_encoder_state = current_state;
            }
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    printf("Start app_main !!!!\n");

    qEncoder = xQueueCreate(10, sizeof(int));

    xTaskCreate(task_print, "Task print", 2048, NULL, 5, NULL);
    xTaskCreate(task_encoder, "Task encoder", 2048, NULL, 5, NULL);

    printf("End app_main !!!!\n");
}