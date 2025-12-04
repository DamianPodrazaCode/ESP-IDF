#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h" // <--- WAŻNE: Biblioteka Timerów
#include "driver/gpio.h"
#include "esp_log.h"

// Definicje pinów (zmień na swoje, jeśli masz inne)
#define STATUS_LED_GPIO 12

// Uchwyty do timerów
TimerHandle_t timer_periodic_handle = NULL;
TimerHandle_t timer_oneshot_handle = NULL;

static const char *TAG = "TIMERS_EX";

// --- CALLBACK 1: Timer Cykliczny (Status LED) ---
// Ta funkcja będzie wołana co 500ms
void status_led_callback(TimerHandle_t xTimer)
{
    // Pobieramy aktualny stan pinu i go odwracamy
    static int state = 0;
    state = !state;
    gpio_set_level(STATUS_LED_GPIO, state);
    
    // Logowanie co jakiś czas (nie za często, żeby nie zapchać UART w Callbacku)
    if (state == 1) {
        // ESP_LOGI w timerze jest ryzykowne (może blokować), ale przy małym natężeniu OK.
        // W systemach krytycznych lepiej unikać printf w timerach.
        ESP_LOGI(TAG, "[Auto-Reload] Cykliczne mignięcie...");
    }
}

// --- CALLBACK 2: Timer Jednorazowy (Wyłączanie Lampy) ---
// Ta funkcja zostanie wywołana TYLKO RAZ po upływie czasu
void turn_off_lamp_callback(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "[One-Shot] Czas minął! Wyłączam główną lampę.");
    
    // Tutaj kod wyłączający przekaźnik/lampę
    // ...
    
    // Timer sam przechodzi w stan uśpienia, nie trzeba go usuwać.
}

void app_main(void)
{
    // Konfiguracja GPIO
    gpio_reset_pin(STATUS_LED_GPIO);
    gpio_set_direction(STATUS_LED_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "Tworzenie timerów...");

    // --- 1. TWORZENIE TIMERA CYKLICZNEGO ---
    // xTimerCreate(Nazwa, Okres, AutoReload?, ID, Callback)
    timer_periodic_handle = xTimerCreate(
        "StatusBlinker",            // Nazwa (dla debugowania)
        pdMS_TO_TICKS(500),         // Okres: 500ms
        pdTRUE,                     // pdTRUE = Auto-Reload (Cykliczny)
        (void *)0,                  // Timer ID (można użyć do identyfikacji, tu 0)
        status_led_callback         // Funkcja do wywołania
    );

    // --- 2. TWORZENIE TIMERA JEDNORAZOWEGO ---
    timer_oneshot_handle = xTimerCreate(
        "LampOffTimer",
        pdMS_TO_TICKS(5000),        // Czas: 5000ms (5 sekund)
        pdFALSE,                    // pdFALSE = One-Shot (Jednorazowy)
        (void *)1,                  // ID 1
        turn_off_lamp_callback      // Funkcja do wywołania
    );

    // --- 3. STARTOWANIE TIMERÓW ---
    // Samo utworzenie nie uruchamia timera! Trzeba go wystartować.
    // 0 na końcu to czas oczekiwania na kolejkę komend (tutaj nie czekamy).
    
    if (timer_periodic_handle != NULL) {
        xTimerStart(timer_periodic_handle, 0);
        ESP_LOGI(TAG, "Uruchomiono timer cykliczny.");
    }

    if (timer_oneshot_handle != NULL) {
        ESP_LOGI(TAG, "Włączam lampę... Zgaśnie za 5 sekund.");
        xTimerStart(timer_oneshot_handle, 0);
    }

    // app_main może się teraz skończyć lub robić co innego.
    // Timery działają w tle (w zadaniu Timer Service Task).
}