#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

// Używamy domyślnego portu UART0
#define EX_UART_NUM UART_NUM_0
// Bufor dla sterownika (musi być dość duży)
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

static QueueHandle_t uart0_queue; // Uchwyt do kolejki, którą utworzy nam sterownik

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE); // Tymczasowy bufor na dane

    while (1)
    {
        if (xQueueReceive(uart0_queue, &event, portMAX_DELAY))
        {
            bzero(dtmp, RD_BUF_SIZE);
            printf("uart[%d] event:\n", EX_UART_NUM);

            switch (event.type)
            {
            case UART_DATA:
                printf("[UART DATA]: %d bajtów\n", event.size);
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY); // Odczytujemy dokładnie tyle danych, ile zgłosił sterownik

                printf("[DATA EVT]: %s\n", (char *)dtmp);

                uart_write_bytes(EX_UART_NUM, (const char *)dtmp, event.size); // Odsyłamy z powrotem (Echo)
                break;

            case UART_FIFO_OVF: // PRZEPEŁNIENIE SPRZĘTOWE
                printf("hw fifo overflow\n");
                uart_flush_input(EX_UART_NUM); // Wyczyść bufor
                xQueueReset(uart0_queue);
                break;

            case UART_BUFFER_FULL: // BUFOR PROGRAMOWY PEŁNY
                printf("ring buffer full\n");
                uart_flush_input(EX_UART_NUM); // Wyczyść bufor
                xQueueReset(uart0_queue);
                break;

            default: // INNE ZDARZENIA
                printf("type: %d\n", event.type);
                break;
            }
        }
    }
    free(dtmp);
    vTaskDelete(NULL);
}

void app_main(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(EX_UART_NUM, &uart_config);
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // (Port, RxBuffSize, TxBuffSize, QueueSize, *QueueHandle, IntrFlags)
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);

    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
}