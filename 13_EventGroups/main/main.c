#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h" // <--- WAŻNE: Biblioteka Event Groups

// Definicje bitów (flag)
// Przesuwamy jedynkę o odpowiednią liczbę miejsc
#define BIT_WIFI_CONNECTED  (1 << 0) // ...0001 (Dec: 1)
#define BIT_TIME_SYNCED     (1 << 1) // ...0010 (Dec: 2)

// Uchwyt do Grupy Zdarzeń
EventGroupHandle_t s_init_event_group;

// --- ZADANIE 1: Symulacja łączenia z Wi-Fi ---
void task_wifi_connect(void *pvParam)
{
    printf("[WiFi] Rozpoczynam łączenie...\n");
    
    // Symulacja czasu łączenia (np. 3 sekundy)
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    printf("[WiFi] Połączono! Ustawiam bit BIT_WIFI_CONNECTED.\n");
    
    // Ustawiamy bit 0 na wysoki (1)
    xEventGroupSetBits(s_init_event_group, BIT_WIFI_CONNECTED);
    
    // Zadanie może się zakończyć (lub pracować dalej)
    vTaskDelete(NULL);
}

// --- ZADANIE 2: Symulacja synchronizacji czasu (NTP) ---
void task_time_sync(void *pvParam)
{
    printf("[Time] Czekam na serwer NTP...\n");
    
    // To trwa dłużej, np. 5 sekund
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    
    printf("[Time] Czas zsynchronizowany! Ustawiam bit BIT_TIME_SYNCED.\n");
    
    // Ustawiamy bit 1 na wysoki (1)
    xEventGroupSetBits(s_init_event_group, BIT_TIME_SYNCED);
    
    vTaskDelete(NULL);
}

// --- ZADANIE GŁÓWNE (Kierownik) ---
void task_main_app(void *pvParam)
{
    printf("[Main] Czekam na inicjalizację systemów...\n");

    // Definiujemy, na jakie bity czekamy (suma logiczna OR)
    const EventBits_t bits_to_wait_for = (BIT_WIFI_CONNECTED | BIT_TIME_SYNCED);

    EventBits_t result_bits;

    // --- KLUCZOWA FUNKCJA: xEventGroupWaitBits ---
    // 1. Uchwyt grupy
    // 2. Na jakie bity czekamy? (WIFI oraz TIME)
    // 3. Czy wyczyścić te bity po wyjściu? (pdTRUE = tak, resetujemy flagi)
    // 4. Czy czekać na WSZYSTKIE bity? (pdTRUE = AND, pdFALSE = OR)
    // 5. Ile czasu czekać? (portMAX_DELAY = w nieskończoność)
    
    result_bits = xEventGroupWaitBits(
                    s_init_event_group, 
                    bits_to_wait_for, 
                    pdTRUE,   // Clear on exit
                    pdTRUE,   // Wait for ALL (AND logic)
                    portMAX_DELAY
                  );

    // Skoro przeszliśmy dalej, to znaczy, że oba warunki są spełnione!
    
    printf("[Main] Wszyskie systemy gotowe! (Bity: %lu)\n", result_bits);
    printf("[Main] Startuję główną pętlę programu.\n");

    while(1) {
        // Tu właściwa praca urządzenia
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // 1. Tworzenie grupy zdarzeń
    s_init_event_group = xEventGroupCreate();

    // 2. Uruchamiamy zadania w dowolnej kolejności
    xTaskCreate(task_main_app, "MainApp", 2048, NULL, 5, NULL);
    xTaskCreate(task_wifi_connect, "WifiTask", 2048, NULL, 5, NULL);
    xTaskCreate(task_time_sync, "TimeTask", 2048, NULL, 5, NULL);
}