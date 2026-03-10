/*
Grupa zdarzeń to jakby połaczenie kilku semaforów binarnych,
ale muszą sie wykonać wszystkie naraz(AND) lub tylko jeden(OR), w zalezności od ustawień.
Event Groups ma 24bity do flag sterujacych.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h" // <--- WAŻNE: Biblioteka Event Groups

// Definicje bitów (flag) dostępne 24 bity
#define BIT_1 (1 << 0)
#define BIT_2 (1 << 1)
#define BIT_3 (1 << 2)

EventGroupHandle_t s_init_event_group;

void task_1(void *pvParam)
{
    printf("Task1 start waiting 3 sec.\n");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    printf("Task1 ready!!!\n");
    xEventGroupSetBits(s_init_event_group, BIT_1); // Ustawiamy bit 0 na wysoki (1)
    vTaskDelete(NULL);                             // Zadanie może się zakończyć (lub pracować dalej)
}

void task_2(void *pvParam)
{
    printf("Task2 start waiting 5 sec.\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Task2 ready!!!\n");
    xEventGroupSetBits(s_init_event_group, BIT_2); // Ustawiamy bit 0 na wysoki (1)
    vTaskDelete(NULL);                             // Zadanie może się zakończyć (lub pracować dalej)
}

void task_3(void *pvParam)
{
    printf("Task3 start waiting 2 sec.\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("Task3 ready!!!\n");
    xEventGroupSetBits(s_init_event_group, BIT_3); // Ustawiamy bit 0 na wysoki (1)
    vTaskDelete(NULL);                             // Zadanie może się zakończyć (lub pracować dalej)
}

void task_main_app(void *pvParam)
{
    printf("Start task_main_app!!!\n");

    const EventBits_t bits_to_wait_for = (BIT_1 | BIT_2 | BIT_3); // Definiujemy, na jakie bity czekamy (suma logiczna OR)

    EventBits_t result_bits;

    result_bits = xEventGroupWaitBits(
        s_init_event_group, // Uchwyt grupy
        bits_to_wait_for,   // Na jakie bity czekamy?
        pdTRUE,             // Czy wyczyścić te bity po wyjściu? (pdTRUE = tak, resetujemy flagi)
        pdTRUE,             // Czy czekać na WSZYSTKIE bity? (pdTRUE = AND, pdFALSE = OR)
        portMAX_DELAY);     // Ile czasu czekać? (portMAX_DELAY = w nieskończoność)

    // Skoro przeszliśmy dalej, to znaczy, że oba warunki są spełnione!

    printf("All flags ON!!!!\n");

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // 1. Tworzenie grupy zdarzeń
    s_init_event_group = xEventGroupCreate();

    // 2. Uruchamiamy zadania w dowolnej kolejności
    xTaskCreate(task_main_app, "MainApp", 2048, NULL, 5, NULL);
    xTaskCreate(task_1, "Task 1", 2048, NULL, 5, NULL);
    xTaskCreate(task_2, "Task 1", 2048, NULL, 5, NULL);
    xTaskCreate(task_3, "Task 1", 2048, NULL, 5, NULL);
}