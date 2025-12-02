/*
Powiadomienia Zadań pozwalają na komunikację bezpośrednią - Zadanie A -> (szturcha) -> Zadanie B
*/

#include <stdio.h>
#include <stdlib.h> // do rand()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Uchwyt do zadania odbiorcy (musimy wiedzieć, kogo "szturchnąć")
static TaskHandle_t s_receiver_handle = NULL;

// --- ZADANIE 1: NADAWCA (Sensor) ---
void task_sender(void *pvParam)
{
    while(1) {
        // 1. Symulujemy odczyt z czujnika (losowa liczba 20-30)
        uint32_t temperature = 20 + (rand() % 10);
        
        printf("[Sender] Wysyłam temperaturę: %lu\n", temperature);

        // 2. WYSYŁAMY POWIADOMIENIE
        // xTaskNotify(
        //    uchwyt_adresata, 
        //    wartość, 
        //    akcja
        // )
        
        // eSetValueWithOverwrite: Nadpisz starą wartość w "kieszeni" nową.
        // Działa jak jednoelementowa kolejka, która zawsze przyjmuje najnowsze dane.
        xTaskNotify(s_receiver_handle, temperature, eSetValueWithOverwrite);

        // Symulacja pracy co 2 sekundy
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

// --- ZADANIE 2: ODBIORCA (Display) ---
void task_receiver(void *pvParam)
{
    uint32_t received_value;

    printf("[Receiver] Czekam na dane...\n");

    while(1) {
        // 3. ODBIERAMY POWIADOMIENIE
        // xTaskNotifyWait(
        //    bity_do_wyczyszczenia_na_wejsciu,
        //    bity_do_wyczyszczenia_na_wyjsciu,
        //    wskaźnik_na_zmienną_do_zapisu,
        //    czas_oczekiwania
        // )
        
        // 0x00, 0xFFFFFFFF -> Nie czyść nic przed sprawdzeniem, wyczyść wszystko po odczycie.
        // To klasyczna konfiguracja do odczytu wartości liczbowej.
        BaseType_t result = xTaskNotifyWait(0x00, 0xFFFFFFFF, &received_value, portMAX_DELAY);

        if (result == pdTRUE) {
            printf("[Receiver] Odebrano! Temperatura: %lu 'C\n", received_value);
            
            // Reakcja na wysoką temperaturę
            if (received_value > 28) {
                printf("[Receiver] ALARM! Za gorąco!\n");
            }
        }
    }
}

void app_main(void)
{
    // 1. Najpierw tworzymy Odbiorcę, żeby mieć jego uchwyt!
    // Przekazujemy adres zmiennej s_receiver_handle, aby xTaskCreate wpisało tam ID.
    xTaskCreate(task_receiver, "Receiver", 2048, NULL, 5, &s_receiver_handle);

    // 2. Tworzymy Nadawcę
    xTaskCreate(task_sender, "Sender", 2048, NULL, 5, NULL);
}