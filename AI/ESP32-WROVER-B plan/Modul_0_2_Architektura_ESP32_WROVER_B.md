# Moduł 0.2 — Architektura ESP32-WROVER-B

> **Poziom:** 🟢 Laik · **Czas:** Tydzień 1–2 (druga połowa Fazy 0)  
> **Cel:** Dogłębne zrozumienie architektury ESP32-WROVER-B — rdzenie, pamięć, mapa pamięci, zasilanie, stany uśpienia i pinout NodeMCU-32.

---

## Spis treści

1. [Przegląd ESP32-WROVER-B](#1-przegląd-esp32-wrover-b)
2. [Rdzenie Xtensa LX6 (dual-core)](#2-rdzenie-xtensa-lx6-dual-core)
3. [Architektura pamięci](#3-architektura-pamięci)
4. [Mapa pamięci — IRAM, DRAM, RTC, PSRAM](#4-mapa-pamięci--iram-dram-rtc-psram)
5. [Zasilanie i regulacja napięcia](#5-zasilanie-i-regulacja-napięcia)
6. [Stany uśpienia — Light Sleep i Deep Sleep](#6-stany-uśpienia--light-sleep-i-deep-sleep)
7. [Pinout NodeMCU-32](#7-pinout-nodemcu-32)
8. [Strapping Pins i Bootstrap](#8-strapping-pins-i-bootstrap)
9. [Podsumowanie i dalsze kroki](#9-podsumowanie-i-dalsze-kroki)
10. [Źródła i dokumentacja](#10-źródła-i-dokumentacja)

---

## 1. Przegląd ESP32-WROVER-B

### 1.1 Co to jest ESP32-WROVER-B?

ESP32-WROVER-B to **moduł** (nie sam chip!) produkowany przez Espressif Systems. Zawiera:

| Element | Opis |
|---------|------|
| **Chip ESP32-D0WD-V3** | Główny SoC (System on Chip) z dwoma rdzeniami Xtensa LX6 |
| **Flash SPI** | 4 MB (lub 16 MB) pamięci Flash (zewnętrzna, na PCB modułu) |
| **PSRAM** | 8 MB (64 Mbit) pseudo-statycznej RAM (chip ESP-PSRAM64H) |
| **Antena PCB** | Wbudowana antena WiFi/BT (wzór F-inverted) |
| **Kryształ 40 MHz** | Główny oscylator kwarcowy |
| **Filtrowanie zasilania** | Kondensatory decoupling na PCB modułu |

### 1.2 Hierarchia: Chip → Moduł → Płytka deweloperska

```
┌─────────────────────────────────────────────────────────┐
│  NodeMCU-32 (Płytka deweloperska)                       │
│  ┌───────────────────────────────────────────────────┐  │
│  │  ESP32-WROVER-B (Moduł)                           │  │
│  │  ┌─────────────────────────────────────────────┐  │  │
│  │  │  ESP32-D0WD-V3 (Chip / SoC)                │  │  │
│  │  │  • 2× Xtensa LX6 @ 240 MHz                │  │  │
│  │  │  • 520 KB SRAM                             │  │  │
│  │  │  • WiFi 802.11 b/g/n                       │  │  │
│  │  │  • Bluetooth 4.2 BR/EDR + BLE              │  │  │
│  │  │  • Peryferia: GPIO, SPI, I2C, UART, ADC... │  │  │
│  │  └─────────────────────────────────────────────┘  │  │
│  │  + 4 MB Flash SPI (W25Q32)                        │  │
│  │  + 8 MB PSRAM (ESP-PSRAM64H)                      │  │
│  │  + Antena PCB + kryształ 40 MHz                   │  │
│  └───────────────────────────────────────────────────┘  │
│  + USB-UART (CP2102/CH340)                              │
│  + Regulator napięcia 3.3V (AMS1117)                    │
│  + Przyciski: EN (reset), BOOT (GPIO0)                  │
│  + LED zasilania, złącza pinowe                         │
└─────────────────────────────────────────────────────────┘
```

### 1.3 Specyfikacja techniczna ESP32-WROVER-B

| Parametr | Wartość |
|----------|---------|
| **Procesor** | Xtensa® dual-core 32-bit LX6, do 600 DMIPS |
| **Częstotliwość** | 80 / 160 / 240 MHz (konfigurowalne) |
| **SRAM** | 520 KB (wewnętrzna) |
| **PSRAM** | 8 MB (zewnętrzna, na module) |
| **Flash** | 4 MB (zewnętrzna, na module) |
| **WiFi** | 802.11 b/g/n, 2.4 GHz, do 150 Mbps |
| **Bluetooth** | v4.2 BR/EDR + BLE |
| **GPIO** | 34 piny (nie wszystkie dostępne na NodeMCU-32) |
| **ADC** | 18 kanałów, 12-bit SAR |
| **DAC** | 2 kanały, 8-bit |
| **SPI** | 4 interfejsy (SPI0/1 zajęte przez Flash/PSRAM) |
| **I2C** | 2 interfejsy |
| **UART** | 3 interfejsy |
| **I2S** | 2 interfejsy |
| **PWM (LEDC)** | 16 kanałów |
| **Touch** | 10 kanałów capacitive touch |
| **Zasilanie** | 3.0–3.6 V (typowo 3.3 V) |
| **Temperatura pracy** | -40°C do +85°C |
| **Wymiary modułu** | 18 × 20 × 3.2 mm |

---

## 2. Rdzenie Xtensa LX6 (dual-core)

### 2.1 Architektura procesora

ESP32 posiada **dwa rdzenie** procesora Xtensa LX6, oznaczane jako:

| Rdzeń | Nazwa | Rola w ESP-IDF |
|-------|-------|----------------|
| **Core 0** | **PRO_CPU** (Protocol CPU) | Obsługuje WiFi/BT stack, przerwania systemowe |
| **Core 1** | **APP_CPU** (Application CPU) | Uruchamia `app_main()`, taski aplikacyjne |

```
┌─────────────────────────────────────────────────┐
│                    ESP32 SoC                     │
│                                                  │
│  ┌──────────────┐      ┌──────────────┐         │
│  │   Core 0     │      │   Core 1     │         │
│  │  (PRO_CPU)   │      │  (APP_CPU)   │         │
│  │              │      │              │         │
│  │ WiFi/BT      │      │ app_main()   │         │
│  │ System ISR   │      │ User tasks   │         │
│  │ Tick Timer   │      │              │         │
│  └──────┬───────┘      └──────┬───────┘         │
│         │                     │                  │
│         └─────────┬───────────┘                  │
│                   │                              │
│         ┌─────────▼──────────┐                   │
│         │   Shared Bus       │                   │
│         │ (AHB / APB)        │                   │
│         └─────────┬──────────┘                   │
│                   │                              │
│    ┌──────┬───────┼───────┬──────┐              │
│    ▼      ▼       ▼       ▼      ▼              │
│  SRAM   Flash   PSRAM   GPIO  Peryferia         │
│  520KB  4MB     8MB            SPI,I2C,UART...  │
└─────────────────────────────────────────────────┘
```

### 2.2 Pipeline procesora

Każdy rdzeń Xtensa LX6:
- **Pipeline 7-etapowy**: fetch → decode → execute → ...
- **32 rejestry ogólnego przeznaczenia** (32-bit, window-based register file)
- **Brak FPU** (Floating Point Unit) — operacje zmiennoprzecinkowe emulowane programowo!
- **MAC unit** — wsparcie sprzętowe dla mnożenia z akumulacją (DSP)

> **⚠️ Ważne:** ESP32 **nie ma sprzętowego FPU**! Operacje `float` i `double` są wolne (emulacja programowa). Gdzie to możliwe, używaj arytmetyki stałoprzecinkowej (`int`, `fixed-point`). ESP32-S3 ma FPU — ale to inny chip.

### 2.3 Częstotliwość taktowania

| Częstotliwość CPU | APB Bus Clock | Zastosowanie |
|-------------------|---------------|--------------|
| **80 MHz** | 80 MHz | Niski pobór mocy |
| **160 MHz** | 80 MHz | Zbalansowane |
| **240 MHz** | 80 MHz | Maksymalna wydajność (domyślne) |

Konfiguracja w `menuconfig`:
```
Component config → ESP System Settings → CPU frequency → 240 MHz
```

Lub w `sdkconfig.defaults`:
```ini
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
```

**Dynamic Frequency Scaling (DFS):**

ESP-IDF pozwala na automatyczne skalowanie częstotliwości CPU w zależności od obciążenia:

```c
#include "esp_pm.h"

// Konfiguracja DFS
esp_pm_config_t pm_config = {
    .max_freq_mhz = 240,  // Maks. gdy jest praca do wykonania
    .min_freq_mhz = 80,   // Min. gdy CPU jest idle
    .light_sleep_enable = true  // Opcjonalnie: Light Sleep gdy idle
};
esp_pm_configure(&pm_config);
```

### 2.4 SMP — Symmetric Multi-Processing

ESP-IDF wykorzystuje **FreeRTOS SMP** — oba rdzenie uruchamiają ten sam scheduler. Można:

1. **Tworzyć taski bez przypisania** — scheduler sam decyduje:
```c
// Task może działać na dowolnym rdzeniu
xTaskCreate(my_task, "task1", 4096, NULL, 5, NULL);
```

2. **Przypiąć task do konkretnego rdzenia:**
```c
// Task TYLKO na Core 1 (APP_CPU)
xTaskCreatePinnedToCore(my_task, "task1", 4096, NULL, 5, NULL, 1);

// Task TYLKO na Core 0 (PRO_CPU) — uwaga: dzieli czas z WiFi/BT!
xTaskCreatePinnedToCore(my_task, "task0", 4096, NULL, 5, NULL, 0);

// Task na dowolnym rdzeniu (tskNO_AFFINITY = -1)
xTaskCreatePinnedToCore(my_task, "any", 4096, NULL, 5, NULL, tskNO_AFFINITY);
```

> **💡 Wskazówka:** Domyślnie `app_main()` uruchamia się na **Core 1** (APP_CPU). WiFi/BT stack działa na Core 0. Dla maksymalnej wydajności przypisuj ciężkie obliczenia do Core 1.

### 2.5 Sprawdzanie informacji o CPU w runtime

```c
#include "esp_chip_info.h"
#include "esp_system.h"
#include "soc/soc.h"

void print_chip_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "=== Informacje o chipie ===");
    ESP_LOGI(TAG, "Model: %s",
        chip_info.model == CHIP_ESP32 ? "ESP32" : "inny");
    ESP_LOGI(TAG, "Rdzenie: %d", chip_info.cores);
    ESP_LOGI(TAG, "Rewizja: %d", chip_info.revision);
    ESP_LOGI(TAG, "WiFi: %s",
        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "TAK" : "NIE");
    ESP_LOGI(TAG, "Bluetooth: %s",
        (chip_info.features & CHIP_FEATURE_BT) ? "TAK" : "NIE");
    ESP_LOGI(TAG, "BLE: %s",
        (chip_info.features & CHIP_FEATURE_BLE) ? "TAK" : "NIE");
    ESP_LOGI(TAG, "Flash: %s, %d MB",
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external",
        spi_flash_get_chip_size() / (1024 * 1024));

    // Na którym rdzeniu działa ten kod?
    ESP_LOGI(TAG, "Ten kod działa na Core %d", xPortGetCoreID());
}
```

---

## 3. Architektura pamięci

### 3.1 Podsumowanie pamięci ESP32-WROVER-B

```
┌─────────────────────────────────────────────────────────┐
│                 ESP32-WROVER-B — Pamięć                 │
│                                                          │
│  ┌─ Wewnętrzna (w chipie ESP32) ─────────────────────┐  │
│  │                                                    │  │
│  │  SRAM0 (192 KB) ──► IRAM (instrukcje) + cache     │  │
│  │  SRAM1 (128 KB) ──► DRAM (dane) + IRAM overflow   │  │
│  │  SRAM2 (200 KB) ──► DRAM (dane, heap)             │  │
│  │  ─────────────────────────────────────────         │  │
│  │  Razem: 520 KB SRAM                               │  │
│  │                                                    │  │
│  │  RTC FAST SRAM (8 KB) ──► Kod RTC, deep sleep     │  │
│  │  RTC SLOW SRAM (8 KB) ──► Dane ULP, deep sleep    │  │
│  │                                                    │  │
│  │  ROM (448 KB) ──► Bootloader ROM, biblioteki       │  │
│  │  eFuse (1 KB) ──► MAC, kalibracja, security keys  │  │
│  │                                                    │  │
│  └────────────────────────────────────────────────────┘  │
│                                                          │
│  ┌─ Zewnętrzna (na PCB modułu) ──────────────────────┐  │
│  │                                                    │  │
│  │  Flash SPI (4 MB) ──► Firmware, NVS, SPIFFS       │  │
│  │  PSRAM SPI (8 MB) ──► Duże bufory, frame buffy    │  │
│  │                                                    │  │
│  └────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 3.2 Wewnętrzna SRAM — podział szczegółowy

| Blok | Rozmiar | Adres początkowy | Przeznaczenie |
|------|---------|------------------|---------------|
| **SRAM0** | 192 KB | `0x4007_0000` | IRAM — kod krytyczny czasowo, cache instrukcji |
| **SRAM1** | 128 KB | `0x3FFE_0000` | DRAM (dane) lub IRAM (overflow) |
| **SRAM2** | 200 KB | `0x3FFA_E000` | DRAM — heap, zmienne globalne |
| **RTC FAST** | 8 KB | `0x3FF8_0000` | Przechowywane w deep sleep (dostęp z PRO_CPU) |
| **RTC SLOW** | 8 KB | `0x5000_0000` | Dane ULP co-processor, deep sleep |

> **⚠️ Ważne:** Z 520 KB SRAM nie wszystko jest dostępne dla aplikacji! System ESP-IDF (FreeRTOS, WiFi/BT stack, sterowniki) zajmuje ~50-100 KB. Typowo dostępne jest **~290-350 KB** heap DRAM.

### 3.3 Flash SPI — organizacja

Flash jest podzielona na **partycje** zdefiniowane w tablicy partycji:

```
┌──────────────────────────────────────────────────────┐
│                   4 MB Flash                          │
│                                                       │
│  0x0000 ┌──────────────────────────┐                 │
│         │  Bootloader (16 KB)      │ ← Second-stage  │
│  0x8000 ├──────────────────────────┤                 │
│         │  Partition Table (4 KB)  │                  │
│  0x9000 ├──────────────────────────┤                 │
│         │  NVS (24 KB)            │ ← Non-Volatile   │
│  0xF000 ├──────────────────────────┤    Storage      │
│         │  PHY Init Data (4 KB)   │ ← Kalibracja RF  │
│ 0x10000 ├──────────────────────────┤                 │
│         │                          │                  │
│         │  Application (do ~3.9MB) │ ← Twój firmware  │
│         │                          │                  │
│         └──────────────────────────┘                 │
└──────────────────────────────────────────────────────┘
```

ESP32 **nie wykonuje kodu bezpośrednio z Flash** w tradycyjny sposób. Używa mechanizmu **XIP (Execute In Place)** z cache:

```
Flash SPI ──► Cache (32 KB IRAM) ──► CPU fetch
              ▲
              │ Automatycznie
              │ Cache miss → załaduj stronę z Flash
```

### 3.4 PSRAM — zewnętrzna pamięć RAM

PSRAM (Pseudo-Static RAM) to **8 MB dodatkowej pamięci RAM** dostępnej przez magistralę SPI. Jest wolniejsza niż wewnętrzna SRAM, ale znacznie większa.

| Cecha | Wewnętrzna SRAM | PSRAM |
|-------|-----------------|-------|
| **Rozmiar** | 520 KB | 8 MB (8192 KB) |
| **Prędkość** | 1 cykl CPU | ~10-20 cykli (przez SPI + cache) |
| **DMA** | ✅ Tak | ❌ **Nie bezpośrednio!** |
| **Dostęp z ISR** | ✅ Tak | ⚠️ Ryzykowne (SPI busy) |
| **Cache** | Nie potrzebuje | 32 KB cache wewnętrznej SRAM |

Alokacja pamięci w PSRAM:

```c
#include "esp_heap_caps.h"

// Metoda 1: Jawna alokacja w PSRAM
void *big_buffer = heap_caps_malloc(1024 * 1024, MALLOC_CAP_SPIRAM);
if (big_buffer == NULL) {
    ESP_LOGE(TAG, "Nie udało się zaalokować 1 MB w PSRAM!");
}

// Metoda 2: malloc() automatycznie używa PSRAM (wymaga CONFIG_SPIRAM_USE_MALLOC)
// Gdy braknie SRAM, malloc() sięgnie do PSRAM
void *buffer = malloc(500000);  // 500 KB — trafi do PSRAM

// Metoda 3: calloc z PSRAM
uint16_t *framebuffer = heap_caps_calloc(320 * 240, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
// 320×240×2 = 150 KB frame buffer dla TFT
```

> **⚠️ Ważne o DMA i PSRAM:** Peryferia DMA (SPI, I2S) **nie mogą** bezpośrednio czytać z PSRAM. Musisz skopiować dane do wewnętrznej SRAM przed transferem DMA, lub użyć EDMA (dostępne w ESP32-S3).

---

## 4. Mapa pamięci — IRAM, DRAM, RTC, PSRAM

### 4.1 Pełna mapa adresowa

ESP32 ma przestrzeń adresową 4 GB (32-bit). Kluczowe regiony:

```
Adres                  Region                    Opis
─────────────────────────────────────────────────────────────
0x0000_0000─0x3F3F_FFFF  Dane cache (Flash)     Flash read-only dane (mmap)
0x3F40_0000─0x3F7F_FFFF  Dane cache (PSRAM)     PSRAM memory-mapped
0x3F80_0000─0x3FBF_FFFF  Peryferia dane         Rejestry peryferiów
0x3FFC_0000─0x3FFF_FFFF  DRAM (wewnętrzna)      Zmienne, heap, stos
0x4000_0000─0x400C_1FFF  IRAM (wewnętrzna)      Kod krytyczny czasowo
0x400C_2000─0x40BF_FFFF  Instrukcje cache       Flash XIP (Execute in Place)
0x5000_0000─0x5000_1FFF  RTC SLOW mem           Dane ULP, deep sleep
```

### 4.2 IRAM — Instruction RAM

**IRAM** to pamięć na kod, który **musi być wykonywany szybko** — bez opóźnień cache miss z Flash.

Co trafia do IRAM:
- **Handlery przerwań (ISR)** — oznaczone `IRAM_ATTR`
- **Funkcje krytyczne czasowo** — oznaczone `IRAM_ATTR`
- **Wektory przerwań** — automatycznie
- **Hot functions** — często wywoływane (opcjonalnie)

```c
#include "esp_attr.h"

// Funkcja w IRAM — wykonuje się z RAM, nie z Flash cache
// Konieczne dla ISR i kodu wywoływanego podczas operacji na Flash
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    // Obsługa przerwania — MUSI być krótka!
    // NIE można tu używać ESP_LOGx() ani malloc()!
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Funkcja wywoływana podczas OTA update (Flash jest zajęty!)
void IRAM_ATTR critical_during_flash_update(void)
{
    // Ten kod działa nawet gdy Flash jest programowany
}
```

> **⚠️ Ważne:** IRAM jest ograniczona (~128 KB po odjęciu cache). Nie umieszczaj w niej dużych funkcji! Tylko ISR handlery i naprawdę krytyczne fragmenty.

### 4.3 DRAM — Data RAM

**DRAM** to pamięć na dane — zmienne globalne, heap (malloc), stosy tasków.

```c
// Zmienna w DRAM (domyślnie)
static int counter = 0;

// Zmienne const mogą trafić do Flash (rodata) — oszczędność DRAM
static const uint8_t lookup_table[] = {0, 1, 4, 9, 16, 25};

// Wymuszenie DRAM (np. dla danych używanych w ISR)
static DRAM_ATTR uint32_t isr_counter = 0;

// String w DRAM (dla ISR — Flash string niedostępny z IRAM)
static DRAM_ATTR const char isr_message[] = "ISR triggered";
```

### 4.4 RTC FAST Memory (8 KB)

Pamięć **zachowywana w deep sleep**, dostępna **tylko z PRO_CPU (Core 0)**. Używana do:
- Kodu uruchamianego po wybudzeniu z deep sleep (stub function)
- Szybkich zmiennych RTC

```c
// Zmienna przeżywająca deep sleep
RTC_DATA_ATTR int boot_count = 0;

// Zmienna w RTC FAST dla kodu wybudzenia
RTC_FAST_ATTR uint64_t last_wakeup_time = 0;

void app_main(void)
{
    boot_count++;
    ESP_LOGI(TAG, "Numer uruchomienia: %d", boot_count);
    // boot_count zachowuje wartość po deep sleep!

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Wybudzony z deep sleep przez timer");
    }
}
```

### 4.5 RTC SLOW Memory (8 KB)

Pamięć dostępna z **ULP co-processor** i zachowywana w deep sleep. ULP to ultra-low-power procesor, który działa gdy główne rdzenie są wyłączone.

```c
// Zmienna współdzielona z ULP co-processor
RTC_SLOW_ATTR uint32_t ulp_measurement_count = 0;
```

### 4.6 Profiling pamięci — sprawdzanie dostępnej pamięci

```c
#include "esp_heap_caps.h"
#include "esp_system.h"

void print_memory_info(void)
{
    ESP_LOGI(TAG, "=== Informacje o pamięci ===");

    // Całkowity wolny heap
    ESP_LOGI(TAG, "Free heap: %lu bytes",
        (unsigned long)esp_get_free_heap_size());

    // Minimalny wolny heap od startu (high water mark)
    ESP_LOGI(TAG, "Min free heap ever: %lu bytes",
        (unsigned long)esp_get_minimum_free_heap_size());

    // Szczegóły wewnętrznej SRAM
    ESP_LOGI(TAG, "--- Wewnętrzna SRAM (DRAM) ---");
    ESP_LOGI(TAG, "  Free: %lu bytes",
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "  Largest block: %lu bytes",
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    // Szczegóły PSRAM (jeśli dostępna)
    ESP_LOGI(TAG, "--- PSRAM (SPI RAM) ---");
    ESP_LOGI(TAG, "  Free: %lu bytes",
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "  Largest block: %lu bytes",
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

    // Pamięć DMA-capable
    ESP_LOGI(TAG, "--- DMA-capable ---");
    ESP_LOGI(TAG, "  Free: %lu bytes",
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));

    // Pełny raport
    ESP_LOGI(TAG, "--- Pełny raport heap ---");
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
}
```

Typowy output na ESP32-WROVER-B (po starcie, bez WiFi/BT):
```
Free heap: 4388576 bytes        ← ~4.2 MB (wewn. SRAM + PSRAM)
Min free heap ever: 4385024 bytes
--- Wewnętrzna SRAM (DRAM) ---
  Free: 296348 bytes             ← ~290 KB dostępne (z 520 KB)
  Largest block: 131072 bytes    ← największy ciągły blok ~128 KB
--- PSRAM (SPI RAM) ---
  Free: 8388352 bytes            ← ~8 MB PSRAM prawie cała wolna
  Largest block: 8388336 bytes
--- DMA-capable ---
  Free: 264348 bytes             ← ~258 KB (tylko wewn. SRAM!)
```

---

## 5. Zasilanie i regulacja napięcia

### 5.1 Wymagania zasilania ESP32-WROVER-B

| Parametr | Min | Typ | Max |
|----------|-----|-----|-----|
| **Napięcie VDD** | 3.0 V | 3.3 V | 3.6 V |
| **Prąd (Active, WiFi TX)** | — | ~240 mA | 340 mA (peak) |
| **Prąd (Active, CPU only)** | — | ~30-50 mA | — |
| **Prąd (Modem Sleep)** | — | ~20 mA | — |
| **Prąd (Light Sleep)** | — | ~0.8 mA | — |
| **Prąd (Deep Sleep)** | — | ~10 µA | 150 µA |
| **Prąd (Hibernation)** | — | ~5 µA | — |

### 5.2 Zasilanie na NodeMCU-32

```
USB 5V ──► AMS1117-3.3 (LDO regulator) ──► 3.3V (ESP32 + peryferia)
              │
              ▼
          Max ~800 mA output
          (ale USB port daje max ~500 mA!)

Alternatywa: Pin VIN (5V) ──► AMS1117-3.3 ──► 3.3V
             Pin 3V3 ──► bezpośrednio 3.3V (POMIJAJĄC regulator!)
```

> **⚠️ Ważne:** Podłączając zasilanie na pin **3V3** omijasz regulator! Napięcie musi być stabilne 3.3V (±0.3V). Podanie 5V na pin 3V3 **zniszczy ESP32**.

### 5.3 Domeny zasilania wewnątrz ESP32

```
┌──────────────────────────────────────────┐
│          Domeny zasilania ESP32           │
│                                           │
│  VDD3P3_RTC (3.3V) ──► RTC controller    │  ← Zawsze ON w deep sleep
│                    ──► RTC SRAM           │
│                    ──► ULP co-processor   │
│                    ──► RTC GPIO           │
│                                           │
│  VDD3P3_CPU (3.3V) ──► CPU cores         │  ← OFF w deep sleep
│                    ──► SRAM              │
│                    ──► Digital periphs   │
│                                           │
│  VDD_SDIO (1.8/3.3V) ─► Flash SPI       │  ← Konfigurowalne napięcie
│                       ─► PSRAM SPI       │
│                       ─► SDIO piny       │
└──────────────────────────────────────────┘
```

---

## 6. Stany uśpienia — Light Sleep i Deep Sleep

### 6.1 Porównanie stanów uśpienia

| Stan | Pobór prądu | CPU | SRAM | WiFi/BT | RTC | Czas wybudzenia |
|------|-------------|-----|------|---------|-----|-----------------|
| **Active** | ~240 mA (WiFi TX) | ✅ ON | ✅ ON | ✅ ON | ✅ ON | — |
| **Modem Sleep** | ~20 mA | ✅ ON | ✅ ON | ❌ OFF | ✅ ON | natychmiast |
| **Light Sleep** | ~0.8 mA | ⏸️ Paused | ✅ ON | ❌ OFF | ✅ ON | ~1 ms |
| **Deep Sleep** | ~10 µA | ❌ OFF | ❌ OFF* | ❌ OFF | ✅ ON | ~10 ms (reboot) |
| **Hibernation** | ~5 µA | ❌ OFF | ❌ OFF | ❌ OFF | ❌ OFF** | ~10 ms |

*\* Tylko RTC SRAM (8+8 KB) zachowana*  
*\*\* Tylko RTC timer*

### 6.2 Light Sleep

CPU jest **wstrzymany**, ale po wybudzeniu **kontynuuje** od miejsca, gdzie się zatrzymał. Pamięć SRAM jest zachowana.

```c
#include "esp_sleep.h"
#include "esp_wifi.h"

void enter_light_sleep(void)
{
    // Źródło wybudzenia: timer (5 sekund)
    esp_sleep_enable_timer_wakeup(5 * 1000000);  // mikrosekundy!

    // Źródło wybudzenia: GPIO (np. przycisk na GPIO0)
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);

    ESP_LOGI(TAG, "Wchodzę w Light Sleep...");

    // Wejście w Light Sleep — CPU się zatrzymuje
    esp_light_sleep_start();

    // ─── CPU WSTRZYMANY ───
    // ─── Po wybudzeniu kontynuuje TUTAJ ───

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wybudzony z Light Sleep: TIMER");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            ESP_LOGI(TAG, "Wybudzony z Light Sleep: GPIO");
            break;
        default:
            ESP_LOGI(TAG, "Wybudzony z Light Sleep: przyczyna %d", cause);
            break;
    }
}
```

### 6.3 Deep Sleep

CPU jest **wyłączony**. Po wybudzeniu ESP32 wykonuje **pełny reboot** (od bootloadera). Jedynie pamięć RTC (16 KB) jest zachowana.

```c
#include "esp_sleep.h"

// Zmienna przeżywająca deep sleep — musi być w RTC memory!
RTC_DATA_ATTR int deep_sleep_count = 0;
RTC_DATA_ATTR int64_t last_sleep_time = 0;

void enter_deep_sleep(void)
{
    deep_sleep_count++;
    last_sleep_time = esp_timer_get_time();

    ESP_LOGI(TAG, "Deep sleep #%d, idę spać na 10 sekund...", deep_sleep_count);

    // Źródło 1: Timer (10 sekund)
    esp_sleep_enable_timer_wakeup(10 * 1000000);  // 10s w µs

    // Źródło 2: Przycisk na RTC GPIO (np. GPIO 4 = RTC GPIO 10)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 0);  // 0 = LOW level

    // Wyłącz co nie jest potrzebne (oszczędność prądu)
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    // Uwaga: wyłączenie RTC_PERIPH wyłączy pullup/pulldown na RTC GPIO!

    ESP_LOGI(TAG, "Dobranoc!");
    esp_deep_sleep_start();

    // ─── TEN KOD NIGDY SIĘ NIE WYKONA ───
    // ─── Po wybudzeniu ESP32 startuje od app_main() ───
}

void app_main(void)
{
    ESP_LOGI(TAG, "Boot #%d", deep_sleep_count);

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        int64_t sleep_duration = (esp_timer_get_time() - last_sleep_time) / 1000000;
        ESP_LOGI(TAG, "Wybudzony timerem, spałem %lld sekund", sleep_duration);
    } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Wybudzony przyciskiem na GPIO4");
    } else {
        ESP_LOGI(TAG, "Pierwszy boot (power-on reset)");
    }

    // ... reszta aplikacji ...
    vTaskDelay(pdMS_TO_TICKS(5000));
    enter_deep_sleep();
}
```

### 6.4 Źródła wybudzenia — porównanie

| Źródło | Light Sleep | Deep Sleep | Opis |
|--------|:-----------:|:----------:|------|
| **Timer** | ✅ | ✅ | `esp_sleep_enable_timer_wakeup()` |
| **GPIO** | ✅ | ❌ | `esp_sleep_enable_gpio_wakeup()` |
| **EXT0** | ❌ | ✅ | Jeden pin RTC GPIO, level |
| **EXT1** | ❌ | ✅ | Wiele pinów RTC GPIO, maska |
| **Touch Pad** | ✅ | ✅ | `esp_sleep_enable_touchpad_wakeup()` |
| **ULP** | ❌ | ✅ | ULP co-processor |
| **UART** | ✅ | ❌ | `esp_sleep_enable_uart_wakeup()` |

---

## 7. Pinout NodeMCU-32

### 7.1 Mapa pinów NodeMCU-32 z ESP32-WROVER-B

```
                    ┌──────────────────┐
                    │    NodeMCU-32     │
                    │   ESP32-WROVER-B │
                    │                  │
            3V3 ───┤1              38├─── GND
             EN ───┤2    ┌──────┐  37├─── GPIO 23 (VSPI MOSI)
      GPIO 36 /VP ─┤3    │      │  36├─── GPIO 22 (I2C SCL)
      GPIO 39 /VN ─┤4    │ ESP  │  35├─── GPIO  1 (TX0)
        GPIO 34 ───┤5    │  32  │  34├─── GPIO  3 (RX0)
        GPIO 35 ───┤6    │WROVER│  33├─── GPIO 21 (I2C SDA)
        GPIO 32 ───┤7    │  -B  │  32├─── GND
        GPIO 33 ───┤8    │      │  31├─── GPIO 19 (VSPI MISO)
        GPIO 25 ───┤9    └──────┘  30├─── GPIO 18 (VSPI CLK)
        GPIO 26 ───┤10             29├─── GPIO  5 (VSPI CS0) ⚡
        GPIO 27 ───┤11             28├─── GPIO 17 (TX2)
        GPIO 14 ───┤12             27├─── GPIO 16 (RX2)
        GPIO 12 ───┤13  ⚡         26├─── GPIO  4
            GND ───┤14             25├─── GPIO  0  ⚡ BOOT
        GPIO 13 ───┤15             24├─── GPIO  2  ⚡
    (NC) GPIO  9 ──┤16             23├─── GPIO 15  ⚡
   (NC) GPIO 10 ──┤17             22├─── GPIO  8  (NC)
   (NC) GPIO 11 ──┤18             21├─── GPIO  7  (NC)
            VIN ───┤19             20├─── GPIO  6  (NC)
                    └──────────────────┘

⚡ = Strapping pin (wpływa na boot!)
(NC) = Zajęty przez Flash/PSRAM — NIE UŻYWAĆ!
```

### 7.2 Klasyfikacja GPIO

| GPIO | Funkcja domyślna | Tryb | ADC | Touch | RTC | Uwagi |
|------|-------------------|------|-----|-------|-----|-------|
| **0** | BOOT button | I/O | ADC2_1 | T1 | RTC_11 | ⚡ Strapping: LOW=download |
| **1** | UART0 TX | O | — | — | — | USB-UART TX, nie używaj |
| **2** | — | I/O | ADC2_2 | T2 | RTC_12 | ⚡ Strapping: musi=LOW przy boot |
| **3** | UART0 RX | I | — | — | — | USB-UART RX, nie używaj |
| **4** | — | I/O | ADC2_0 | T0 | RTC_10 | Bezpieczny GPIO |
| **5** | VSPI CS0 | I/O | — | — | — | ⚡ Strapping: startup log |
| **6–11** | Flash SPI | — | — | — | — | ❌ ZAJĘTE przez Flash! |
| **12** | — | I/O | ADC2_5 | T5 | RTC_15 | ⚡ Strapping: VDD_SDIO |
| **13** | — | I/O | ADC2_4 | T4 | RTC_14 | Bezpieczny GPIO |
| **14** | HSPI CLK | I/O | ADC2_6 | T6 | RTC_16 | Bezpieczny GPIO |
| **15** | — | I/O | ADC2_3 | T3 | RTC_13 | ⚡ Strapping: silence log |
| **16** | UART2 RX | I/O | — | — | — | ⚠️ Na WROVER używany przez PSRAM! |
| **17** | UART2 TX | I/O | — | — | — | ⚠️ Na WROVER używany przez PSRAM! |
| **18** | VSPI CLK | I/O | — | — | — | Bezpieczny GPIO |
| **19** | VSPI MISO | I/O | — | — | — | Bezpieczny GPIO |
| **21** | I2C SDA | I/O | — | — | — | Bezpieczny GPIO |
| **22** | I2C SCL | I/O | — | — | — | Bezpieczny GPIO |
| **23** | VSPI MOSI | I/O | — | — | — | Bezpieczny GPIO |
| **25** | DAC1 | I/O | ADC2_8 | — | RTC_6 | Bezpieczny GPIO + DAC |
| **26** | DAC2 | I/O | ADC2_9 | — | RTC_7 | Bezpieczny GPIO + DAC |
| **27** | — | I/O | ADC2_7 | T7 | RTC_17 | Bezpieczny GPIO |
| **32** | — | I/O | ADC1_4 | T9 | RTC_9 | Bezpieczny GPIO |
| **33** | — | I/O | ADC1_5 | T8 | RTC_8 | Bezpieczny GPIO |
| **34** | — | **Input only** | ADC1_6 | — | RTC_4 | Brak pull-up/down! |
| **35** | — | **Input only** | ADC1_7 | — | RTC_5 | Brak pull-up/down! |
| **36/VP** | — | **Input only** | ADC1_0 | — | RTC_0 | Brak pull-up/down! |
| **39/VN** | — | **Input only** | ADC1_3 | — | RTC_3 | Brak pull-up/down! |

### 7.3 GPIO — ograniczenia ESP32-WROVER-B

> **⚠️ Krytyczne ograniczenia:**
> 
> 1. **GPIO 6–11:** Zajęte przez wewnętrzny Flash SPI — **NIGDY nie podłączaj!**
> 2. **GPIO 16–17:** Na ESP32-WROVER-B zajęte przez **PSRAM SPI** — nie używaj!
> 3. **GPIO 34, 35, 36, 39:** Tylko **INPUT** — nie mogą być output, nie mają pull-up/down
> 4. **ADC2 (GPIO 0, 2, 4, 12–15, 25–27):** **Nie działa gdy WiFi jest aktywne!** Używaj ADC1

---

## 8. Strapping Pins i Bootstrap

### 8.1 Czym są Strapping Pins?

**Strapping pins** to specjalne piny GPIO, których stan (HIGH/LOW) jest odczytywany **podczas resetu** ESP32 i decyduje o trybie uruchomienia. Po zakończeniu bootstrap piny te mogą być używane normalnie.

### 8.2 Tabela Strapping Pins

| GPIO | Strapping Pin | Stan domyślny | Efekt |
|------|---------------|----------------|-------|
| **GPIO 0** | Tryb boot | Pull-up (HIGH) | **HIGH** = normalny boot z Flash / **LOW** = tryb download (programowanie) |
| **GPIO 2** | Tryb boot | Pull-down (LOW) | Musi być **LOW** lub floating przy boot z Flash |
| **GPIO 5** | SDIO timing | Pull-up (HIGH) | Wpływa na timing SDIO slave |
| **GPIO 12** | VDD_SDIO | Pull-down (LOW) | **LOW** = VDD_SDIO 3.3V / **HIGH** = VDD_SDIO 1.8V |
| **GPIO 15** | Silence log | Pull-up (HIGH) | **LOW** = wycisza logi bootloadera na UART0 |

### 8.3 Sekwencja bootstrap ESP32

```
Zasilanie ON / Reset
       │
       ▼
┌──────────────────┐
│  ROM Bootloader  │  ← Wbudowany w chip, niezmienny (First-Stage)
│  (wewnętrzny ROM)│
└────────┬─────────┘
         │
    Odczyt GPIO 0
         │
    ┌────┴────┐
    │         │
 GPIO0=HIGH  GPIO0=LOW
    │         │
    ▼         ▼
┌────────┐ ┌──────────────┐
│ Normal │ │ Download Mode│
│  Boot  │ │ (UART/JTAG)  │
└───┬────┘ └──────────────┘
    │        Czeka na firmware
    ▼        przez UART (esptool)
┌──────────────────┐
│ Second-Stage     │
│ Bootloader       │  ← Z Flash pod adresem 0x1000
│ (ESP-IDF bootl.) │
└────────┬─────────┘
         │
    Odczyt tablicy partycji
    Weryfikacja firmware
         │
         ▼
┌──────────────────┐
│   Aplikacja      │
│   (app_main)     │  ← Z Flash pod adresem 0x10000
└──────────────────┘
```

### 8.4 Pułapka: GPIO 12 i PSRAM

> **⚠️ KRYTYCZNE dla ESP32-WROVER-B:**
>
> GPIO 12 ustawia napięcie VDD_SDIO (zasilanie Flash i PSRAM):
> - **GPIO 12 = LOW** → VDD_SDIO = **3.3V** ✅ (poprawne dla WROVER-B)
> - **GPIO 12 = HIGH** → VDD_SDIO = **1.8V** ❌ Flash i PSRAM na WROVER-B wymagają 3.3V!
>
> Jeśli GPIO 12 jest HIGH podczas boot, ESP32 się **nie uruchomi** lub będzie niestabilny!
>
> **Rozwiązanie:** Ustaw eFuse, aby wymusić 3.3V niezależnie od GPIO 12:

```bash
# ⚠️ JEDNORAZOWE! eFuse to operacja NIEODWRACALNA!
espefuse.py --port COM3 set_flash_voltage 3.3V
```

Lub w `menuconfig`:
```
Component config → Hardware Settings → GPIO12 → 
    [*] Ignore GPIO12 strapping pin, set VDD_SDIO = 3.3V via eFuse
```

---

## 9. Podsumowanie i dalsze kroki

### 9.1 Kluczowe wnioski

| Temat | Zapamiętaj |
|-------|-----------|
| **Rdzenie** | Dual-core Xtensa LX6: Core 0 (WiFi/BT), Core 1 (aplikacja). Brak FPU! |
| **SRAM** | 520 KB, ale ~290 KB dostępne (reszta: system). Używaj `heap_caps_get_free_size()` |
| **PSRAM** | 8 MB dodatkowej RAM, wolniejsza, **nie obsługuje DMA bezpośrednio** |
| **Flash** | 4 MB z XIP cache. Podzielona na partycje (bootloader, NVS, app) |
| **IRAM_ATTR** | Obowiązkowe dla ISR handlerów i kodu krytycznego czasowo |
| **RTC_DATA_ATTR** | Zmienne przeżywające deep sleep (max 8+8 KB) |
| **Deep Sleep** | ~10 µA, pełny reboot po wybudzeniu, tylko RTC SRAM zachowana |
| **Light Sleep** | ~0.8 mA, CPU kontynuuje po wybudzeniu, SRAM zachowana |
| **GPIO 6–11** | Zajęte przez Flash — NIGDY nie używaj! |
| **GPIO 16–17** | Na WROVER-B zajęte przez PSRAM — nie używaj! |
| **GPIO 12** | Strapping pin VDD_SDIO — ustaw eFuse na 3.3V! |
| **ADC2** | Nie działa gdy WiFi aktywne — używaj ADC1 (GPIO 32–39) |

### 9.2 Dalsze kroki

Po opanowaniu architektury przejdź do:
- **Moduł 0.3** — Podstawowe narzędzia programisty (`menuconfig`, `monitor`, logi, debugging)
- **Faza 1** — GPIO i podstawowe peryferia (diody, przyciski, timery)

---

## 10. Źródła i dokumentacja

### Dokumenty w workspace

| Plik | Zawartość |
|------|-----------|
| `esp32-wrover-b_datasheet_en.pdf` | Datasheet modułu WROVER-B — pinout, schemat, specyfikacja |
| `esp32_datasheet_en.pdf` | Datasheet chipa ESP32 — kompletna specyfikacja |
| `esp32_technical_reference_manual_en.pdf` | TRM — szczegóły rejestrów, peryferiów, pamięci |
| `NODEMCU32-V1.3.PDF` | Schemat płytki NodeMCU-32 — regulator, USB-UART, pinout |
| `esp-chip-errata-en-master-esp32.pdf` | Znane błędy chipa i obejścia (errata) |

### Dokumentacja online

- **ESP-IDF Programming Guide:** https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/
- **ESP32 Technical Reference Manual:** https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
- **ESP32-WROVER-B Datasheet:** https://www.espressif.com/sites/default/files/documentation/esp32-wrover-b_datasheet_en.pdf
- **Sleep Modes:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html
- **Heap Memory:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html

---

> *Moduł 0.2 — opracowany 28.02.2026. Przed podłączeniem jakichkolwiek peryferiów do ESP32-WROVER-B, zawsze sprawdź tabelę GPIO powyżej, aby uniknąć konfliktów z Flash, PSRAM i strapping pins.*
