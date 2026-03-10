# Moduł 2.1 — Analog to Digital Converter (ADC)

> **Poziom:** 🟡 Początkujący · **Czas:** Tydzień 7–9 (Faza 2)
> **Cel:** Opanowanie przetwornika analogowo-cyfrowego (ADC) w ESP32 — konfiguracja jednostek ADC1/ADC2, rozdzielczość, atenuacja, kalibracja, tryby one-shot i continuous, oraz praktyczne ćwiczenia z czujnikami analogowymi (LDR, dżojstik, Sharp).

---

## Spis treści

1. [Architektura ADC w ESP32](#1-architektura-adc-w-esp32)
2. [Mapowanie kanałów ADC na GPIO](#2-mapowanie-kanałów-adc-na-gpio)
3. [Atenuacja — zakres napięcia wejściowego](#3-atenuacja--zakres-napięcia-wejściowego)
4. [API ADC One-shot Mode](#4-api-adc-one-shot-mode)
5. [Kalibracja ADC](#5-kalibracja-adc)
6. [API ADC Continuous Mode](#6-api-adc-continuous-mode)
7. [One-shot vs Continuous — porównanie](#7-one-shot-vs-continuous--porównanie)
8. [Redukcja szumu — filtrowanie i kondensatory](#8-redukcja-szumu--filtrowanie-i-kondensatory)
9. [Ćwiczenie 1: Czujnik światła LDR — pomiar natężenia oświetlenia](#9-ćwiczenie-1-czujnik-światła-ldr--pomiar-natężenia-oświetlenia)
10. [Ćwiczenie 2: Dżojstik analogowy — dwie osie + przycisk](#10-ćwiczenie-2-dżojstik-analogowy--dwie-osie--przycisk)
11. [Ćwiczenie 3: Czujnik odległości Sharp GP2Y0A41SK0F](#11-ćwiczenie-3-czujnik-odległości-sharp-gp2y0a41sk0f)
12. [Podsumowanie i dalsze kroki](#12-podsumowanie-i-dalsze-kroki)
13. [Źródła i dokumentacja](#13-źródła-i-dokumentacja)

---

## 1. Architektura ADC w ESP32

### 1.1 Czym jest ADC?

**Analog to Digital Converter (ADC)** — przetwornik analogowo-cyfrowy — to układ wbudowany w ESP32, który mierzy napięcie analogowe na wybranych pinach GPIO i zamienia je na wartość cyfrową (liczbę całkowitą). Jest to podstawowy sposób odczytu czujników analogowych: potencjometrów, fotorezystorów, czujników odległości, dżojstików itp.

**Kluczowe fakty ESP32 ADC:**
- **2 niezależne jednostki ADC:** ADC1 (8 kanałów) i ADC2 (10 kanałów)
- **Typ:** SAR (Successive Approximation Register) — przetwornik kolejnych przybliżeń
- **Rozdzielczość:** konfigurowana 9, 10, 11 lub 12 bitów (domyślnie 12-bit = 4096 poziomów)
- **Częstotliwość próbkowania:** do ~2 MHz w trybie continuous (z DMA)
- **Napięcie referencyjne:** ~1100 mV (Vref), rozszerzane atenuatorem wejściowym
- **Kalibracja:** wbudowane schematy korekcji (eFuse, Line Fitting)

> **⚠️ UWAGA:** ADC2 jest współdzielony z kontrolerem WiFi! Gdy WiFi jest aktywne, kanały ADC2 mogą być niedostępne lub zwracać błędne wyniki. **Zawsze używaj ADC1 gdy to możliwe!**

### 1.2 Schemat blokowy ADC

```
ESP32 — Przetwornik ADC (SAR)
═══════════════════════════════════════════════════════════════

  Sygnał analogowy         Atenuator              SAR ADC
  (0 – 3.3V)              wejściowy              (12-bit)
       │                      │                    │
       ▼                      ▼                    ▼
  ┌───────┐          ┌────────────┐     ┌───────────────┐
  │ GPIO  │────────►│ Attenuation│────►│  SAR Engine   │
  │ (pin) │          │ 0/2.5/6/11 │     │ 12-bit        │
  └───────┘          │ dB         │     │ 0 — 4095      │
                     └────────────┘     └───────┬───────┘
                                              │
                     ┌────────────┐     ┌─────▼───────┐
                     │ Kalibracja │◀────│  RAW Data   │
                     │ (eFuse +   │     │  (0..4095)  │
                     │  Line Fit) │     └─────────────┘
                     └──────┬─────┘
                            │
                            ▼
                     ┌────────────┐
                     │ Voltage mV │   → Skalibrowany wynik w mV
                     └────────────┘
```

### 1.3 ADC1 vs ADC2

| Cecha | ADC1 | ADC2 |
|-------|------|------|
| **Kanały** | 8 (CH0–CH7) | 10 (CH0–CH9) |
| **GPIO** | 32–39 | 0,2,4,12–15,25–27 |
| **Konflikty WiFi** | ✔ Brak | ⚠ Tak! |
| **Continuous mode** | ✔ Tak | ✔ Tak |
| **One-shot mode** | ✔ Tak | ✔ Tak (z mutex) |
| **ULP** | ✔ Tak | ✔ Tak |
| **Zalecenie** | ✅ PREFEROWANY | ⚠ Używaj ostrożnie |

> **💡 Ważne:** Gdy WiFi jest aktywne, ADC2 jest blokowany przez driver WiFi. Funkcja `adc_oneshot_read()` ma wbudowany mutex, ale może zwrócić `ESP_ERR_TIMEOUT`. Planuj swoje połączenia tak, by czujniki analogowe były na **ADC1 (GPIO 32–39)**.

---

## 2. Mapowanie kanałów ADC na GPIO

### 2.1 ADC1 — kanały i piny

```
Kanał ADC1     GPIO    Uwagi
────────────── ─────── ────────────────────────────────────
ADC1_CH0       GPIO36  VP (input only), brak pull-up/down
ADC1_CH1       GPIO37  (input only), brak pull-up/down
ADC1_CH2       GPIO38  (input only), brak pull-up/down
ADC1_CH3       GPIO39  VN (input only), brak pull-up/down
ADC1_CH4       GPIO32  XTAL_32K_P (może być użyty jako ADC)
ADC1_CH5       GPIO33  XTAL_32K_N (może być użyty jako ADC)
ADC1_CH6       GPIO34  (input only), brak pull-up/down
ADC1_CH7       GPIO35  (input only), brak pull-up/down
```

> **⚠️ GPIO 34–39** są **input-only** i NIE mają wewnętrznych rezystorów pull-up/pull-down! Jeśli czujnik tego wymaga, musisz dodać zewnętrzny rezystor.

### 2.2 ADC2 — kanały i piny

```
Kanał ADC2     GPIO    Uwagi
────────────── ─────── ────────────────────────────────────
ADC2_CH0       GPIO4   ⚠ WiFi conflict
ADC2_CH1       GPIO0   ⚠ WiFi + strapping pin (boot)
ADC2_CH2       GPIO2   ⚠ WiFi + strapping pin + LED
ADC2_CH3       GPIO15  ⚠ WiFi + strapping pin
ADC2_CH4       GPIO13  ⚠ WiFi conflict
ADC2_CH5       GPIO12  ⚠ WiFi + strapping (VDD_SDIO)
ADC2_CH6       GPIO14  ⚠ WiFi conflict
ADC2_CH7       GPIO27  ⚠ WiFi conflict
ADC2_CH8       GPIO25  ⚠ WiFi + DAC1
ADC2_CH9       GPIO26  ⚠ WiFi + DAC2
```

> **💡 Praktyczna zasada:** Do odczytu czujników analogowych **zawsze używaj ADC1** (GPIO 32–39). ADC2 rezerwuj tylko na sytuacje bez WiFi.

### 2.3 Przypisanie pinów w tym module

```
Czujnik                    GPIO    Kanał ADC1    Uwagi
────────────────────────── ─────── ───────────── ───────────
LDR (fotorezystor)         GPIO36  ADC1_CH0      Ćw.1
Dżojstik oś X             GPIO34  ADC1_CH6      Ćw.2
Dżojstik oś Y             GPIO35  ADC1_CH7      Ćw.2
Sharp GP2Y0A41SK0F         GPIO39  ADC1_CH3      Ćw.3
```

---

## 3. Atenuacja — zakres napięcia wejściowego

### 3.1 Czym jest atenuacja?

ADC ESP32 ma **wbudowany atenuator wejściowy**, który pozwala na pomiar wyższych napięć niż napięcie referencyjne (~1100 mV). Atenuacja określa stosunek napięcia wejściowego do napięcia podawanego na przetwornik SAR.

### 3.2 Tabela atenuacji

```
Atenuacja       Stała             Zakres pomiaru     Zalecany zakres
─────────────── ───────────────── ────────────────── ───────────────
0 dB            ADC_ATTEN_DB_0    0 ~ 950 mV         100 ~ 950 mV
2.5 dB          ADC_ATTEN_DB_2_5  0 ~ 1250 mV        100 ~ 1250 mV
6 dB            ADC_ATTEN_DB_6    0 ~ 1750 mV        150 ~ 1750 mV
11 dB           ADC_ATTEN_DB_11   0 ~ 2450 mV        150 ~ 2450 mV
12 dB           ADC_ATTEN_DB_12   0 ~ 3100 mV        150 ~ 3100 mV
```

> **💡** W ESP-IDF v5.x zalecane jest użycie `ADC_ATTEN_DB_12` (nowa nazwa dla 11 dB w starszych wersjach). Daje najszerszy zakres pomiarowy — do ~3.1V, co pokrywa większość zastosowań z zasilaniem 3.3V.

### 3.3 Wpływ atenuacji na dokładność

```
                  Dokładność vs Zakres
  Precyzja ▲
           │  ████
  Wysoka   │  ████  ███
           │  ████  ███  ██
  Średnia  │  ████  ███  ██   █
           │  ████  ███  ██   █
  Niska    │  ████  ███  ██   █
           └──────────────────────► Zakres napięcia
              0dB   2.5   6   11/12 dB

  0 dB:   Najwyższa precyzja, najmniejszy zakres (0-950 mV)
  12 dB:  Najniższa precyzja, największy zakres (0-3100 mV)
```

### 3.4 Wybór atenuacji w praktyce

```c
// Dla czujników 0-3.3V (większość zastosowań):
.atten = ADC_ATTEN_DB_12    // Zakres 0 ~ 3100 mV

// Dla precyzyjnych pomiarów niskich napięć (np. termopara):
.atten = ADC_ATTEN_DB_0     // Zakres 0 ~ 950 mV (najwyższa precyzja)
```

---

## 4. API ADC One-shot Mode

### 4.1 Nagłówki i CMake

```c
// Nagłówki wymagane dla ADC One-shot
#include "esp_adc/adc_oneshot.h"          // Tryb one-shot
#include "esp_adc/adc_cali.h"             // Kalibracja (bazowy)
#include "esp_adc/adc_cali_scheme.h"      // Schematy kalibracji
```

W pliku `CMakeLists.txt` komponenty `esp_adc` są dołączane automatycznie w standardowych projektach ESP-IDF.

### 4.2 Kluczowe funkcje API

```
Funkcja                                Opis
────────────────────────────────────── ────────────────────────────────────────
adc_oneshot_new_unit()                 Utworzenie instancji jednostki ADC
adc_oneshot_config_channel()           Konfiguracja kanału (atenuacja, rozdzielczość)
adc_oneshot_read()                     Odczyt surowej wartości konwersji
adc_oneshot_del_unit()                 Usunięcie instancji jednostki ADC
adc_cali_create_scheme_line_fitting()  Utworzenie schematu kalibracji (ESP32)
adc_cali_raw_to_voltage()             Konwersja RAW → mV (skalibrowana)
adc_cali_delete_scheme_line_fitting()  Usunięcie schematu kalibracji
```

### 4.3 Sekwencja użycia — krok po kroku

```
1. adc_oneshot_new_unit()            ← Rezerwacja jednostki ADC
2. adc_oneshot_config_channel()      ← Konfiguracja kanału (atten, bitwidth)
3. [opcjonalnie] Tworzenie kalibracji
4. adc_oneshot_read()                ← Odczyt RAW (w pętli)
5. [opcjonalnie] adc_cali_raw_to_voltage()  ← RAW → mV
6. adc_oneshot_del_unit()            ← Zwolnienie zasobów
```

### 4.4 Minimalny przykład — odczyt ADC1

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "ADC_BASIC";

void app_main(void)
{
    // ═══ KROK 1: Utworzenie instancji ADC1 ═══
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,               // Użyj ADC1
        .ulp_mode = ADC_ULP_MODE_DISABLE,    // Normalny tryb (nie ULP)
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    // ═══ KROK 2: Konfiguracja kanału ═══
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,       // Zakres 0–3100 mV
        .bitwidth = ADC_BITWIDTH_12,    // 12-bit (0–4095)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle,
                                               ADC_CHANNEL_0,  // GPIO36
                                               &chan_cfg));

    // ═══ KROK 3: Odczyt w pętli ═══
    int raw_value;
    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw_value));
        ESP_LOGI(TAG, "ADC1 CH0 (GPIO36): RAW = %d", raw_value);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // ═══ KROK 4: Cleanup (nieosiągalny w tej pętli) ═══
    // ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}
```

### 4.5 Pola konfiguracyjne — szczegóły

| Pole | Typ | Opis |
|------|-----|------|
| `unit_id` | `adc_unit_t` | `ADC_UNIT_1` lub `ADC_UNIT_2` |
| `ulp_mode` | `adc_ulp_mode_t` | `ADC_ULP_MODE_DISABLE` (normalny) lub `ADC_ULP_MODE_FSM` (ULP) |
| `clk_src` | `adc_oneshot_clk_src_t` | Źródło zegara (0 = domyślne) |
| `atten` | `adc_atten_t` | Atenuacja: `ADC_ATTEN_DB_0` / `_2_5` / `_6` / `_11` / `_12` |
| `bitwidth` | `adc_bitwidth_t` | Rozdzielczość: `ADC_BITWIDTH_9` / `_10` / `_11` / `_12` / `_DEFAULT` |

### 4.6 Rozdzielczość — wpływ na wynik

```
Rozdzielczość    Stała                Zakres RAW    Precyzja (12 dB)
──────────────── ──────────────────── ──────────── ─────────────────
9 bit            ADC_BITWIDTH_9       0 – 511      ~6.1 mV/LSB
10 bit           ADC_BITWIDTH_10      0 – 1023     ~3.0 mV/LSB
11 bit           ADC_BITWIDTH_11      0 – 2047     ~1.5 mV/LSB
12 bit           ADC_BITWIDTH_12      0 – 4095     ~0.76 mV/LSB
Default          ADC_BITWIDTH_DEFAULT (12 bit)      ~0.76 mV/LSB
```

> **💡 Zalecenie:** Używaj `ADC_BITWIDTH_DEFAULT` (12-bit) — daje najwyższą rozdzielczość. Niższe rozdzielczości mogą być przydatne do szybszej konwersji.

---

## 5. Kalibracja ADC

### 5.1 Dlaczego kalibracja jest potrzebna?

ADC ESP32 ma napięcie referencyjne (Vref) które **różni się między chipami** — może wynosić od 1000 mV do 1200 mV (nominalnie 1100 mV). Bez kalibracji, ten sam odczyt RAW na dwóch różnych ESP32 może oznaczać różne napięcia!

```
Bez kalibracji:           Z kalibracją:
  RAW = 2048               RAW = 2048
  Chip A: ~1.55V            Chip A: 1547 mV  (dokładnie!)
  Chip B: ~1.48V            Chip B: 1547 mV  (dokładnie!)
  (różnica do 7%!)          (wynik zgodny)
```

### 5.2 Schemat kalibracji dla ESP32 — Line Fitting

ESP32 używa schematu **Line Fitting** (dopasowanie liniowe). Driver wykorzystuje dane kalibracyjne zapisane w **eFuse** podczas produkcji:

- **eFuse TP (Two Point):** dwa punkty odniesienia — najdokładniejsze
- **eFuse Vref:** wartość napięciowa Vref — dobre
- **Default Vref:** wartość domyślna (1100 mV) — najmniej dokładne (fallback)

### 5.3 Tworzenie kalibracji

```c
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static adc_cali_handle_t cali_handle = NULL;

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten)
{
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config,
                                                         &cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Kalibracja ADC: Line Fitting utworzone pomyślnie");
        return true;
    }
    ESP_LOGW(TAG, "Kalibracja ADC: błąd (%s)", esp_err_to_name(ret));
    return false;
}

// Użycie po odczycie RAW:
int raw, voltage_mv;
adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw);
adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv);
ESP_LOGI(TAG, "RAW: %d, Voltage: %d mV", raw, voltage_mv);
```

### 5.4 Struktura kalibracyjna — pola

| Pole | Typ | Opis |
|------|-----|------|
| `unit_id` | `adc_unit_t` | Jednostka ADC (ADC_UNIT_1 / ADC_UNIT_2) |
| `atten` | `adc_atten_t` | Atenuacja użyta przy odczycie |
| `bitwidth` | `adc_bitwidth_t` | Rozdzielczość (lub DEFAULT) |
| `default_vref` | `uint32_t` | Domyślne Vref w mV (0 = z eFuse) |

### 5.5 Sprawdzanie danych eFuse

```c
#include "esp_adc/adc_cali_scheme.h"

void check_efuse(void)
{
    adc_cali_line_fitting_efuse_val_t efuse_val;
    esp_err_t ret = adc_cali_scheme_line_fitting_check_efuse(&efuse_val);

    if (ret == ESP_OK) {
        switch (efuse_val) {
            case ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_TP:
                ESP_LOGI(TAG, "Kalibracja eFuse: Two Point ✅ (najlepsza)");
                break;
            case ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_VREF:
                ESP_LOGI(TAG, "Kalibracja eFuse: Vref ✅ (dobra)");
                break;
            case ADC_CALI_LINE_FITTING_EFUSE_VAL_DEFAULT_VREF:
                ESP_LOGW(TAG, "Kalibracja eFuse: Default Vref ⚠ (fallback)");
                break;
        }
    }
}
```

### 5.6 Kompletny przykład — ADC z kalibracją

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "ADC_CALI";

#define ADC_CHANNEL    ADC_CHANNEL_0    // GPIO36
#define ADC_ATTEN      ADC_ATTEN_DB_12

void app_main(void)
{
    // ── Inicjalizacja ADC ──
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle,
                                               ADC_CHANNEL,
                                               &chan_cfg));

    // ── Inicjalizacja kalibracji ──
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id  = ADC_UNIT_1,
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config,
                                                         &cali_handle);
    bool calibrated = (ret == ESP_OK);

    if (calibrated) {
        ESP_LOGI(TAG, "Kalibracja ADC aktywna (Line Fitting)");
    } else {
        ESP_LOGW(TAG, "Kalibracja niedostępna, używam RAW");
    }

    // ── Pętla odczytu ──
    int raw, voltage;
    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw));

        if (calibrated) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw,
                                                     &voltage));
            ESP_LOGI(TAG, "GPIO36: RAW=%d, Voltage=%d mV", raw, voltage);
        } else {
            voltage = raw * 3100 / 4095;
            ESP_LOGI(TAG, "GPIO36: RAW=%d, ~Voltage=%d mV (bez kalibracji)",
                     raw, voltage);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

---

## 6. API ADC Continuous Mode

### 6.1 Czym jest tryb Continuous?

W trybie **Continuous** ADC automatycznie próbkuje wybrane kanały w tle, używając **DMA** do transferu wyników do pamięci. CPU nie jest blokowany — dane są gotowe do odczytu z bufora.

```
ONE-SHOT MODE:                    CONTINUOUS MODE:
  CPU → "odczytaj!" → ADC            DMA → ADC (automatycznie)
     ← RAW ←                        Bufor ← RAW ← (strumień danych)
  CPU czeka na wynik               CPU odczytuje bufor gdy chce
```

### 6.2 Nagłówek

```c
#include "esp_adc/adc_continuous.h"
```

### 6.3 Kluczowe funkcje

```
Funkcja                                    Opis
────────────────────────────────────────── ──────────────────────────────────
adc_continuous_new_handle()                Utworzenie handlera continuous
adc_continuous_config()                    Konfiguracja kanałów + częstotliwości
adc_continuous_register_event_callbacks()  Rejestracja callbacków (done, overflow)
adc_continuous_start()                     Start ciągłego próbkowania
adc_continuous_read()                      Odczyt danych z bufora DMA
adc_continuous_stop()                      Zatrzymanie próbkowania
adc_continuous_deinit()                    Zwolnienie zasobów
```

### 6.4 Sekwencja użycia Continuous Mode

```
1. adc_continuous_new_handle()                ← Alokacja bufora DMA
2. adc_continuous_config()                    ← Kanały, sample rate, format
3. adc_continuous_register_event_callbacks()  ← Callback on_conv_done
4. adc_continuous_start()                     ← Start DMA
5. adc_continuous_read()                      ← Odczyt danych (w pętli)
6. adc_continuous_stop()                      ← Stop DMA
7. adc_continuous_deinit()                    ← Zwolnienie zasobów
```

### 6.5 Przykład — Continuous Mode

```c
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"

static const char *TAG = "ADC_CONT";

#define ADC_CONV_FRAME_SIZE  256
#define ADC_SAMPLE_FREQ_HZ   20000   // 20 kHz sampling

static TaskHandle_t s_task_handle;

// Callback ISR: konwersja zakończona — powiadom task
static bool IRAM_ATTR conv_done_cb(adc_continuous_handle_t handle,
                                   const adc_continuous_evt_data_t *edata,
                                   void *user_data)
{
    BaseType_t must_yield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &must_yield);
    return (must_yield == pdTRUE);
}

void app_main(void)
{
    s_task_handle = xTaskGetCurrentTaskHandle();

    // ── Konfiguracja handlera ──
    adc_continuous_handle_t adc_handle = NULL;
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 1024,
        .conv_frame_size    = ADC_CONV_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    // ── Konfiguracja kanałów ──
    adc_digi_pattern_config_t adc_pattern = {
        .atten     = ADC_ATTEN_DB_12,
        .channel   = ADC_CHANNEL_0,       // GPIO36
        .unit      = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = ADC_SAMPLE_FREQ_HZ,
        .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
        .format         = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num    = 1,
        .adc_pattern    = &adc_pattern,
    };
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    // ── Rejestracja callbacka ──
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle,
                                                            &cbs, NULL));

    // ── Start ──
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    ESP_LOGI(TAG, "ADC Continuous mode: %d Hz", ADC_SAMPLE_FREQ_HZ);

    // ── Pętla odczytu ──
    uint8_t result[ADC_CONV_FRAME_SIZE] = {0};
    uint32_t ret_num = 0;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (adc_continuous_read(adc_handle, result,
                                    ADC_CONV_FRAME_SIZE,
                                    &ret_num, 0) == ESP_OK) {
            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p =
                    (adc_digi_output_data_t *)&result[i];
                uint32_t channel = p->type1.channel;
                uint32_t data    = p->type1.data;

                if (channel < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                    ESP_LOGI(TAG, "CH%lu: %lu", channel, data);
                }
            }
        }
    }
}
```

> **⚠️ Ważne:** Na ESP32, tryb continuous wymaga **I2S0** jako kontrolera DMA. Jeśli I2S0 jest używany (np. przez I2S audio), continuous mode nie będzie dostępny!

### 6.6 Format danych wyjściowych

```
ESP32 używa ADC_DIGI_OUTPUT_FORMAT_TYPE1:

  ┌─────────────────────────────────┐
  │ Bit 15..12 │ Bit 11..0          │
  │ Channel    │ Data (12-bit RAW)  │
  └─────────────────────────────────┘

Odczyt przez strukturę adc_digi_output_data_t:
  p->type1.channel  → numer kanału (0–7)
  p->type1.data     → wartość RAW (0–4095)
```

---

## 7. One-shot vs Continuous — porównanie

| Cecha | One-shot | Continuous |
|-------|----------|------------|
| **Użycie CPU** | Blokuje na czas konwersji | DMA — CPU wolny |
| **Częstotliwość** | Niska (polling) | Wysoka (do ~2 MHz) |
| **Złożoność kodu** | Prosta | Bardziej złożona (callbacki, DMA) |
| **Kalibracja** | Łatwa integracja | Wymaga ręcznej konwersji |
| **Typowe zastosowanie** | Odczyt czujników co 100ms+ | Audio, oscyloskop, fast sampling |
| **Wielokanałowy** | Ręcznie w pętli | Automatyczny round-robin |
| **ADC2 + WiFi** | Mutex wbudowany | Brak ochrony |
| **Zależności HW** | Brak | Wymaga I2S0 (DMA) |

> **💡 Dla większości zastosowań z czujnikami** (LDR, dżojstik, czujnik odległości) **One-shot mode jest wystarczający i zalecany.** Continuous mode jest potrzebny tylko przy szybkim próbkowaniu (audio, oscyloskop).

---

## 8. Redukcja szumu — filtrowanie i kondensatory

### 8.1 Źródła szumu ADC w ESP32

ADC ESP32 jest **wrażliwy na szum**. Typowe źródła:
- Szum zasilania (switching regulator na płytce NodeMCU)
- Zakłócenia EMI od WiFi/BT
- Impedancja źródła sygnału zbyt wysoka
- Brak kondensatora filtrującego na wejściu

### 8.2 Metody redukcji szumu

```
1. KONDENSATOR na wejściu (ZAWSZE zalecany!):

   GPIO ────┤├──── GND
          100nF (ceramiczny)

   Filtruje szumy wysokoczęstotliwościowe.

2. MULTISAMPLING (uśrednianie):
   Odczytaj N próbek i oblicz średnią.
   Im więcej próbek, tym mniejszy szum (ale wolniejszy odczyt).
   Typowo: 16–64 próbki.

3. FILTR EMA (Exponential Moving Average):
   filtered = alpha * new_sample + (1 - alpha) * filtered
   alpha = 0.1..0.3 (mniejsze = gładsze, ale wolniejsze)
```

### 8.3 Funkcja multisampling

```c
#define ADC_SAMPLES  64   // Liczba próbek do uśrednienia

static int adc_read_averaged(adc_oneshot_unit_handle_t handle,
                             adc_channel_t channel, int num_samples)
{
    int sum = 0;
    int raw;
    for (int i = 0; i < num_samples; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(handle, channel, &raw));
        sum += raw;
    }
    return sum / num_samples;
}

// Użycie:
int averaged_raw = adc_read_averaged(adc1_handle, ADC_CHANNEL_0, 64);
```

### 8.4 Filtr EMA (Exponential Moving Average)

```c
typedef struct {
    float alpha;       // Współczynnik wygładzania (0.0 – 1.0)
    float filtered;    // Bieżąca wartość przefiltrowana
    bool  initialized; // Czy filtr zainicjalizowany
} ema_filter_t;

static void ema_init(ema_filter_t *f, float alpha)
{
    f->alpha = alpha;
    f->filtered = 0.0f;
    f->initialized = false;
}

static float ema_update(ema_filter_t *f, float new_sample)
{
    if (!f->initialized) {
        f->filtered = new_sample;
        f->initialized = true;
    } else {
        f->filtered = f->alpha * new_sample
                    + (1.0f - f->alpha) * f->filtered;
    }
    return f->filtered;
}

// Użycie:
ema_filter_t ldr_filter;
ema_init(&ldr_filter, 0.1f);   // alpha=0.1 → bardzo gładkie

int raw;
adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw);
float smooth = ema_update(&ldr_filter, (float)raw);
```

---

## 9. Ćwiczenie 1: Czujnik światła LDR — pomiar natężenia oświetlenia

### 9.1 Cel ćwiczenia

Odczyt wartości z fotorezystora (LDR — Light Dependent Resistor) podłączonego do GPIO36 (ADC1_CH0). Program odczytuje napięcie, kalibruje wynik do mV, i interpretuje poziom oświetlenia.

### 9.2 Podłączenie LDR — dzielnik napięciowy

```
          3.3V
           │
           │
         ┌─┴─┐
         │LDR│  Fotorezystor (jasno: ~1kΩ, ciemno: ~1MΩ)
         └─┬─┘
           │
           ├──────── GPIO36 (ADC1_CH0)
           │
         ┌─┴─┐
         │10k│  Rezystor stały 10 kΩ
         └─┬─┘
           │
          GND

  Wzór dzielnika:
    V_out = 3.3V × R_stały / (R_LDR + R_stały)

  Jasno:  R_LDR ≈ 1kΩ  → V_out ≈ 3.3 × 10k/(1k+10k) ≈ 3.0V
  Ciemno: R_LDR ≈ 1MΩ  → V_out ≈ 3.3 × 10k/(1M+10k) ≈ 0.03V
```

> **💡** Dodaj **kondensator 100nF** między GPIO36 a GND aby zredukować szum ADC!

### 9.3 Kompletny kod

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

// ═══════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════
#define LDR_GPIO          GPIO_NUM_36
#define LDR_ADC_CHANNEL   ADC_CHANNEL_0      // GPIO36 = ADC1_CH0
#define LDR_ADC_ATTEN     ADC_ATTEN_DB_12    // Zakres 0–3100 mV
#define ADC_SAMPLES       32                  // Próbki do uśrednienia

static const char *TAG = "LDR_SENSOR";

// ── Struktura filtra EMA ──
typedef struct {
    float alpha;
    float value;
    bool  init;
} ema_t;

static float ema_update(ema_t *f, float sample) {
    if (!f->init) { f->value = sample; f->init = true; }
    else { f->value = f->alpha * sample + (1.0f - f->alpha) * f->value; }
    return f->value;
}

// ── Odczyt uśredniony ──
static int adc_read_avg(adc_oneshot_unit_handle_t h, adc_channel_t ch, int n)
{
    int sum = 0, raw;
    for (int i = 0; i < n; i++) {
        adc_oneshot_read(h, ch, &raw);
        sum += raw;
    }
    return sum / n;
}

// ── Interpretacja poziomu światła ──
static const char* light_level_str(int voltage_mv)
{
    if (voltage_mv > 2500) return "JASNO ☀️";
    if (voltage_mv > 1500) return "UMIARKOWANIE 🌤";
    if (voltage_mv > 500)  return "PRZYCIEMNIONE 🌥";
    return "CIEMNO 🌙";
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 2.1 Ćw.1: Czujnik światła LDR ===");

    // ═══ Inicjalizacja ADC1 ═══
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = LDR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle,
                                               LDR_ADC_CHANNEL, &chan_cfg));

    // ═══ Kalibracja ═══
    adc_cali_handle_t cali = NULL;
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = LDR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    bool cal_ok = (adc_cali_create_scheme_line_fitting(&cali_cfg, &cali) == ESP_OK);
    ESP_LOGI(TAG, "Kalibracja: %s", cal_ok ? "OK ✅" : "niedostępna ⚠️");

    // ═══ Filtr EMA ═══
    ema_t filter = { .alpha = 0.15f, .init = false };

    // ═══ Pętla odczytu ═══
    ESP_LOGI(TAG, "Rozpoczynam odczyt LDR na GPIO%d...", LDR_GPIO);

    while (1) {
        int raw = adc_read_avg(adc1_handle, LDR_ADC_CHANNEL, ADC_SAMPLES);
        float smooth = ema_update(&filter, (float)raw);

        int voltage = 0;
        if (cal_ok) {
            adc_cali_raw_to_voltage(cali, (int)smooth, &voltage);
        } else {
            voltage = (int)(smooth * 3100.0f / 4095.0f);
        }

        ESP_LOGI(TAG, "RAW=%d | Filtered=%.0f | %d mV | %s",
                 raw, smooth, voltage, light_level_str(voltage));

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 9.4 Oczekiwane wyjście

```
I (325) LDR_SENSOR: === Moduł 2.1 Ćw.1: Czujnik światła LDR ===
I (330) LDR_SENSOR: Kalibracja: OK ✅
I (335) LDR_SENSOR: Rozpoczynam odczyt LDR na GPIO36...
I (840) LDR_SENSOR: RAW=3150 | Filtered=3150 | 2890 mV | JASNO ☀️
I (1345) LDR_SENSOR: RAW=3170 | Filtered=3153 | 2893 mV | JASNO ☀️
I (1850) LDR_SENSOR: RAW=1200 | Filtered=2860 | 2567 mV | JASNO ☀️
I (2355) LDR_SENSOR: RAW=450  | Filtered=2498 | 2190 mV | UMIARKOWANIE 🌤
I (2860) LDR_SENSOR: RAW=120  | Filtered=2141 | 1832 mV | UMIARKOWANIE 🌤
```

---

## 10. Ćwiczenie 2: Dżojstik analogowy — dwie osie + przycisk

### 10.1 Cel ćwiczenia

Odczyt dżojstika analogowego z dwoma osiami (X, Y) i przyciskiem (SW). Osie podłączone do ADC, przycisk do GPIO z przerwaniem.

### 10.2 Podłączenie dżojstika

```
Dżojstik                 ESP32 NodeMCU-32
┌─────────────┐           ┌────────────────┐
│ VCC         ├───────────┤ 3.3V           │
│ GND         ├───────────┤ GND            │
│ VRx (oś X)  ├───────────┤ GPIO34 (ADC1_CH6) │
│ VRy (oś Y)  ├───────────┤ GPIO35 (ADC1_CH7) │
│ SW (przycisk)├──────────┤ GPIO32         │
└─────────────┘           └────────────────┘

  Oś X/Y:
    Centralna pozycja: ~1.65V (RAW ≈ 2048)
    Lewo/Dół: ~0V (RAW ≈ 0)
    Prawo/Góra: ~3.3V (RAW ≈ 4095)

  Przycisk (SW):
    Nienaciśnięty: HIGH (pull-up)
    Naciśnięty: LOW (GND)
```

### 10.3 Kompletny kod

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "esp_log.h"

// ═══════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════
#define JOY_X_CHANNEL    ADC_CHANNEL_6      // GPIO34
#define JOY_Y_CHANNEL    ADC_CHANNEL_7      // GPIO35
#define JOY_SW_GPIO      GPIO_NUM_32        // Przycisk dżojstika
#define ADC_ATTEN_USED   ADC_ATTEN_DB_12
#define DEADZONE         200                 // Strefa martwa (centralna)
#define CENTER_VALUE     2048                // Wartość centralna (12-bit / 2)
#define ADC_SAMPLES      16

static const char *TAG = "JOYSTICK";
static volatile bool button_pressed = false;

// ── ISR przycisku ──
static void IRAM_ATTR button_isr(void *arg)
{
    button_pressed = true;
}

// ── Odczyt uśredniony ──
static int adc_avg(adc_oneshot_unit_handle_t h, adc_channel_t ch)
{
    int sum = 0, raw;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_oneshot_read(h, ch, &raw);
        sum += raw;
    }
    return sum / ADC_SAMPLES;
}

// ── Konwersja RAW na pozycję -100..+100 ──
static int raw_to_position(int raw)
{
    int offset = raw - CENTER_VALUE;
    if (abs(offset) < DEADZONE) return 0;  // Strefa martwa

    if (offset > 0) {
        return (int)((float)(offset - DEADZONE) /
                     (float)(4095 - CENTER_VALUE - DEADZONE) * 100.0f);
    } else {
        return (int)((float)(offset + DEADZONE) /
                     (float)(CENTER_VALUE - DEADZONE) * 100.0f);
    }
}

// ── Nazwa kierunku ──
static const char* direction_str(int x, int y)
{
    if (x == 0 && y == 0) return "CENTRUM";
    if (abs(x) > abs(y)) return (x > 0) ? "PRAWO →" : "LEWO ←";
    return (y > 0) ? "GÓRA ↑" : "DÓŁ ↓";
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 2.1 Ćw.2: Dżojstik analogowy ===");

    // ═══ Konfiguracja przycisku z przerwaniem ═══
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << JOY_SW_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(JOY_SW_GPIO, button_isr, NULL);

    // ═══ Inicjalizacja ADC1 ═══
    adc_oneshot_unit_handle_t adc1;
    adc_oneshot_unit_init_cfg_t init = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init, &adc1));

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = ADC_ATTEN_USED,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, JOY_X_CHANNEL, &ch_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, JOY_Y_CHANNEL, &ch_cfg));

    // ═══ Kalibracja ═══
    adc_cali_handle_t cali = NULL;
    adc_cali_line_fitting_config_t cal_cfg = {
        .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    bool cal_ok = (adc_cali_create_scheme_line_fitting(&cal_cfg, &cali) == ESP_OK);

    // ═══ Pętla odczytu ═══
    ESP_LOGI(TAG, "Odczyt dżojstika... Porusz osiami i naciśnij przycisk!");

    while (1) {
        int raw_x = adc_avg(adc1, JOY_X_CHANNEL);
        int raw_y = adc_avg(adc1, JOY_Y_CHANNEL);

        int pos_x = raw_to_position(raw_x);
        int pos_y = raw_to_position(raw_y);

        int mv_x = 0, mv_y = 0;
        if (cal_ok) {
            adc_cali_raw_to_voltage(cali, raw_x, &mv_x);
            adc_cali_raw_to_voltage(cali, raw_y, &mv_y);
        }

        ESP_LOGI(TAG, "X: RAW=%d (%dmV) pos=%+4d%% | "
                      "Y: RAW=%d (%dmV) pos=%+4d%% | %s%s",
                 raw_x, mv_x, pos_x,
                 raw_y, mv_y, pos_y,
                 direction_str(pos_x, pos_y),
                 button_pressed ? " | 🔘 NACIŚNIĘTY!" : "");

        if (button_pressed) button_pressed = false;

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

---

## 11. Ćwiczenie 3: Czujnik odległości Sharp GP2Y0A41SK0F

### 11.1 Cel ćwiczenia

Odczyt czujnika odległości **Sharp GP2Y0A41SK0F** (zakres 4–30 cm) podłączonego do GPIO39 (ADC1_CH3). Konwersja napięcia analogowego na odległość w centymetrach.

### 11.2 Charakterystyka czujnika

```
Sharp GP2Y0A41SK0F — czujnik odległości IR
═══════════════════════════════════════════
  Zakres pomiaru:     4 – 30 cm
  Napięcie zasilania: 4.5 – 5.5V (⚠ wymaga 5V!)
  Napięcie wyjścia:   0.3 – 3.1V (bezpieczne dla ESP32 3.3V)
  Prąd:               ~12 mA
  Czas odpowiedzi:    ~16.5 ms (60 Hz)

  Charakterystyka V(d) — nieliniowa!
  ────────────────────────────────────
  Napięcie ▲
   3.1V    │  ●
   2.5V    │    ●
   2.0V    │       ●
   1.5V    │           ●
   1.0V    │                ●
   0.5V    │                       ●
   0.3V    │                              ●
           └──────────────────────────────────► Odległość
           4   6    8   10   15   20   30  cm
```

### 11.3 Podłączenie

```
Sharp GP2Y0A41SK0F        ESP32 NodeMCU-32
┌──────────────────┐       ┌────────────────┐
│ VCC (czerwony)   ├───────┤ 5V (VIN)       │  ⚠ Zasilanie 5V!
│ GND (czarny)     ├───────┤ GND            │
│ Vo  (żółty)      ├───┬───┤ GPIO39 (VN)    │  ADC1_CH3
└──────────────────┘   │   └────────────────┘
                       │
                     ─┤├─  100nF kondensator
                       │      (filtr szumu)
                      GND

  ⚠ WAŻNE:
  • Czujnik Sharp WYMAGA zasilania 5V (pin VIN na NodeMCU)
  • Napięcie wyjschode Vo = 0.3-3.1V — bezpieczne dla GPIO ESP32
  • Dodaj 100nF + 10µF na zasilaniu czujnika (stabilizacja)
```

### 11.4 Konwersja napięcie → odległość

Charakterystyka Sharp GP2Y0A41SK0F jest **nieliniowa** i najlepiej aproksymowana wzorem:

```
  distance_cm = A / (voltage_mV - B)

  Gdzie (wartości z datasheetu, dopasowane empirycznie):
    A ≈ 12000
    B ≈ -50

  Alternatywnie — tablica korekcyjna (LUT) z datasheetu:

   mV       cm
   ────── ─────
   3100     4
   2700     5
   2300     6
   1900     7
   1650     8
   1400    10
   1100    12
    900    15
    750    18
    600    20
    500    25
    400    30
```

### 11.5 Kompletny kod

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

// ═══════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════
#define SHARP_ADC_CHANNEL   ADC_CHANNEL_3     // GPIO39 (VN)
#define SHARP_ADC_ATTEN     ADC_ATTEN_DB_12
#define ADC_SAMPLES         32
#define SHARP_MIN_DIST_CM   4.0f
#define SHARP_MAX_DIST_CM   30.0f

static const char *TAG = "SHARP_SENSOR";

// ── Tablica korekcyjna (LUT) — z datasheetu ──
typedef struct { int mv; float cm; } sharp_lut_t;

static const sharp_lut_t SHARP_LUT[] = {
    {3100,  4.0f},
    {2700,  5.0f},
    {2300,  6.0f},
    {1900,  7.0f},
    {1650,  8.0f},
    {1400, 10.0f},
    {1100, 12.0f},
    { 900, 15.0f},
    { 750, 18.0f},
    { 600, 20.0f},
    { 500, 25.0f},
    { 400, 30.0f},
};
#define SHARP_LUT_SIZE  (sizeof(SHARP_LUT) / sizeof(SHARP_LUT[0]))

// ── Konwersja mV → cm z interpolacją liniową ──
static float sharp_mv_to_cm(int voltage_mv)
{
    // Poza zakresem
    if (voltage_mv >= SHARP_LUT[0].mv) return SHARP_MIN_DIST_CM;
    if (voltage_mv <= SHARP_LUT[SHARP_LUT_SIZE-1].mv) return SHARP_MAX_DIST_CM;

    // Interpolacja liniowa między punktami LUT
    for (int i = 0; i < SHARP_LUT_SIZE - 1; i++) {
        if (voltage_mv <= SHARP_LUT[i].mv &&
            voltage_mv >= SHARP_LUT[i+1].mv) {
            float ratio = (float)(SHARP_LUT[i].mv - voltage_mv) /
                          (float)(SHARP_LUT[i].mv - SHARP_LUT[i+1].mv);
            return SHARP_LUT[i].cm +
                   ratio * (SHARP_LUT[i+1].cm - SHARP_LUT[i].cm);
        }
    }
    return SHARP_MAX_DIST_CM;
}

// ── Odczyt uśredniony ──
static int adc_avg(adc_oneshot_unit_handle_t h, adc_channel_t ch)
{
    int sum = 0, raw;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_oneshot_read(h, ch, &raw);
        sum += raw;
    }
    return sum / ADC_SAMPLES;
}

// ── Wizualizacja odległości ──
static void print_distance_bar(float cm)
{
    int bars = (int)((SHARP_MAX_DIST_CM - cm) /
                     SHARP_MAX_DIST_CM * 30.0f);
    if (bars < 0) bars = 0;
    if (bars > 30) bars = 30;

    char bar[64] = {0};
    for (int i = 0; i < bars; i++) bar[i] = '#';
    for (int i = bars; i < 30; i++) bar[i] = '.';

    const char *emoji = (cm < 8) ? "🔴 BLISKO!" :
                        (cm < 15) ? "🟡 ŚREDNIO" : "🟢 DALEKO";

    ESP_LOGI(TAG, "%5.1f cm [%-30s] %s", cm, bar, emoji);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 2.1 Ćw.3: Sharp GP2Y0A41SK0F ===");
    ESP_LOGI(TAG, "Zakres: %.0f – %.0f cm", SHARP_MIN_DIST_CM, SHARP_MAX_DIST_CM);

    // ═══ Inicjalizacja ADC1 ═══
    adc_oneshot_unit_handle_t adc1;
    adc_oneshot_unit_init_cfg_t init = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init, &adc1));

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = SHARP_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, SHARP_ADC_CHANNEL,
                                               &ch_cfg));

    // ═══ Kalibracja ═══
    adc_cali_handle_t cali = NULL;
    adc_cali_line_fitting_config_t cal_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = SHARP_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    bool cal_ok = (adc_cali_create_scheme_line_fitting(&cal_cfg, &cali) == ESP_OK);
    ESP_LOGI(TAG, "Kalibracja: %s", cal_ok ? "OK ✅" : "niedostępna ⚠️");

    // ═══ Filtr EMA ═══
    float filtered_cm = 15.0f;
    const float ema_alpha = 0.2f;

    // ═══ Pętla odczytu ═══
    while (1) {
        int raw = adc_avg(adc1, SHARP_ADC_CHANNEL);
        int mv = 0;
        if (cal_ok) {
            adc_cali_raw_to_voltage(cali, raw, &mv);
        } else {
            mv = raw * 3100 / 4095;
        }

        float cm = sharp_mv_to_cm(mv);

        // Filtr EMA na odległość
        filtered_cm = ema_alpha * cm + (1.0f - ema_alpha) * filtered_cm;

        print_distance_bar(filtered_cm);

        vTaskDelay(pdMS_TO_TICKS(100));   // 10 Hz odświeżanie
    }
}
```

### 11.6 Oczekiwane wyjście

```
I (325) SHARP_SENSOR: === Moduł 2.1 Ćw.3: Sharp GP2Y0A41SK0F ===
I (330) SHARP_SENSOR: Zakres: 4 – 30 cm
I (335) SHARP_SENSOR: Kalibracja: OK ✅
I (440) SHARP_SENSOR:  15.0 cm [###############...............] 🟢 DALEKO
I (545) SHARP_SENSOR:  12.3 cm [##################............] 🟡 ŚREDNIO
I (650) SHARP_SENSOR:   8.1 cm [######################........] 🟡 ŚREDNIO
I (755) SHARP_SENSOR:   5.2 cm [#########################.....] 🔴 BLISKO!
I (860) SHARP_SENSOR:   4.5 cm [##########################....] 🔴 BLISKO!
```

---

## 12. Podsumowanie i dalsze kroki

### 12.1 Co opanowaliśmy

```
✅ Architektura ADC ESP32 — SAR, ADC1 vs ADC2, konflikty z WiFi
✅ Mapowanie kanałów na GPIO — ADC1 (GPIO 32–39), ADC2 (GPIO 0–27)
✅ Atenuacja — 4 poziomy (0/2.5/6/11/12 dB), zakres napięcia
✅ Rozdzielczość — 9/10/11/12 bit, wpływ na precyzję
✅ API One-shot Mode — adc_oneshot_new_unit(), config_channel(), read()
✅ Kalibracja — Line Fitting, eFuse, adc_cali_raw_to_voltage()
✅ API Continuous Mode — DMA, callbacki, szybkie próbkowanie
✅ Redukcja szumu — kondensatory, multisampling, filtr EMA
✅ Ćwiczenie 1: Czujnik światła LDR — dzielnik napięciowy, poziomy oświetlenia
✅ Ćwiczenie 2: Dżojstik — dwie osie + przycisk, strefa martwa, kierunki
✅ Ćwiczenie 3: Sharp GP2Y0A41SK0F — konwersja V→cm, LUT, wizualizacja
```

### 12.2 Najważniejsze zasady

1. **Zawsze używaj ADC1** (GPIO 32–39) — brak konfliktów z WiFi
2. **Zawsze dodawaj kondensator 100nF** na pin ADC — redukcja szumu
3. **Używaj kalibracji** (`adc_cali_raw_to_voltage()`) — dokładne wyniki w mV
4. **Stosuj multisampling** (16–64 próbki) — stabilniejsze odczyty
5. **Filtr EMA** — dla gładkich, stabilnych odczytów w czasie rzeczywistym
6. **One-shot mode** — wystarczający dla 99% zastosowań z czujnikami

### 12.3 Dalsze kroki

- **Moduł 2.2:** DAC — generowanie napięcia wyjściowego, przebiegi z DMA
- **Moduł 2.3:** Sigma-Delta Modulation — alternatywa dla DAC/PWM
- **Faza 6:** FreeRTOS — wielozadaniowy odczyt czujników ADC w osobnych taskach

---

## 13. Źródła i dokumentacja

### 13.1 Oficjalna dokumentacja ESP-IDF

| Dokument | Link |
|----------|------|
| ADC Oneshot Mode | [docs.espressif.com — ADC Oneshot](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/peripherals/adc_oneshot.html) |
| ADC Continuous Mode | [docs.espressif.com — ADC Continuous](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/peripherals/adc_continuous.html) |
| ADC Calibration | [docs.espressif.com — ADC Calibration](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/peripherals/adc_calibration.html) |
| ESP32 Technical Reference Manual | `esp32_technical_reference_manual_en.pdf` (w workspace) |
| ESP32 Datasheet | `esp32_datasheet_en.pdf` (w workspace) |

### 13.2 Datasheety czujników (w workspace)

| Czujnik | Plik |
|---------|------|
| Sharp GP2Y0A41SK0F | `gp2y0a41sk_e.pdf` |

### 13.3 Przykłady ESP-IDF

```
Przykłady w repozytorium ESP-IDF:
  examples/peripherals/adc/oneshot_read/     — podstawowy odczyt ADC
  examples/peripherals/adc/continuous_read/  — tryb continuous z DMA
```

---

> *Moduł 2.1 — Analog to Digital Converter (ADC). Ostatnia aktualizacja: marzec 2026.*
