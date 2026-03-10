# Moduł 2.2 — Digital to Analog Converter (DAC)

> **Poziom:** 🟡 Początkujący · **Czas:** Tydzień 7–9 (Faza 2)
> **Cel:** Opanowanie przetwornika cyfrowo-analogowego (DAC) w ESP32 — generowanie stałego napięcia (one-shot), generowanie fali kosinusoidalnej (cosine mode), ciągłe generowanie przebiegów z DMA (continuous mode), oraz praktyczne ćwiczenie z czujnikiem dźwięku LM386.

---

## Spis treści

1. [Architektura DAC w ESP32](#1-architektura-dac-w-esp32)
2. [Kanały DAC — mapowanie GPIO](#2-kanały-dac--mapowanie-gpio)
3. [API DAC One-shot Mode](#3-api-dac-one-shot-mode)
4. [API DAC Cosine Mode — generator fali kosinusoidalnej](#4-api-dac-cosine-mode--generator-fali-kosinusoidalnej)
5. [API DAC Continuous Mode — DMA](#5-api-dac-continuous-mode--dma)
6. [Generowanie przebiegów — sinus, piła, trójkąt](#6-generowanie-przebiegów--sinus-piła-trójkąt)
7. [Porównanie trybów DAC](#7-porównanie-trybów-dac)
8. [Power Management, Thread Safety, IRAM Safe](#8-power-management-thread-safety-iram-safe)
9. [Ćwiczenie: Generator sygnału sinusoidalnego z weryfikacją czujnikiem dźwięku LM386](#9-ćwiczenie-generator-sygnału-sinusoidalnego-z-weryfikacją-czujnikiem-dźwięku-lm386)
10. [Podsumowanie i dalsze kroki](#10-podsumowanie-i-dalsze-kroki)
11. [Źródła i dokumentacja](#11-źródła-i-dokumentacja)

---

## 1. Architektura DAC w ESP32

### 1.1 Czym jest DAC?

**Digital to Analog Converter (DAC)** — przetwornik cyfrowo-analogowy — to układ wbudowany w ESP32, który zamienia wartość cyfrową (liczbę 0–255) na napięcie analogowe na wyjściu GPIO. Jest to operacja odwrotna do ADC — zamiast mierzyć napięcie, **generujemy** je.

**Kluczowe fakty ESP32 DAC:**
- **2 kanały DAC:** DAC1 (GPIO25) i DAC2 (GPIO26)
- **Rozdzielczość:** 8-bit (256 poziomów, wartości 0–255)
- **Zakres napięcia wyjściowego:** 0 V – Vref (≈3.3V, z pinu VDD3P3_RTC)
- **Wzór napięcia:** `V_out = Vref × digital_value / 255`
- **3 tryby pracy:** One-shot (stałe napięcie), Cosine (fala kosinusoidalna), Continuous (DMA)
- **Sprzętowy generator fali kosinusoidalnej** — bez obciążania CPU
- **DMA przez I2S0** — ciągłe generowanie dowolnych przebiegów

> **⚠️ UWAGA:** DAC w ESP32 ma tylko 8-bit rozdzielczości (256 kroków). Dla aplikacji wymagających wyższej rozdzielczości, rozważ użycie PWM z filtrem RC (LEDC) lub Sigma-Delta Modulation.

### 1.2 Schemat blokowy DAC

```
ESP32 — Przetwornik DAC (8-bit)
═══════════════════════════════════════════════════════════════

  Dane cyfrowe           DAC Core               Wyjście analogowe
  (0 – 255)              (8-bit)                (0 – Vref)
       │                    │                       │
       ▼                    ▼                       ▼
  ┌───────────┐     ┌──────────────┐         ┌──────────┐
  │ Rejestr   │────►│  Konwersja   │────────►│  GPIO    │
  │ danych    │     │  D/A (R-2R)  │         │  25/26   │
  └───────────┘     └──────────────┘         └──────────┘
       ▲                    ▲
       │                    │
  ┌────┴────┐        ┌─────┴──────┐
  │ Źródła  │        │   Cosine   │
  │ danych: │        │ Generator  │ → Sprzętowy generator
  │ CPU     │        │ (RTC_FAST) │   fali kosinusoidalnej
  │ DMA/I2S │        └────────────┘
  └─────────┘

  Tryby pracy:
  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐
  │  ONE-SHOT   │  │   COSINE    │  │  CONTINUOUS  │
  │ Stałe       │  │ Fala cos()  │  │  DMA + I2S0  │
  │ napięcie    │  │ sprzętowo   │  │  dowolne     │
  │             │  │ 130Hz–200kHz│  │  przebiegi   │
  └─────────────┘  └─────────────┘  └──────────────┘
```

### 1.3 Wzór napięcia wyjściowego

```
V_out = Vref × digi_val / 255

Gdzie:
  Vref     = napięcie na pinie VDD3P3_RTC (idealnie ≈ 3.3V)
  digi_val = wartość cyfrowa 0–255

Przykłady (przy Vref = 3.3V):
  digi_val =   0  →  V_out = 3.3 × 0/255   = 0.000 V
  digi_val = 128  →  V_out = 3.3 × 128/255  ≈ 1.655 V
  digi_val = 200  →  V_out = 3.3 × 200/255  ≈ 2.588 V
  digi_val = 255  →  V_out = 3.3 × 255/255  = 3.300 V

Krok napięcia (LSB):
  ΔV = Vref / 255 ≈ 3.3 / 255 ≈ 12.94 mV
```

> **💡** Rozdzielczość 8-bit oznacza krok ~13 mV. Dla większości zastosowań audio i generowania sygnałów jest to wystarczające. Dla precyzyjnego sterowania napięciem rozważ zewnętrzny DAC (np. MCP4725 12-bit I2C).

---

## 2. Kanały DAC — mapowanie GPIO

### 2.1 Kanały i piny

```
Kanał DAC     GPIO    Stała ESP-IDF           Alias (legacy)
───────────── ─────── ─────────────────────── ──────────────
DAC Channel 0  GPIO25  DAC_CHAN_0              DAC_CHANNEL_1
DAC Channel 1  GPIO26  DAC_CHAN_1              DAC_CHANNEL_2
```

> **⚠️ UWAGA o nazewnictwie:** W ESP-IDF v5.x indeksowanie kanałów zaczyna się od `0` (`DAC_CHAN_0`, `DAC_CHAN_1`). Stare nazwy `DAC_CHANNEL_1` i `DAC_CHANNEL_2` to aliasy wsteczne — mogą myląco sugerować że kanały są numerowane od 1. **Używaj nowych nazw!**

### 2.2 Ograniczenia pinów DAC

```
GPIO 25 (DAC_CHAN_0):
  ├── RTC_GPIO6
  ├── ADC2_CH8 (współdzielony z ADC2!)
  └── ⚠ Nie może być jednocześnie DAC i ADC/GPIO

GPIO 26 (DAC_CHAN_1):
  ├── RTC_GPIO7
  ├── ADC2_CH9 (współdzielony z ADC2!)
  └── ⚠ Nie może być jednocześnie DAC i ADC/GPIO
```

> **💡 Praktyczna zasada:** Gdy kanał DAC jest aktywny, odpowiedni pin GPIO jest w pełni zajęty — nie możesz go używać jako ADC2 ani zwykłego GPIO. Jeśli potrzebujesz ADC2 na tych pinach, musisz najpierw zwolnić kanał DAC.

### 2.3 Przypisanie pinów w tym module

```
Wyjście/Element            GPIO    Kanał DAC     Uwagi
────────────────────────── ─────── ───────────── ──────────────
DAC — generator sygnału    GPIO25  DAC_CHAN_0    Ćwiczenie
DAC — drugi kanał (opcja)  GPIO26  DAC_CHAN_1    Opcjonalnie
```

---

## 3. API DAC One-shot Mode

### 3.1 Czym jest tryb One-shot?

W trybie **One-shot** (Direct Mode) ustawiasz jedną wartość cyfrową, a DAC natychmiast konwertuje ją na odpowiednie napięcie analogowe. Napięcie jest **utrzymywane** na wyjściu do momentu ustawienia nowej wartości lub wyłączenia kanału.

```
CPU: "ustaw 128"           DAC wyjście: ~1.65V (stabilne)
CPU: "ustaw 255"           DAC wyjście: ~3.3V  (stabilne)
CPU: (nic nie robi)        DAC wyjście: ~3.3V  (nadal utrzymane)
```

### 3.2 Nagłówek i CMake

```c
#include "driver/dac_oneshot.h"    // API one-shot
```

W `CMakeLists.txt` komponent `esp_driver_dac` jest dodawany automatycznie w standardowych projektach ESP-IDF.

### 3.3 Kluczowe funkcje API

```
Funkcja                         Opis
──────────────────────────────── ────────────────────────────────────────
dac_oneshot_new_channel()        Alokacja kanału DAC w trybie one-shot
dac_oneshot_output_voltage()     Ustawienie wartości cyfrowej (0–255)
dac_oneshot_del_channel()        Zwolnienie kanału DAC
```

### 3.4 Konfiguracja — struktura `dac_oneshot_config_t`

| Pole | Typ | Opis |
|------|-----|------|
| `chan_id` | `dac_channel_t` | Kanał DAC: `DAC_CHAN_0` (GPIO25) lub `DAC_CHAN_1` (GPIO26) |

### 3.5 Sekwencja użycia

```
1. dac_oneshot_new_channel()      ← Alokacja kanału (kanał jest od razu aktywny!)
2. dac_oneshot_output_voltage()   ← Ustawienie napięcia (0–255) — w pętli
3. dac_oneshot_del_channel()      ← Zwolnienie zasobów
```

> **💡** Funkcja `dac_oneshot_output_voltage()` może być wywoływana z kontekstu ISR! Czas wykonania to ok. 7–11 µs na ESP32.

### 3.6 Minimalny przykład — stałe napięcie na DAC

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_log.h"

static const char *TAG = "DAC_ONESHOT";

void app_main(void)
{
    // ═══ KROK 1: Alokacja kanału DAC ═══
    dac_oneshot_handle_t dac_handle;
    dac_oneshot_config_t dac_cfg = {
        .chan_id = DAC_CHAN_0,    // GPIO25
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&dac_cfg, &dac_handle));
    ESP_LOGI(TAG, "DAC Channel 0 (GPIO25) zainicjalizowany");

    // ═══ KROK 2: Generowanie napięcia narastającego ═══
    uint8_t dac_value = 0;
    while (1) {
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac_handle, dac_value));

        float voltage = 3.3f * dac_value / 255.0f;
        ESP_LOGI(TAG, "DAC value: %3d → Voltage: %.3f V", dac_value, voltage);

        dac_value += 10;    // Inkrementuj o 10 (wraca do 0 po overflow uint8_t)
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // ═══ KROK 3: Cleanup (nieosiągalny w pętli) ═══
    // ESP_ERROR_CHECK(dac_oneshot_del_channel(dac_handle));
}
```

### 3.7 Przykład — sterowanie napięciem krok po kroku (schody)

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_log.h"

static const char *TAG = "DAC_STAIRS";

// Predefiniowane poziomy napięcia (8 kroków)
static const uint8_t voltage_steps[] = {0, 36, 73, 109, 146, 182, 219, 255};
//                                     0V 0.47 0.94 1.41 1.89 2.36 2.83 3.3V

void app_main(void)
{
    dac_oneshot_handle_t dac_handle;
    dac_oneshot_config_t dac_cfg = {
        .chan_id = DAC_CHAN_0,
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&dac_cfg, &dac_handle));

    int step = 0;
    int direction = 1;    // 1 = w górę, -1 = w dół
    int num_steps = sizeof(voltage_steps) / sizeof(voltage_steps[0]);

    while (1) {
        uint8_t val = voltage_steps[step];
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac_handle, val));
        ESP_LOGI(TAG, "Krok %d: DAC=%3d (%.2f V)",
                 step, val, 3.3f * val / 255.0f);

        step += direction;
        if (step >= num_steps - 1 || step <= 0) {
            direction = -direction;    // Zmień kierunek
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
```

---

## 4. API DAC Cosine Mode — generator fali kosinusoidalnej

### 4.1 Czym jest tryb Cosine?

ESP32 posiada **sprzętowy generator fali kosinusoidalnej** wbudowany w peryferium DAC. Generator pracuje niezależnie od CPU — po konfiguracji i uruchomieniu, fala jest generowana automatycznie bez żadnego obciążenia procesora.

```
Cosine Wave Generator:
  ┌──────────────┐       ┌──────────┐
  │  RTC_FAST    │──────►│ Cosine   │──────► GPIO 25/26
  │  Clock       │       │ Generator│        (fala cos)
  │  (~8 MHz)    │       │ + dzielnik│
  └──────────────┘       └──────────┘
                          Konfigurowalne:
                          • Częstotliwość (130 Hz – 200 kHz)
                          • Amplituda (0/6/12/18 dB tłumienia)
                          • Faza (0° lub 180°)
                          • DC offset
```

> **⚠️ Ważne:** Oba kanały DAC dzielą **jeden generator kosinusowy** — częstotliwość jest wspólna! Amplituda, faza i offset mogą być ustawione indywidualnie per kanał.

### 4.2 Nagłówek

```c
#include "driver/dac_cosine.h"    // API cosine mode
```

### 4.3 Kluczowe funkcje API

```
Funkcja                     Opis
─────────────────────────── ──────────────────────────────────────
dac_cosine_new_channel()    Alokacja kanału w trybie cosine
dac_cosine_start()          Uruchomienie generatora fali
dac_cosine_stop()           Zatrzymanie generatora fali
dac_cosine_del_channel()    Zwolnienie kanału
```

### 4.4 Konfiguracja — struktura `dac_cosine_config_t`

| Pole | Typ | Opis |
|------|-----|------|
| `chan_id` | `dac_channel_t` | `DAC_CHAN_0` (GPIO25) lub `DAC_CHAN_1` (GPIO26) |
| `freq_hz` | `uint32_t` | Częstotliwość fali w Hz (~130 Hz – 200 kHz) |
| `clk_src` | `dac_cosine_clk_src_t` | Źródło zegara: `DAC_COSINE_CLK_SRC_DEFAULT` (RTC_FAST) |
| `atten` | `dac_cosine_atten_t` | Tłumienie amplitudy |
| `phase` | `dac_cosine_phase_t` | Faza: `DAC_COSINE_PHASE_0` lub `DAC_COSINE_PHASE_180` |
| `offset` | `int8_t` | DC offset (-128 – 127) |
| `flags.force_set_freq` | `bool` | Wymuszenie częstotliwości (gdy drugi kanał ma inną) |

### 4.5 Tłumienie amplitudy — `dac_cosine_atten_t`

```
Stała                         Tłumienie   Amplituda    Vpp (przy 3.3V)
───────────────────────────── ─────────── ──────────── ───────────────
DAC_COSINE_ATTEN_DB_0         0 dB        1/1 (pełna)  ≈ 3.3 Vpp
DAC_COSINE_ATTEN_DB_6         6 dB        1/2          ≈ 1.65 Vpp
DAC_COSINE_ATTEN_DB_12        12 dB       1/4          ≈ 0.825 Vpp
DAC_COSINE_ATTEN_DB_18        18 dB       1/8          ≈ 0.413 Vpp
```

### 4.6 Sekwencja użycia

```
1. dac_cosine_new_channel()    ← Konfiguracja kanału cosine
2. dac_cosine_start()          ← Start generatora
   ... fala jest generowana sprzętowo ...
3. dac_cosine_stop()           ← Stop generatora
4. dac_cosine_del_channel()    ← Zwolnienie zasobów
```

### 4.7 Przykład — fala kosinusoidalna 1 kHz

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_cosine.h"
#include "esp_log.h"

static const char *TAG = "DAC_COSINE";

void app_main(void)
{
    // ═══ Konfiguracja kanału cosine ═══
    dac_cosine_handle_t cos_handle;
    dac_cosine_config_t cos_cfg = {
        .chan_id  = DAC_CHAN_0,                      // GPIO25
        .freq_hz = 1000,                             // 1 kHz
        .clk_src = DAC_COSINE_CLK_SRC_DEFAULT,       // RTC_FAST clock
        .atten   = DAC_COSINE_ATTEN_DB_0,            // Pełna amplituda
        .phase   = DAC_COSINE_PHASE_0,               // Faza 0°
        .offset  = 0,                                // Brak DC offset
        .flags   = { .force_set_freq = false },
    };
    ESP_ERROR_CHECK(dac_cosine_new_channel(&cos_cfg, &cos_handle));
    ESP_LOGI(TAG, "Cosine 1 kHz na GPIO25 skonfigurowany");

    // ═══ Start generatora ═══
    ESP_ERROR_CHECK(dac_cosine_start(cos_handle));
    ESP_LOGI(TAG, "Generator cosine uruchomiony — mierz oscyloskopem GPIO25!");

    // ═══ Fala jest generowana sprzętowo — CPU wolny ═══
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Cosine wave running...");
    }

    // Cleanup (nieosiągalny):
    // dac_cosine_stop(cos_handle);
    // dac_cosine_del_channel(cos_handle);
}
```

### 4.8 Przykład — dwa kanały z przesunięciem fazy 180°

```c
#include "driver/dac_cosine.h"
#include "esp_log.h"

static const char *TAG = "DAC_DUAL_COS";

void app_main(void)
{
    // ── Kanał 0 (GPIO25): faza 0° ──
    dac_cosine_handle_t cos0_handle;
    dac_cosine_config_t cos0_cfg = {
        .chan_id  = DAC_CHAN_0,
        .freq_hz = 500,                        // 500 Hz
        .clk_src = DAC_COSINE_CLK_SRC_DEFAULT,
        .atten   = DAC_COSINE_ATTEN_DB_0,
        .phase   = DAC_COSINE_PHASE_0,         // 0°
        .offset  = 0,
    };
    ESP_ERROR_CHECK(dac_cosine_new_channel(&cos0_cfg, &cos0_handle));

    // ── Kanał 1 (GPIO26): faza 180° ──
    dac_cosine_handle_t cos1_handle;
    dac_cosine_config_t cos1_cfg = {
        .chan_id  = DAC_CHAN_1,
        .freq_hz = 500,                        // Ta sama częstotliwość!
        .clk_src = DAC_COSINE_CLK_SRC_DEFAULT,
        .atten   = DAC_COSINE_ATTEN_DB_0,
        .phase   = DAC_COSINE_PHASE_180,       // 180° przesunięcie
        .offset  = 0,
    };
    ESP_ERROR_CHECK(dac_cosine_new_channel(&cos1_cfg, &cos1_handle));

    // ── Start obu kanałów ──
    ESP_ERROR_CHECK(dac_cosine_start(cos0_handle));
    ESP_ERROR_CHECK(dac_cosine_start(cos1_handle));
    ESP_LOGI(TAG, "Dual cosine 500 Hz: GPIO25 (0°), GPIO26 (180°)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

> **💡** Dwa kanały z przesunięciem 180° generują sygnał różnicowy — przydatne przy sterowaniu głośnikiem podłączonym między GPIO25 a GPIO26 (podwójna amplituda!).

---

## 5. API DAC Continuous Mode — DMA

### 5.1 Czym jest tryb Continuous?

W trybie **Continuous** DAC konwertuje dane z bufora pamięci w sposób ciągły, używając **DMA** (Direct Memory Access) przez kontroler **I2S0**. CPU nie jest zaangażowany w konwersję — przygotowuje dane w buforze, a DMA automatycznie przesyła je do DAC z określoną częstotliwością.

```
COSINE MODE:                     CONTINUOUS MODE:
  Sprzętowy generator              DMA → DAC (automatycznie)
  (tylko cos(), ograniczone)       Bufor ← dowolne dane
  CPU = 0% obciążenia              CPU = minimalne (przygotowanie bufora)
  Częst.: 130 Hz – 200 kHz        Częst.: 19.6 kHz – ~2 MHz (PLL_D2)
                                           648 Hz – ~2 MHz (APLL)
```

> **⚠️ UWAGA:** Na ESP32, tryb continuous używa **I2S0** jako kontrolera DMA. Jeśli I2S0 jest zajęty przez inny driver (np. I2S audio), DAC continuous mode nie będzie dostępny!

### 5.2 Nagłówek

```c
#include "driver/dac_continuous.h"    // API continuous mode
```

### 5.3 Kluczowe funkcje API

```
Funkcja                                     Opis
─────────────────────────────────────────── ──────────────────────────────────
dac_continuous_new_channels()               Alokacja kanałów w trybie continuous
dac_continuous_enable()                     Włączenie konwersji DMA
dac_continuous_disable()                    Wyłączenie konwersji DMA
dac_continuous_del_channels()               Zwolnienie zasobów

// Metody zapisu danych:
dac_continuous_write()                      Zapis synchroniczny (blokujący)
dac_continuous_write_cyclically()           Zapis cykliczny (powtarzanie bufora)

// Tryb asynchroniczny:
dac_continuous_register_event_callback()    Rejestracja callbacka on_convert_done
dac_continuous_start_async_writing()        Start asynchronicznego zapisu
dac_continuous_write_asynchronously()       Załadowanie danych w callbacku
```

### 5.4 Konfiguracja — struktura `dac_continuous_config_t`

| Pole | Typ | Opis |
|------|-----|------|
| `chan_mask` | `dac_channel_mask_t` | Maska kanałów: `DAC_CHANNEL_MASK_CH0`, `DAC_CHANNEL_MASK_CH1`, lub oba |
| `desc_num` | `uint32_t` | Liczba deskryptorów DMA (min. 2, zalecane ≥5) |
| `buf_size` | `size_t` | Rozmiar bufora DMA (32–4092 bajtów, wielokrotność 4) |
| `freq_hz` | `uint32_t` | Częstotliwość konwersji w Hz |
| `offset` | `int8_t` | DC offset danych (-128 – 127) |
| `clk_src` | `dac_continuous_digi_clk_src_t` | Źródło zegara |
| `chan_mode` | `dac_continuous_channel_mode_t` | Tryb kanałów (simul/alter) |

### 5.5 Źródła zegara

```
Źródło zegara                        Zakres częstotliwości    Uwagi
──────────────────────────────────── ──────────────────────── ─────────────────
DAC_DIGI_CLK_SRC_PLL_D2 (DEFAULT)   19.6 kHz – kilka MHz    Domyślne, stabilne
DAC_DIGI_CLK_SRC_APLL               648 Hz – kilka MHz      Może być zajęte
```

> **💡** Dla częstotliwości poniżej 19.6 kHz musisz użyć `APLL` jako źródła zegara. Jednak APLL może być zajęty przez inne peryferia (I2S, LCD). W takim przypadku driver zwróci błąd.

### 5.6 Tryby zapisu danych

```
1. SYNCHRONICZNY (dac_continuous_write):
   ┌──────┐    blokuje    ┌──────┐
   │ CPU  │──────────────►│ DMA  │──► DAC
   │      │   aż bufor    │bufor │
   │      │   załadowany  └──────┘
   └──────┘
   Zastosowanie: audio, długie sygnały

2. CYKLICZNY (dac_continuous_write_cyclically):
   ┌──────┐  załaduj raz  ┌──────┐
   │ CPU  │──────────────►│ DMA  │──► DAC ──► DMA (powtórz)
   │      │   nie blokuje │bufor │          ↑         │
   │ wolny│               └──────┘          └─────────┘
   └──────┘                          pętla cykliczna
   Zastosowanie: fale periodyczne (sinus, piła, trójkąt)

3. ASYNCHRONICZNY (callback on_convert_done):
   ┌──────┐               ┌──────┐
   │ DMA  │──callback────►│ CPU  │──── zapisz nowe dane
   │      │  "bufor       │      │     do bufora DMA
   │      │   gotowy"     │      │
   └──────┘               └──────┘
   Zastosowanie: streaming, dynamiczna zmiana danych
```

### 5.7 Sekwencja użycia — zapis cykliczny

```
1. dac_continuous_new_channels()          ← Alokacja kanałów + DMA
2. dac_continuous_enable()                ← Włączenie DMA
3. dac_continuous_write_cyclically()      ← Załadowanie bufora (cykliczne)
   ... DAC powtarza bufor w nieskończoność ...
4. dac_continuous_disable()               ← Wyłączenie DMA
5. dac_continuous_del_channels()          ← Zwolnienie zasobów
```

### 5.8 Przykład — generator sinusoidy (DMA cykliczny)

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_continuous.h"
#include "esp_log.h"

static const char *TAG = "DAC_SINE";

// ═══════════════════════════════════════════
// Parametry generatora
// ═══════════════════════════════════════════
#define WAVE_FREQ_HZ       1000     // Częstotliwość fali wyjściowej: 1 kHz
#define SAMPLE_RATE_HZ     100000   // Częstotliwość próbkowania: 100 kHz
#define TABLE_SIZE          (SAMPLE_RATE_HZ / WAVE_FREQ_HZ)  // = 100 próbek

// Bufor z jednym okresem sinusoidy
static uint8_t sine_table[TABLE_SIZE];

// Generowanie tablicy sinusoidy
static void generate_sine_table(void)
{
    for (int i = 0; i < TABLE_SIZE; i++) {
        // sin() zwraca -1.0 .. 1.0
        // Skalujemy do 0 .. 255
        float sin_val = sinf(2.0f * M_PI * i / TABLE_SIZE);
        sine_table[i] = (uint8_t)((sin_val + 1.0f) * 127.5f);
    }
    ESP_LOGI(TAG, "Sine table: %d samples, freq: %d Hz",
             TABLE_SIZE, WAVE_FREQ_HZ);
}

void app_main(void)
{
    // ── Generowanie tablicy sinusoidy ──
    generate_sine_table();

    // ── Konfiguracja DAC Continuous ──
    dac_continuous_handle_t dac_handle;
    dac_continuous_config_t cont_cfg = {
        .chan_mask  = DAC_CHANNEL_MASK_CH0,            // GPIO25
        .desc_num  = 8,                                // Deskryptory DMA
        .buf_size  = 1024,                              // Rozmiar bufora DMA
        .freq_hz   = SAMPLE_RATE_HZ,                   // 100 kHz sample rate
        .offset    = 0,
        .clk_src   = DAC_DIGI_CLK_SRC_DEFAULT,         // PLL_D2
        .chan_mode  = DAC_CHANNEL_MODE_SIMUL,
    };
    ESP_ERROR_CHECK(dac_continuous_new_channels(&cont_cfg, &dac_handle));

    // ── Włączenie konwersji ──
    ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));

    // ── Zapis cykliczny — fala powtarzana w nieskończoność ──
    size_t bytes_loaded = 0;
    ESP_ERROR_CHECK(dac_continuous_write_cyclically(
        dac_handle,
        sine_table,
        TABLE_SIZE,
        &bytes_loaded
    ));
    ESP_LOGI(TAG, "Sine wave %d Hz on GPIO25 (loaded %d bytes)",
             WAVE_FREQ_HZ, bytes_loaded);

    // ── CPU wolny — DMA robi robotę ──
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 6. Generowanie przebiegów — sinus, piła, trójkąt

### 6.1 Generowanie tablic przebiegów

Tryb Continuous DMA wymaga **tablicy próbek** jednego okresu fali. Poniżej funkcje generujące popularne przebiegi:

```c
#include <math.h>
#include <string.h>

// ── Sinusoida ──
static void generate_sine(uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++) {
        buf[i] = (uint8_t)((sinf(2.0f * M_PI * i / len) + 1.0f) * 127.5f);
    }
}

// ── Piła (sawtooth) ──
static void generate_sawtooth(uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++) {
        buf[i] = (uint8_t)(255 * i / (len - 1));
    }
}

// ── Trójkąt (triangle) ──
static void generate_triangle(uint8_t *buf, int len)
{
    int half = len / 2;
    for (int i = 0; i < len; i++) {
        if (i < half) {
            buf[i] = (uint8_t)(255 * i / half);
        } else {
            buf[i] = (uint8_t)(255 * (len - 1 - i) / (len - 1 - half));
        }
    }
}

// ── Prostokąt (square) ──
static void generate_square(uint8_t *buf, int len)
{
    int half = len / 2;
    for (int i = 0; i < len; i++) {
        buf[i] = (i < half) ? 255 : 0;
    }
}
```

### 6.2 Wizualizacja przebiegów

```
Sinusoida:              Piła (Sawtooth):
  255│    ╱╲              255│      ╱│      ╱│
     │   ╱  ╲                │     ╱ │     ╱ │
  128│──╱────╲──           128│    ╱  │    ╱  │
     │ ╱      ╲              │   ╱   │   ╱   │
    0│╱        ╲╱            0│──╱────│──╱────│

Trójkąt (Triangle):    Prostokąt (Square):
  255│   ╱╲                255│ ┌────┐      ┌───
     │  ╱  ╲                  │ │    │      │
  128│ ╱    ╲              128│ │    │      │
     │╱      ╲                │ │    │      │
    0│        ╲╱             0│─┘    └──────┘
```

### 6.3 Wymiary tablicy a częstotliwość

```
Związek:
  f_output = sample_rate / table_size

Przykłady (sample_rate = 100 kHz):
  Table size = 100  → f_output = 100000/100  = 1000 Hz
  Table size = 200  → f_output = 100000/200  =  500 Hz
  Table size =  50  → f_output = 100000/50   = 2000 Hz
  Table size = 1000 → f_output = 100000/1000 =  100 Hz

Praktyczne ograniczenia:
  • Minimalna tablica: ~20 próbek (dalej zniekształcenia)
  • Maksymalny sample rate: ~2 MHz (dalej distortion)
  • Dla audio: 44.1 kHz lub 22.05 kHz sample rate
```

---

## 7. Porównanie trybów DAC

| Cecha | One-shot | Cosine | Continuous (DMA) |
|-------|----------|--------|-------------------|
| **Nagłówek** | `dac_oneshot.h` | `dac_cosine.h` | `dac_continuous.h` |
| **Obciążenie CPU** | Minimalne (każde wywołanie) | **Zerowe** (sprzętowe) | Minimalne (DMA) |
| **Kształt fali** | Stałe napięcie DC | Tylko kosinusoida | Dowolny (sinus, piła, trójkąt...) |
| **Częstotliwość** | N/A (DC) | 130 Hz – 200 kHz | 648 Hz – ~2 MHz |
| **Złożoność kodu** | Bardzo prosta | Prosta | Średnia (DMA, bufory) |
| **Zależności HW** | Brak | RTC_FAST clock | I2S0 (DMA) |
| **Wywoływalna z ISR** | ✅ Tak | ❌ Nie | ❌ Nie |
| **Typowe zastosowanie** | Napięcie referencyjne, DC bias | Ton testowy, sygnał zegarowy | Audio, generator przebiegów |

---

## 8. Power Management, Thread Safety, IRAM Safe

### 8.1 Power Management

Gdy power management jest włączony (`CONFIG_PM_ENABLE`), system może zmienić lub wyłączyć zegar DAC przed wejściem w Light-sleep. W trybie continuous driver automatycznie blokuje zmianę częstotliwości APB zegara (`ESP_PM_APB_FREQ_MAX`) po wywołaniu `dac_continuous_enable()` i zwalnia blokadę po `dac_continuous_disable()`.

### 8.2 Thread Safety

Wszystkie publiczne API DAC są **thread-safe** — chronione mutexami. Można bezpiecznie wywoływać je z różnych tasków RTOS. Wyjątek: `dac_oneshot_output_voltage()` — jako jedyna może być wywoływana z kontekstu ISR.

### 8.3 IRAM Safe

Opcja `CONFIG_DAC_ISR_IRAM_SAFE` w menuconfig sprawia, że przerwania DMA mogą być obsługiwane nawet gdy cache jest wyłączony (np. podczas zapisu do Flash). Zwiększa zużycie IRAM.

---

## 9. Ćwiczenie: Generator sygnału sinusoidalnego z weryfikacją czujnikiem dźwięku LM386

### 9.1 Cel ćwiczenia

Wygenerowanie fali sinusoidalnej na wyjściu DAC (GPIO25) i zweryfikowanie poprawności sygnału za pomocą czujnika dźwięku LM386 podłączonego do ADC1. Program generuje sinusoidę 440 Hz (ton A4) przy użyciu trybu DMA cyklicznego, a jednocześnie odczytuje wynik z czujnika dźwięku LM386 przez ADC, monitorując poziom sygnału.

### 9.2 Podłączenie

```
ESP32 NodeMCU-32        Czujnik dźwięku LM386        Głośnik/Buzzer
═══════════════════     ══════════════════════        ═══════════════

GPIO25 (DAC_CHAN_0)─────┐
                        │
                   ┌────┤
                   │    │  (do głośnika / buzzera)
                   │ ┌──┴──┐
                   │ │Buzzer│ ← podłączony między GPIO25 a GND
                   │ └──┬──┘    (przez rezystor 100Ω opcjonalny)
                   │    │
                   GND  GND


Czujnik dźwięku LM386 → odczyt przez ADC:

                  3.3V
                   │
              ┌────┴────┐
              │  LM386   │
              │  Sound   │
  Mikrofon →  │  Sensor  │
              │          │
              │ AO  DO  │
              └─┬───┬───┘
                │   │   │
                │   │  GND
                │   │
      GPIO36 ───┘   └─── (DO — cyfrowe, opcjonalnie)
    (ADC1_CH0)

Schemat połączeń:
  ┌────────────────────────────────────────────┐
  │ ESP32                                      │
  │                                            │
  │ GPIO25 (DAC) ──→ Buzzer(+) ──→ GND       │
  │                                            │
  │ GPIO36 (ADC) ←── LM386 AO (analog out)   │
  │ 3.3V ──────────→ LM386 VCC               │
  │ GND ───────────→ LM386 GND               │
  └────────────────────────────────────────────┘
```

> **💡** Czujnik dźwięku LM386 ma wyjście analogowe (AO) proporcjonalne do poziomu dźwięku. Buzzera podłączamy bezpośrednio do GPIO25 — DAC generuje napięcie analogowe, które wprawia membranę buzzera w drgania.

### 9.3 Kompletny kod

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_continuous.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

// ═══════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════
static const char *TAG = "SIGNAL_GEN";

#define DAC_CHANNEL         DAC_CHAN_0          // GPIO25
#define WAVE_FREQ_HZ        440                // Ton A4 (440 Hz)
#define SAMPLE_RATE_HZ      44000              // 44 kHz sample rate
#define TABLE_SIZE          (SAMPLE_RATE_HZ / WAVE_FREQ_HZ)  // = 100 próbek

// ADC — czujnik dźwięku LM386
#define SOUND_ADC_CHANNEL   ADC_CHANNEL_0      // GPIO36 (ADC1_CH0)
#define SOUND_ADC_ATTEN     ADC_ATTEN_DB_12    // Zakres 0–3100 mV
#define ADC_SAMPLES         32                  // Multisampling

// ═══════════════════════════════════════════
// Bufory
// ═══════════════════════════════════════════
static uint8_t sine_table[TABLE_SIZE];

// Typ przebiegu
typedef enum {
    WAVE_SINE,
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH,
    WAVE_SQUARE,
} wave_type_t;

// ═══════════════════════════════════════════
// Generowanie tablicy przebiegu
// ═══════════════════════════════════════════
static void generate_wave(uint8_t *buf, int len, wave_type_t type)
{
    for (int i = 0; i < len; i++) {
        float t = (float)i / len;    // 0.0 .. 1.0
        float val;
        switch (type) {
            case WAVE_SINE:
                val = sinf(2.0f * M_PI * t);                 // -1..1
                buf[i] = (uint8_t)((val + 1.0f) * 127.5f);
                break;
            case WAVE_TRIANGLE:
                val = (t < 0.5f) ? (4.0f * t - 1.0f)
                                 : (3.0f - 4.0f * t);
                buf[i] = (uint8_t)((val + 1.0f) * 127.5f);
                break;
            case WAVE_SAWTOOTH:
                buf[i] = (uint8_t)(255.0f * t);
                break;
            case WAVE_SQUARE:
                buf[i] = (t < 0.5f) ? 255 : 0;
                break;
        }
    }
}

// ═══════════════════════════════════════════
// Odczyt ADC uśredniony
// ═══════════════════════════════════════════
static int adc_read_avg(adc_oneshot_unit_handle_t h, adc_channel_t ch, int n)
{
    int sum = 0, raw;
    for (int i = 0; i < n; i++) {
        adc_oneshot_read(h, ch, &raw);
        sum += raw;
    }
    return sum / n;
}

// ═══════════════════════════════════════════
// app_main
// ═══════════════════════════════════════════
void app_main(void)
{
    ESP_LOGI(TAG, "=== Generator sygnału sinusoidalnego ===");

    // ── 1. Generowanie tablicy sinusoidy ──
    generate_wave(sine_table, TABLE_SIZE, WAVE_SINE);
    ESP_LOGI(TAG, "Sine table: %d samples, freq: %d Hz, sample rate: %d Hz",
             TABLE_SIZE, WAVE_FREQ_HZ, SAMPLE_RATE_HZ);

    // ── 2. Inicjalizacja DAC Continuous ──
    dac_continuous_handle_t dac_handle;
    dac_continuous_config_t cont_cfg = {
        .chan_mask  = DAC_CHANNEL_MASK_CH0,
        .desc_num  = 8,
        .buf_size  = 1024,
        .freq_hz   = SAMPLE_RATE_HZ,
        .offset    = 0,
        .clk_src   = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode  = DAC_CHANNEL_MODE_SIMUL,
    };
    ESP_ERROR_CHECK(dac_continuous_new_channels(&cont_cfg, &dac_handle));
    ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));

    // ── 3. Zapis cykliczny sinusoidy ──
    size_t bytes_loaded;
    ESP_ERROR_CHECK(dac_continuous_write_cyclically(
        dac_handle, sine_table, TABLE_SIZE, &bytes_loaded));
    ESP_LOGI(TAG, "✅ Sinusoida %d Hz aktywna na GPIO25 (loaded %d bytes)",
             WAVE_FREQ_HZ, bytes_loaded);

    // ── 4. Inicjalizacja ADC — odczyt czujnika dźwięku ──
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t adc_init = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init, &adc1_handle));

    adc_oneshot_chan_cfg_t adc_chan_cfg = {
        .atten    = SOUND_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        adc1_handle, SOUND_ADC_CHANNEL, &adc_chan_cfg));

    // Kalibracja ADC
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = SOUND_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    bool calibrated = (adc_cali_create_scheme_line_fitting(
                          &cali_cfg, &cali_handle) == ESP_OK);

    ESP_LOGI(TAG, "ADC kalibracja: %s", calibrated ? "OK" : "brak");

    // ── 5. Pętla monitoringu dźwięku ──
    ESP_LOGI(TAG, "Monitoring poziomu dźwięku z czujnika LM386 (GPIO36)...");

    int min_raw, max_raw, raw;
    while (1) {
        // Zbierz min/max z 500 próbek (≈ kilka okresów)
        min_raw = 4095;
        max_raw = 0;
        for (int i = 0; i < 500; i++) {
            adc_oneshot_read(adc1_handle, SOUND_ADC_CHANNEL, &raw);
            if (raw < min_raw) min_raw = raw;
            if (raw > max_raw) max_raw = raw;
        }

        int amplitude_raw = max_raw - min_raw;
        int avg_raw = adc_read_avg(adc1_handle, SOUND_ADC_CHANNEL,
                                    ADC_SAMPLES);

        if (calibrated) {
            int avg_mv, min_mv, max_mv;
            adc_cali_raw_to_voltage(cali_handle, avg_raw, &avg_mv);
            adc_cali_raw_to_voltage(cali_handle, min_raw, &min_mv);
            adc_cali_raw_to_voltage(cali_handle, max_raw, &max_mv);
            ESP_LOGI(TAG, "Dźwięk: avg=%4d mV, min=%4d mV, max=%4d mV, "
                     "amp=%4d mV | %s",
                     avg_mv, min_mv, max_mv, max_mv - min_mv,
                     (amplitude_raw > 200) ? "🔊 DŹWIĘK WYKRYTY"
                                           : "🔇 cisza");
        } else {
            ESP_LOGI(TAG, "Dźwięk: avg=%4d, min=%4d, max=%4d, amp=%4d | %s",
                     avg_raw, min_raw, max_raw, amplitude_raw,
                     (amplitude_raw > 200) ? "🔊 DŹWIĘK WYKRYTY"
                                           : "🔇 cisza");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 9.4 CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc
)
```

### 9.5 Opis działania

1. **Generowanie tablicy sinusoidy** — obliczenie 100 próbek jednego okresu fali 440 Hz (ton muzyczny A4).
2. **Konfiguracja DAC continuous** — DMA z sample rate 44 kHz, tryb cykliczny.
3. **Cykliczny zapis** — `dac_continuous_write_cyclically()` ładuje tablicę do DMA, która jest powtarzana w nieskończoność.
4. **Monitoring czujnikiem LM386** — ADC1 odczytuje wyjście analogowe czujnika dźwięku, zbiera min/max z 500 próbek, oblicza amplitudę i wyświetla status.
5. **Wynik** — buzzer emituje ton 440 Hz, czujnik LM386 wykrywa dźwięk i raportuje amplitudę.

### 9.6 Warianty ćwiczenia

**Wariant A — Zmiana przebiegów w runtime:**
```c
// W pętli głównej — zmiana przebiegu co 5 sekund
wave_type_t types[] = {WAVE_SINE, WAVE_TRIANGLE, WAVE_SAWTOOTH, WAVE_SQUARE};
const char *names[] = {"SINE", "TRIANGLE", "SAWTOOTH", "SQUARE"};
int wave_idx = 0;

while (1) {
    generate_wave(sine_table, TABLE_SIZE, types[wave_idx]);
    dac_continuous_disable(dac_handle);
    dac_continuous_enable(dac_handle);
    dac_continuous_write_cyclically(dac_handle, sine_table,
                                    TABLE_SIZE, &bytes_loaded);
    ESP_LOGI(TAG, "Przebieg: %s", names[wave_idx]);
    wave_idx = (wave_idx + 1) % 4;
    vTaskDelay(pdMS_TO_TICKS(5000));
}
```

**Wariant B — Cosine mode (prostsza alternatywa):**
```c
// Jeśli potrzebujesz tylko fali kosinusoidalnej — użyj cosine mode!
dac_cosine_handle_t cos_handle;
dac_cosine_config_t cos_cfg = {
    .chan_id  = DAC_CHAN_0,
    .freq_hz = 440,
    .clk_src = DAC_COSINE_CLK_SRC_DEFAULT,
    .atten   = DAC_COSINE_ATTEN_DB_0,
    .phase   = DAC_COSINE_PHASE_0,
    .offset  = 0,
};
dac_cosine_new_channel(&cos_cfg, &cos_handle);
dac_cosine_start(cos_handle);
// Gotowe! Zero obciążenia CPU.
```

---

## 10. Podsumowanie i dalsze kroki

### 10.1 Co opanowaliśmy

```
✅ Architektura DAC ESP32            — 2 kanały, 8-bit, wzór napięcia
✅ Tryb One-shot                     — stałe napięcie, dac_oneshot_output_voltage()
✅ Tryb Cosine                       — sprzętowy generator, zero CPU
✅ Tryb Continuous (DMA)             — dowolne przebiegi przez DMA/I2S0
✅ Generowanie przebiegów            — sinus, piła, trójkąt, prostokąt
✅ Ćwiczenie praktyczne              — generator 440 Hz + weryfikacja LM386
```

### 10.2 Dalsze kroki

- **Moduł 2.3:** Sigma-Delta Modulation — alternatywa dla DAC/PWM z wyższą efektywną rozdzielczością
- **Moduł 3.6:** I2S — zaawansowane audio z DMA
- **Moduł 6.1:** FreeRTOS — wielowątkowe systemy z DAC + ADC

---

## 11. Źródła i dokumentacja

| Zasób | Link |
|-------|------|
| **ESP-IDF DAC API Reference** | [docs.espressif.com/…/dac.html](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/dac.html) |
| **Przykład: DAC One-shot** | [github.com/espressif/…/dac_oneshot](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/dac/dac_oneshot) |
| **Przykład: DAC Cosine Wave** | [github.com/espressif/…/dac_cosine_wave](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/dac/dac_cosine_wave) |
| **Przykład: Signal Generator** | [github.com/espressif/…/signal_generator](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/dac/dac_continuous/signal_generator) |
| **Przykład: DAC Audio** | [github.com/espressif/…/dac_audio](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/dac/dac_continuous/dac_audio) |
| **ESP32 Technical Reference Manual** | Rozdział 29: Digital to Analog Converter |
| **ESP32 Datasheet** | Tabela pinów — GPIO25, GPIO26 |

---

> *Moduł 2.2 — Digital to Analog Converter (DAC). Materiał przygotowany na podstawie oficjalnej dokumentacji ESP-IDF v5.4 i ESP32 Technical Reference Manual.*
