#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "ADC_EXAMPLE";

// --- KONFIGURACJA ---
// GPIO 34 to ADC1 Channel 6
#define ADC1_CHAN0 ADC_CHANNEL_6
#define ADC_ATTEN ADC_ATTEN_DB_11 // Pozwala mierzyć wyższe napięcia (blisko 3.3V)

// Uchwyty (Handles)
adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle = NULL;
bool do_calibration = false;

// --- FUNKCJA INICJUJĄCA KALIBRACJĘ --- Curve Fitting
// ESP32 jest fabrycznie "krzywy". Każdy chip jest inny.
// Ta funkcja ładuje fabryczne dane korekcyjne z pamięci eFuse.
// static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
// {
//     adc_cali_handle_t handle = NULL;
//     esp_err_t ret = ESP_FAIL;
//     bool calibrated = false;

//     // Próbujemy użyć schematu Curve Fitting (najdokładniejszy dla ESP32)
//     ESP_LOGI(TAG, "Próba kalibracji...");

//     adc_cali_curve_fitting_config_t cali_config = {
//         .unit_id = unit,
//         .chan = channel, // W nowszych IDF trzeba podać kanał? Zależy od wersji, tu bezpieczniej.
//         .atten = atten,
//         .bitwidth = ADC_BITWIDTH_DEFAULT,
//     };

//     ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);

//     if (ret == ESP_OK) {
//         calibrated = true;
//         ESP_LOGI(TAG, "Kalibracja udana (Curve Fitting)");
//     } else {
//         // Jeśli Curve Fitting nie działa (stare chipy), można próbować Line Fitting
//         // Ale zazwyczaj Curve Fitting jest dostępny w V3 chipach.
//         ESP_LOGE(TAG, "Kalibracja nieudana. Będziemy używać surowych danych.");
//     }

//     *out_handle = handle;
//     return calibrated;
// }

// --- FUNKCJA INICJUJĄCA KALIBRACJĘ --- Line Fitting
// Dla klasycznego ESP32 najbezpieczniejszą i najbardziej kompatybilną metodą jest Line Fitting. 
// Curve Fitting jest dostępne tylko w nowszych wersjach krzemu (V3) i wymaga specyficznej konfiguracji.
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    ESP_LOGI(TAG, "Próba kalibracji (Line Fitting)...");

    // --- ZMIANA NA LINE FITTING ---
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .default_vref = 1100, // Domyślne napięcie referencyjne dla ESP32 (ok. 1100 mV)
    };

    // Używamy funkcji create_scheme_LINE_fitting zamiast CURVE
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);

    if (ret == ESP_OK)
    {
        calibrated = true;
        ESP_LOGI(TAG, "Kalibracja udana (Line Fitting)");
    }
    else
    {
        ESP_LOGE(TAG, "Kalibracja nieudana (Kod błędu: %s)", esp_err_to_name(ret));
    }

    *out_handle = handle;
    return calibrated;
}

void app_main(void)
{
    // 1. Inicjalizacja Jednostki ADC1
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1, // Używamy bezpiecznego ADC1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2. Konfiguracja Kanału (GPIO 34)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // Domyślnie 12 bitów (0-4095)
        .atten = ADC_ATTEN,               // Tłumienie 11dB (szeroki zakres)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN0, &config));

    // 3. Inicjalizacja Kalibracji (Opcjonalna, ale zalecana dla woltów)
    do_calibration = adc_calibration_init(ADC_UNIT_1, ADC1_CHAN0, ADC_ATTEN, &adc1_cali_handle);

    // --- PĘTLA GŁÓWNA ---
    while (1)
    {
        int adc_raw = 0;
        int voltage = 0;

        // A. Odczyt surowy (0 - 4095)
        // To jest szybkie i proste.
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN0, &adc_raw));

        ESP_LOGI(TAG, "Raw Data: %d", adc_raw);

        // B. Konwersja na Wolty (jeśli skalibrowano)
        if (do_calibration)
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
            ESP_LOGI(TAG, "Voltage: %d mV", voltage);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // (Opcjonalnie) Sprzątanie przy zamykaniu aplikacji
    // adc_oneshot_del_unit(adc1_handle);
    // if (do_calibration) adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
}