#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

void print_system_info()
{
    printf("===========================================\n");
    printf("       INFORMACJE O SYSTEMIE ESP32         \n");
    printf("===========================================\n");

    /* ---------------------------------------------------------
     * 1. Informacje o chipie (Procesor)
     * --------------------------------------------------------- */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("Model:          %s\n", CONFIG_IDF_TARGET); // Np. esp32, esp32s3
    printf("Rdzenie:        %d\n", chip_info.cores);
    printf("Rewizja:        v%d\n", chip_info.revision);

    printf("Funkcje:        %s%s%s%s\n",
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi / " : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT / " : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE / " : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? "802.15.4 (Zigbee/Thread)" : "");

    /* ---------------------------------------------------------
     * 2. Informacje o Flash
     * --------------------------------------------------------- */
    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK)
    {
        printf("Flash Rozmiar:  %" PRIu32 " MB (%" PRIu32 " bajtów)\n",
               flash_size / (1024 * 1024), flash_size);
    }
    else
    {
        printf("Flash:          Błąd odczytu rozmiaru\n");
    }

    /* ---------------------------------------------------------
     * 3. Informacje o RAM (Heap)
     * --------------------------------------------------------- */
    // Całkowita wolna pamięć (Internal + PSRAM jeśli jest dostępna)
    uint32_t free_heap = esp_get_free_heap_size();

    // Pamięć wewnętrzna (SRAM wbudowany w chip)
    uint32_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // Pamięć zewnętrzna (PSRAM / SPIRAM) - jeśli podłączona
    uint32_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    printf("RAM Wolne (All): %" PRIu32 " bajtów\n", free_heap);
    printf("RAM Wewn. (Int): %" PRIu32 " bajtów\n", free_internal);

    if (free_spiram > 0)
    {
        printf("RAM Zewn. (PSRAM): %" PRIu32 " bajtów\n", free_spiram);
    }
    else
    {
        printf("RAM Zewn. (PSRAM): Nie wykryto / Brak\n");
    }

    // Największy ciągły blok pamięci (ważne przy alokacji dużych buforów)
    printf("Największy blok: %zu bajtów (Internal)\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    printf("===========================================\n");
}

void app_main(void)
{
    print_system_info();
}