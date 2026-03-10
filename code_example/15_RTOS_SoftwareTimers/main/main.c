#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h" // <--- Biblioteka Timerów
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TIMER_EXAMPLE";

// Definicje
#define STATUS_LED_GPIO 12
#define TIMER_PERIOD_MS 500
#define LIGHT_TIMEOUT_MS 5000

// Uchwyty do timerów
TimerHandle_t xStatusTimer = NULL;
TimerHandle_t xLightTimer = NULL;

// Stan diody statusu (do migania)
static int status_led_state = 0;

// --- CALLBACK 1: Timer Cykliczny (Status LED) ---
// Ta funkcja będzie wołana co 500ms
void vStatusTimerCallback(TimerHandle_t xTimer)
{
    // Zmieniamy stan diody (toggle)
    status_led_state = !status_led_state;
    // W prawdziwym układzie tu byłoby: gpio_set_level(STATUS_LED_GPIO, status_led_state);
    
    // Używamy ESP_LOGI, bo jest szybki. Unikaj długich printf!
    // ESP_LOGI(TAG, "Status LED: %d", status_led_state); 
    // (Zakomentowałem, żeby nie zaśmiecać logów, odkomentuj jeśli chcesz widzieć)
}

// --- CALLBACK 2: Timer Jednorazowy (Gaszenie światła) ---
// Ta funkcja zostanie wywołana RAZ, 5 sekund po uruchomieniu timera
void vLightOffCallback(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, ">>> Czas minął! GASZĘ światło główne. <<<");
    // Tutaj normalnie: gpio_set_level(RELAY_PIN, 0);
}

// --- Symulacja czujnika ruchu ---
void simulate_motion_sensor()
{
    ESP_LOGI(TAG, "Wykryto ruch! ZAPALAM światło główne.");
    
    // Jeśli timer już działał (ktoś machał ręką wcześniej), 
    // to xTimerReset zresetuje odliczanie z powrotem do 5 sekund.
    // Jeśli nie działał - uruchomi go.
    
    // Argument 2 (xTicksToWait): 0 oznacza "nie czekaj, jeśli kolejka komend jest pełna"
    if(xTimerStart(xLightTimer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Nie udało się uruchomić timera światła!");
    }
}

void app_main(void)
{
    // 1. Tworzenie Timera Cyklicznego (Auto-reload)
    // Parametry: Nazwa, Okres (ticki), AutoReload?, ID, Callback
    xStatusTimer = xTimerCreate(
        "StatusTimer",              // Nazwa debugowa
        pdMS_TO_TICKS(TIMER_PERIOD_MS), // Czas: 500ms
        pdTRUE,                     // pdTRUE = Auto-reload (Cykliczny)
        (void *)0,                  // Timer ID (opcjonalne, tu 0)
        vStatusTimerCallback        // Funkcja do wywołania
    );

    // 2. Tworzenie Timera Jednorazowego (One-shot)
    xLightTimer = xTimerCreate(
        "LightTimer",
        pdMS_TO_TICKS(LIGHT_TIMEOUT_MS), // Czas: 5000ms
        pdFALSE,                    // pdFALSE = One-shot (Jednorazowy)
        (void *)1,                  // Timer ID
        vLightOffCallback           // Funkcja do wywołania
    );

    // 3. Sprawdzenie i start
    if (xStatusTimer != NULL && xLightTimer != NULL) {
        
        // Startujemy timer statusu od razu
        ESP_LOGI(TAG, "Startuję timer statusu (miganie)...");
        xTimerStart(xStatusTimer, 0);

    } else {
        ESP_LOGE(TAG, "Błąd tworzenia timerów!");
        return;
    }

    // 4. Pętla główna symulująca zdarzenia
    int seconds = 0;
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        seconds++;
        printf("Mija sekunda: %d\n", seconds);

        // Symulacja: W 3. sekundzie wykryto ruch
        if (seconds == 3) {
            simulate_motion_sensor();
        }

        // Symulacja: W 15. sekundzie znowu ruch
        if (seconds == 15) {
            simulate_motion_sensor();
        }
    }
}