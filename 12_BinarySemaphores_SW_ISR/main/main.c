/*
Semafor Binarny (Pistolet startowy): Nie ma właściciela. 
Jedna osoba (np. Przerwanie) może podnieść flagę (Give), a inna osoba (Zadanie) może ją zabrać (Take). 
Służy do synchronizacji ("Startuj!").
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // Biblioteka do Semaforów i Mutexów
#include "driver/gpio.h"

#define BUTTON_GPIO 4

SemaphoreHandle_t my_binary_semaphore = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    // Zmienna pomocnicza, która powie nam, czy obudziliśmy ważne zadanie
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Dajemy semafor (Podnosimy flagę)
    xSemaphoreGiveFromISR(my_binary_semaphore, &xHigherPriorityTaskWoken);

    // Jeśli zadanie czekające na semafor ma wysoki priorytet,
    // to wymuszamy przełączenie kontekstu od razu po wyjściu z tej funkcji.
    // Dzięki temu reakcja będzie natychmiastowa.
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void task_button_handler(void *pvParameter)
{
    printf("Zadanie uruchomione. Czekam na semafor...\n");

    while (1)
    {
        // Czekaj na semafor w nieskończoność (portMAX_DELAY).
        // W tym momencie zadanie idzie SPAĆ (stan BLOCKED).
        // Nie zużywa procesora!
        if (xSemaphoreTake(my_binary_semaphore, portMAX_DELAY) == pdTRUE)
        {
            printf("PRZYCISK NACIŚNIĘTY! (Odebrano semafor)\n");

            // Symulacja długiej pracy (np. wysyłanie WiFi, zapis na SD)
            vTaskDelay(100 / portTICK_PERIOD_MS);

            printf("Zadanie wraca do spania.\n");
        }
    }
}

void app_main(void)
{

    my_binary_semaphore = xSemaphoreCreateBinary();

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Przerwanie na opadające zbocze
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, NULL);

    xTaskCreate(task_button_handler, "BtnTask", 2048, NULL, 10, NULL);

    printf("System gotowy. Naciśnij przycisk.\n");
}