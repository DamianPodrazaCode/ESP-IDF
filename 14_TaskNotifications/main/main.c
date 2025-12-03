/*
Powiadomienia Zadań pozwalają na komunikację bezpośrednią - Zadanie A -> (szturcha) -> Zadanie B
*/

#include <stdio.h>
#include <stdlib.h> // do rand()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static TaskHandle_t s_receiver_handle = NULL; // Uchwyt do zadania odbiorcy

void task_sender(void *pvParam)
{
    while (1)
    {
        // 1. Symulujemy odczyt z czujnika (losowa liczba 20-30)
        uint32_t temperature = 20 + (rand() % 10);
        printf("[Sender] Wysyłam temperaturę: %lu\n", temperature);

        // xTaskNotify(uchwyt_adresata, wartość, akcja);
        xTaskNotify(s_receiver_handle, temperature, eSetValueWithOverwrite); // eSetValueWithOverwrite: Nadpisz starą wartość, nową.

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void task_receiver(void *pvParam)
{
    uint32_t received_value;
    printf("[Receiver] Czekam na dane...\n");

    while (1)
    {
        // xTaskNotifyWait(bity_do_wyczyszczenia_na_wejsciu, bity_do_wyczyszczenia_na_wyjsciu, wskaźnik_na_zmienną_do_zapisu, czas_oczekiwania);
        BaseType_t result = xTaskNotifyWait(0x00, 0xFFFFFFFF, &received_value, portMAX_DELAY); // 0x00, 0xFFFFFFFF -> Nie czyść nic przed sprawdzeniem, wyczyść wszystko po odczycie.

        if (result == pdTRUE)
        {
            printf("[Receiver] Odebrano! Temperatura: %lu 'C\n", received_value);
        }
    }
}

void app_main(void)
{
    xTaskCreate(task_receiver, "Receiver", 2048, NULL, 5, &s_receiver_handle); // odbiorca
    xTaskCreate(task_sender, "Sender", 2048, NULL, 5, NULL); // nadawca
}