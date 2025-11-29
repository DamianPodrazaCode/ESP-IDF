#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "UART_EVENT";

// Używamy domyślnego portu UART0 (tego od logów/USB)
#define EX_UART_NUM UART_NUM_0
// Bufor dla sterownika (musi być dość duży)
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

// Uchwyt do kolejki, którą utworzy nam sterownik
static QueueHandle_t uart0_queue;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE); // Tymczasowy bufor na dane

    for(;;) {
        // Czekamy na wiadomość od sterownika UART (Queue)
        if(xQueueReceive(uart0_queue, (void * )&event, (TickType_t)portMAX_DELAY)) {
            
            bzero(dtmp, RD_BUF_SIZE); // Czyścimy bufor

            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);

            switch(event.type) {
                // --- ZDARZENIE: PRZYSZŁY DANE ---
                case UART_DATA:
                    ESP_LOGI(TAG, "[UART DATA]: %d bajtów", event.size);
                    
                    // Odczytujemy dokładnie tyle danych, ile zgłosił sterownik
                    uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    
                    // Wypisz co odebrano
                    ESP_LOGI(TAG, "[DATA EVT]: %s", (char*)dtmp);
                    
                    // Odsyłamy z powrotem (Echo)
                    uart_write_bytes(EX_UART_NUM, (const char*) dtmp, event.size);
                    break;

                // --- ZDARZENIE: PRZEPEŁNIENIE SPRZĘTOWE ---
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    // Trzeba wyczyścić bufor sprzętowy
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;

                // --- ZDARZENIE: BUFOR PROGRAMOWY PEŁNY ---
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;

                // --- INNE ZDARZENIA ---
                default:
                    ESP_LOGI(TAG, "type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    vTaskDelete(NULL);
}

void app_main(void)
{
    // 1. Konfiguracja parametrów UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 2. Aplikowanie konfiguracji
    uart_param_config(EX_UART_NUM, &uart_config);

    // 3. Ustawienie pinów (Dla UART0 używamy domyślnych, więc wstawiamy macro UART_PIN_NO_CHANGE)
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 4. Instalacja sterownika + MAGICZNA KOLEJKA
    // Parametry: (Port, RxBuffSize, TxBuffSize, QueueSize, *QueueHandle, IntrFlags)
    // To tutaj ESP-IDF tworzy kolejkę za Ciebie!
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);

    // 5. Uruchomienie naszego zadania do obsługi zdarzeń
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
    
    ESP_LOGI(TAG, "System gotowy. Wpisz coś w terminalu!");
}