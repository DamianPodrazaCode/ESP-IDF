# Moduł 2.3 — Sigma-Delta Modulation (SDM)

> **Poziom:** 🟡 Początkujący · **Czas:** Tydzień 7–9 (Faza 2)
> **Cel:** Opanowanie modulacji sigma-delta (SDM) w ESP32 — zrozumienie zasady działania, konfiguracja kanałów SDM (`sdm_new_channel()`), ustawianie gęstości impulsów (pulse density), filtr RC do konwersji na sygnał analogowy, oraz praktyczne ćwiczenie porównujące sterowanie LED z SDM i LEDC PWM.

---

## Spis treści

1. [Czym jest modulacja Sigma-Delta?](#1-czym-jest-modulacja-sigma-delta)
2. [Architektura SDM w ESP32](#2-architektura-sdm-w-esp32)
3. [API SDM — przegląd](#3-api-sdm--przegląd)
4. [Konfiguracja kanału — sdm_new_channel()](#4-konfiguracja-kanału--sdm_new_channel)
5. [Włączanie i wyłączanie kanału](#5-włączanie-i-wyłączanie-kanału)
6. [Ustawianie gęstości impulsów — sdm_channel_set_pulse_density()](#6-ustawianie-gęstości-impulsów--sdm_channel_set_pulse_density)
7. [Konwersja na sygnał analogowy — filtr RC](#7-konwersja-na-sygnał-analogowy--filtr-rc)
8. [Porównanie SDM vs LEDC PWM vs DAC](#8-porównanie-sdm-vs-ledc-pwm-vs-dac)
9. [Power Management, Thread Safety, IRAM Safe](#9-power-management-thread-safety-iram-safe)
10. [Ćwiczenie: Sterowanie jasnością LED z SDM — porównanie z LEDC PWM](#10-ćwiczenie-sterowanie-jasnością-led-z-sdm--porównanie-z-ledc-pwm)
11. [Podsumowanie i dalsze kroki](#11-podsumowanie-i-dalsze-kroki)
12. [Źródła i dokumentacja](#12-źródła-i-dokumentacja)

---

## 1. Czym jest modulacja Sigma-Delta?

### 1.1 Zasada działania

**Sigma-Delta Modulation (SDM)** — modulacja sigma-delta — to technika konwersji wartości analogowej (lub jej cyfrowej reprezentacji) na strumień szybkich impulsów cyfrowych, w których **gęstość impulsów** (pulse density) jest proporcjonalna do wartości wejściowej. Nazywana jest również **Pulse Density Modulation (PDM)**.

```
Modulacja Sigma-Delta — intuicja:

  Wartość wejściowa: 75% (wysoka gęstość impulsów)
  Wyjście SDM: ██ █ ██ ██ █ ██ ██ █ ██ ██ █ ██ ██ █ ██

  Wartość wejściowa: 50% (średnia gęstość)
  Wyjście SDM: █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █

  Wartość wejściowa: 25% (niska gęstość)
  Wyjście SDM: █   █     █   █     █   █     █   █

  Wartość wejściowa: 0% (brak impulsów)
  Wyjście SDM: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░

  W odróżnieniu od PWM (stały okres, zmienna szerokość):
  PWM 75%:  ████████████░░░░████████████░░░░████████████░░░░
  PWM 50%:  ████████░░░░░░░░████████░░░░░░░░████████░░░░░░░░
  PWM 25%:  ████░░░░░░░░░░░░████░░░░░░░░░░░░████░░░░░░░░░░░░
```

> **💡 Kluczowa różnica PWM vs SDM:**
> - **PWM** — stały okres, zmienna **szerokość** impulsu (duty cycle)
> - **SDM** — zmienny okres, zmienna **gęstość** impulsów (pulse density)
> Obie techniki po filtracji dolnoprzepustowej (RC) dają napięcie proporcjonalne do wartości wejściowej, ale SDM ma lepsze właściwości widmowe (niższe zakłócenia w paśmie audio).

### 1.2 Modulator drugiego rzędu

ESP32 posiada **modulator sigma-delta drugiego rzędu**. Modulator wyższego rzędu zapewnia lepsze kształtowanie szumu (noise shaping) — więcej energii szumu jest przesuwane do wyższych częstotliwości, co ułatwia ich odfiltrowanie.

```
Modulator Σ-Δ drugiego rzędu — schemat blokowy:

  Wejście        ┌───┐     ┌───┐     ┌─────────┐
  (density) ──►(+)─►│ Σ │──►(+)─►│ Σ │──►│Komparator│──► Wyjście
               ▲    └───┘  ▲    └───┘    │ 1-bit   │    (PDM)
               │           │             └────┬────┘
               │           │                  │
               │    ┌───┐  │    ┌───┐         │
               └────┤z⁻¹│◄─┘───┤z⁻¹│◄────────┘
                    └───┘       └───┘
                  Sprzężenie   Sprzężenie
                  zwrotne #1   zwrotne #2

  Wynik: wyjście 1-bitowe (HIGH/LOW) z gęstością impulsów
         proporcjonalną do wartości wejściowej.
         Szum kwantyzacji przesunięty do wysokich częstotliwości.
```

### 1.3 Zastosowania SDM

| Zastosowanie | Opis |
|-------------|------|
| **LED dimming** | Sterowanie jasnością LED — oko ludzkie działa jak filtr dolnoprzepustowy |
| **Prosty DAC 8-bit** | Z filtrem RC aktywnym — konwersja PDM → napięcie analogowe |
| **Wzmacniacz klasy D** | Z mostkiem H/pół-mostkiem + filtr LC |
| **Podświetlenie LCD** | Sterowanie jasnością backlight wyświetlaczy |
| **Generowanie sygnałów** | Generowanie wolno zmiennych sygnałów analogowych |

> **⚠️ Kiedy wybrać SDM zamiast PWM (LEDC)?**
> - Gdy potrzebujesz niskich zakłóceń EMI (szum SDM jest rozłożony w szerokim paśmie)
> - Gdy nie potrzebujesz precyzyjnej kontroli częstotliwości sygnału nośnego
> - Gdy chcesz prostego 8-bitowego DAC bez zajmowania prawdziwego kanału DAC (GPIO25/26)
> - Gdy zależy Ci na dowolnym GPIO (SDM nie jest ograniczony do konkretnych pinów)

---

## 2. Architektura SDM w ESP32

### 2.1 Zasoby sprzętowe

```
ESP32 — Sigma-Delta Modulator
═══════════════════════════════════════════════════════════════

  ┌──────────────────────────────────────────────────────────┐
  │              Sigma-Delta Modulator (SDM)                  │
  │                                                          │
  │  Kanały: 8 niezależnych kanałów SDM                      │
  │                                                          │
  │  ┌────────────┐ ┌────────────┐     ┌────────────┐       │
  │  │ Kanał 0    │ │ Kanał 1    │ ... │ Kanał 7    │       │
  │  │ GPIO: any  │ │ GPIO: any  │     │ GPIO: any  │       │
  │  │ density:   │ │ density:   │     │ density:   │       │
  │  │ -128..127  │ │ -128..127  │     │ -128..127  │       │
  │  └──────┬─────┘ └──────┬─────┘     └──────┬─────┘       │
  │         │              │                   │              │
  │         ▼              ▼                   ▼              │
  │     GPIO pad       GPIO pad           GPIO pad           │
  │    (dowolny)      (dowolny)          (dowolny)           │
  │                                                          │
  │  Źródło zegara: APB_CLK (80 MHz) — wspólne dla kanałów  │
  │  Sample rate: konfigurowalny (prescaler)                 │
  └──────────────────────────────────────────────────────────┘
```

**Kluczowe fakty ESP32 SDM:**
- **8 niezależnych kanałów** — każdy może być przypisany do dowolnego GPIO
- **Rozdzielczość gęstości:** 8 bitów ze znakiem (-128 do 127)
- **Modulator drugiego rzędu** — lepsze kształtowanie szumu
- **Źródło zegara:** APB_CLK (80 MHz) przez programowalny prescaler
- **Dowolne GPIO** — nie jest ograniczony do konkretnych pinów (w odróżnieniu od DAC: GPIO25/26)
- **Brak obciążenia CPU** — po konfiguracji modulacja działa sprzętowo

### 2.2 Wzór napięcia wyjściowego (po filtrze RC)

```
Vout = VDD_IO / 256 × density + VDD_IO / 2

Gdzie:
  VDD_IO   = napięcie zasilania I/O (typowo 3.3V)
  density  = wartość gęstości impulsów (-128 do 127, int8_t)

Przykłady (przy VDD_IO = 3.3V):
  density = -128  →  Vout = 3.3/256 × (-128) + 3.3/2 = -1.65 + 1.65 = 0.000 V
  density =  -64  →  Vout = 3.3/256 × (-64)  + 3.3/2 = -0.825 + 1.65 = 0.825 V
  density =    0  →  Vout = 3.3/256 × 0       + 3.3/2 = 0 + 1.65     = 1.650 V
  density =   64  →  Vout = 3.3/256 × 64      + 3.3/2 = 0.825 + 1.65 = 2.475 V
  density =  127  →  Vout = 3.3/256 × 127     + 3.3/2 = 1.636 + 1.65 = 3.286 V

Zakres napięcia wyjściowego: 0 V – ~3.3 V (pełny zakres VDD_IO)
Krok napięcia (LSB): ΔV = VDD_IO / 256 ≈ 12.89 mV
```

> **💡 Praktyczna informacja:** Dla sterowania jasnością LED filtr RC nie jest potrzebny — ludzkie oko samo działa jak filtr dolnoprzepustowy. Filtr jest potrzebny tylko gdy chcesz uzyskać rzeczywiste napięcie analogowe.

### 2.3 Sample Rate (częstotliwość próbkowania)

Sample rate SDM określa częstotliwość z jaką modulator generuje impulsy wyjściowe. Wyższy sample rate oznacza:
- Lepszy stosunek sygnału do szumu (SNR)
- Łatwiejsze odfiltrowanie szumu (wyższe częstotliwości → prostszy filtr RC)
- Wyższe zużycie energii

```
Sample Rate SDM na ESP32:

  Źródło zegara: APB_CLK = 80 MHz

  sample_rate_hz = APB_CLK / prescaler

  Typowe wartości:
    sample_rate = 1 MHz    → presaler ≈ 80   (zalecane, dobry kompromis)
    sample_rate = 500 kHz  → prescaler ≈ 160
    sample_rate = 2 MHz    → prescaler ≈ 40
    sample_rate = 100 kHz  → prescaler ≈ 800

  Zalecenie: 1 MHz to dobry punkt startowy dla większości zastosowań.
```

---

## 3. API SDM — przegląd

### 3.1 Nagłówek i CMake

```c
#include "driver/sdm.h"    // Główny nagłówek SDM API
```

W `CMakeLists.txt` komponent `esp_driver_sdm` jest dodawany automatycznie w standardowych projektach ESP-IDF.

### 3.2 Kluczowe funkcje API

```
Funkcja                            Opis
────────────────────────────────── ──────────────────────────────────────────
sdm_new_channel()                  Utworzenie nowego kanału SDM
sdm_del_channel()                  Usunięcie kanału SDM (zwolnienie zasobów)
sdm_channel_enable()               Włączenie kanału (start modulacji)
sdm_channel_disable()              Wyłączenie kanału (stop modulacji)
sdm_channel_set_pulse_density()    Ustawienie gęstości impulsów (-128..127)
sdm_channel_set_duty()             Alias dla set_pulse_density (deprecated)
```

### 3.3 Maszyna stanów kanału SDM

```
  ┌────────────┐   sdm_new_channel()    ┌────────────┐
  │  Brak      │ ──────────────────────► │   INIT     │
  │  kanału    │                         │            │
  └────────────┘                         └──────┬─────┘
                                                │
                                  sdm_channel_enable()
                                                │
                                                ▼
                                         ┌────────────┐
                                         │  ENABLED   │ ◄── sdm_channel_set_
                                         │            │      pulse_density()
                                         └──────┬─────┘      (zmiana density)
                                                │
                                  sdm_channel_disable()
                                                │
                                                ▼
  ┌────────────┐   sdm_del_channel()    ┌────────────┐
  │  Brak      │ ◄──────────────────────│   INIT     │
  │  kanału    │                         │            │
  └────────────┘                         └────────────┘

  ⚠️ Kanał MUSI być w stanie INIT aby go usunąć!
     Kolejność: disable() → del_channel()
```

---

## 4. Konfiguracja kanału — sdm_new_channel()

### 4.1 Struktura `sdm_config_t`

| Pole | Typ | Opis |
|------|-----|------|
| `gpio_num` | `int` | Numer GPIO na który wychodzą impulsy PDM (dowolny GPIO) |
| `clk_src` | `sdm_clock_source_t` | Źródło zegara: `SDM_CLK_SRC_DEFAULT` (APB 80 MHz) |
| `sample_rate_hz` | `uint32_t` | Częstotliwość próbkowania w Hz (np. 1000000 = 1 MHz) |
| `invert_out` | `bool` | Inwersja sygnału wyjściowego |
| `flags.io_loop_back` | `bool` | Loopback do testów (sygnał wyjściowy wraca na wejście) |

### 4.2 Minimalny przykład — utworzenie kanału SDM

```c
#include <stdio.h>
#include "driver/sdm.h"
#include "esp_log.h"

static const char *TAG = "SDM_BASIC";

void app_main(void)
{
    // ═══ KROK 1: Konfiguracja kanału SDM ═══
    sdm_channel_handle_t sdm_chan = NULL;
    sdm_config_t sdm_cfg = {
        .clk_src       = SDM_CLK_SRC_DEFAULT,    // APB clock (80 MHz)
        .sample_rate_hz = 1 * 1000 * 1000,       // 1 MHz sample rate
        .gpio_num      = GPIO_NUM_4,              // Wyjście na GPIO4
        .flags = {
            .invert_out  = false,                 // Bez inwersji
            .io_loop_back = false,                // Bez loopback
        },
    };
    ESP_ERROR_CHECK(sdm_new_channel(&sdm_cfg, &sdm_chan));
    ESP_LOGI(TAG, "Kanał SDM utworzony na GPIO4, sample rate: 1 MHz");

    // ═══ KROK 2: Włączenie kanału ═══
    ESP_ERROR_CHECK(sdm_channel_enable(sdm_chan));
    ESP_LOGI(TAG, "Kanał SDM włączony");

    // ═══ KROK 3: Ustawienie gęstości impulsów ═══
    // density = 0 → 50% duty → ~1.65V (po filtrze RC)
    ESP_ERROR_CHECK(sdm_channel_set_pulse_density(sdm_chan, 0));
    ESP_LOGI(TAG, "Density = 0 → ~50%% duty (~1.65V)");

    // ═══ KROK 4: Zmiana density w pętli ═══
    while (1) {
        for (int8_t d = -128; d < 127; d++) {
            ESP_ERROR_CHECK(sdm_channel_set_pulse_density(sdm_chan, d));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        for (int8_t d = 127; d > -128; d--) {
            ESP_ERROR_CHECK(sdm_channel_set_pulse_density(sdm_chan, d));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    // Cleanup (nieosiągalny w pętli):
    // sdm_channel_disable(sdm_chan);
    // sdm_del_channel(sdm_chan);
}
```

### 4.3 Sekwencja użycia

```
1. sdm_new_channel()                  ← Utworzenie kanału (stan: INIT)
2. sdm_channel_enable()               ← Włączenie modulacji (stan: ENABLED)
3. sdm_channel_set_pulse_density()     ← Ustawienie/zmiana density (w pętli)
   ... modulacja działa sprzętowo ...
4. sdm_channel_disable()              ← Wyłączenie (stan: INIT)
5. sdm_del_channel()                  ← Zwolnienie zasobów
```

---

## 5. Włączanie i wyłączanie kanału

### 5.1 sdm_channel_enable()

Włączenie kanału SDM powoduje:
- Przejście ze stanu **INIT** do **ENABLED**
- Pobranie power management lock (jeśli `CONFIG_PM_ENABLE` jest włączone i wybrano APB clock)
- Start generowania impulsów PDM na przypisanym GPIO

```c
ESP_ERROR_CHECK(sdm_channel_enable(sdm_chan));
// Od tego momentu GPIO generuje impulsy PDM
```

### 5.2 sdm_channel_disable()

Wyłączenie kanału SDM:
- Przejście ze stanu **ENABLED** do **INIT**
- Zwolnienie power management lock
- Zatrzymanie generowania impulsów

```c
ESP_ERROR_CHECK(sdm_channel_disable(sdm_chan));
// GPIO przestaje generować impulsy
// Teraz można usunąć kanał: sdm_del_channel()
```

> **⚠️ UWAGA:** Kanał musi być w stanie **INIT** (wyłączony) zanim go usuniesz. Próba usunięcia włączonego kanału zwróci `ESP_ERR_INVALID_STATE`.

---

## 6. Ustawianie gęstości impulsów — sdm_channel_set_pulse_density()

### 6.1 Parametr density

Funkcja `sdm_channel_set_pulse_density()` przyjmuje parametr `density` typu `int8_t` w zakresie **-128 do 127**:

```
density     Duty cycle wyjścia    Vout (po filtrze, VDD=3.3V)
─────────── ────────────────────── ──────────────────────────────
-128        ~0% (same LOWy)        ≈ 0.000 V
 -90        ~15%                   ≈ 0.491 V
 -64        ~25%                   ≈ 0.825 V
   0        ~50%                   ≈ 1.650 V
  64        ~75%                   ≈ 2.475 V
  90        ~85%                   ≈ 2.809 V
 127        ~100% (same HIGHy)     ≈ 3.286 V

⚠️ Zalecany zakres: [-90, 90] — lepsza losowość (randomness)
   impulsów, mniejsze zakłócenia harmoniczne.
   Wartości bliskie -128 i 127 generują prawie stały sygnał.
```

### 6.2 Przykład — kilka poziomów jasności LED

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/sdm.h"
#include "esp_log.h"

static const char *TAG = "SDM_LEVELS";

void app_main(void)
{
    // Utworzenie i włączenie kanału SDM na GPIO4
    sdm_channel_handle_t chan = NULL;
    sdm_config_t cfg = {
        .clk_src        = SDM_CLK_SRC_DEFAULT,
        .sample_rate_hz = 1000000,     // 1 MHz
        .gpio_num       = GPIO_NUM_4,
    };
    ESP_ERROR_CHECK(sdm_new_channel(&cfg, &chan));
    ESP_ERROR_CHECK(sdm_channel_enable(chan));

    // Predefiniowane poziomy gęstości
    const int8_t levels[] = {-128, -90, -64, 0, 64, 90, 127};
    const char *labels[] = {
        "MIN (0V)", "15%", "25%", "50%", "75%", "85%", "MAX (3.3V)"
    };
    int num_levels = sizeof(levels) / sizeof(levels[0]);

    while (1) {
        for (int i = 0; i < num_levels; i++) {
            ESP_ERROR_CHECK(sdm_channel_set_pulse_density(chan, levels[i]));
            ESP_LOGI(TAG, "Density: %4d → %s", levels[i], labels[i]);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}
```

### 6.3 Kontekst ISR

Funkcja `sdm_channel_set_pulse_density()` **może być wywoływana z kontekstu ISR** — jest chroniona sekcją krytyczną. Opcja `CONFIG_SDM_CTRL_FUNC_IN_IRAM` umieszcza ją w IRAM, dzięki czemu działa nawet gdy cache jest wyłączony.

---

## 7. Konwersja na sygnał analogowy — filtr RC

### 7.1 Kiedy potrzebny jest filtr?

```
Zastosowanie                     Filtr RC potrzebny?
──────────────────────────────── ────────────────────
LED dimming                      ❌ NIE — oko = filtr
Podświetlenie LCD                ❌ NIE
Prosty DAC (napięcie analogowe)  ✅ TAK — filtr aktywny
Wzmacniacz klasy D               ✅ TAK — filtr LC
Pomiar oscyloskopem              ✅ TAK — filtr pasywny/aktywny
```

### 7.2 Pasywny filtr RC dolnoprzepustowy

```
Najprostszy filtr RC (1. rzędu):

  GPIO (SDM) ────┤ R ├────┬──── Vout (analogowe)
                           │
                          ═╤═ C
                           │
                          GND

  Częstotliwość odcięcia: fc = 1 / (2π × R × C)

  Przykład dla sample_rate = 1 MHz, pożądane fc ≈ 1 kHz:
    R = 10 kΩ,  C = 15 nF
    fc = 1 / (2π × 10000 × 15e-9) ≈ 1061 Hz  ✓

  Przykład dla fc ≈ 100 Hz (wolna zmiana, np. dimming):
    R = 10 kΩ,  C = 150 nF (0.15 µF)
    fc = 1 / (2π × 10000 × 150e-9) ≈ 106 Hz  ✓
```

> **⚠️ UWAGA:** Filtr pasywny RC obniża napięcie wyjściowe (impedancja obciążenia wpływa na dzielnik). Dla precyzyjnych zastosowań DAC użyj filtru **aktywnego** (np. Sallen-Key z op-ampem).

### 7.3 Aktywny filtr Sallen-Key (zalecany przez Espressif)

```
Filtr Sallen-Key 2. rzędu (topologia low-pass):

              R1          R2
  SDM OUT ───┤├────┬──────┤├──────┬──── (+) OP-AMP ──── Vout
                   │               │         │
                  ═╤═ C1          ═╤═ C2      │
                   │               │         │
                  GND             ├──────── (-) OP-AMP
                                  │
                                 GND

  Zalety:
    ✅ Brak strat napięcia (op-amp buforuje)
    ✅ Strome tłumienie (-40 dB/dekadę dla 2. rzędu)
    ✅ Dobre odwzorowanie napięcia SDM → Vout

  Przykładowe wartości:
    R1 = R2 = 10 kΩ
    C1 = 10 nF, C2 = 100 nF
    fc ≈ 1 / (2π × √(R1×R2×C1×C2)) ≈ 503 Hz
```

---

## 8. Porównanie SDM vs LEDC PWM vs DAC

### 8.1 Tabela porównawcza

| Cecha | **SDM** | **LEDC PWM** | **DAC** |
|-------|---------|-------------|---------|
| **Typ modulacji** | Pulse Density (PDM) | Pulse Width (PWM) | Bezpośredni DAC (R-2R) |
| **Nagłówek** | `driver/sdm.h` | `driver/ledc.h` | `driver/dac_oneshot.h` |
| **Kanały** | 8 | 16 (8 HS + 8 LS) | 2 |
| **Rozdzielczość** | 8-bit (signed) | 1–20 bit | 8-bit |
| **GPIO** | Dowolne GPIO | Dowolne GPIO | Tylko GPIO25, GPIO26 |
| **Obciążenie CPU** | Zerowe | Zerowe | Zerowe (one-shot) |
| **Hardware fade** | ❌ Brak | ✅ Wbudowany | ❌ Brak |
| **Filtr RC potrzebny** | Opcjonalny (LED: nie) | Opcjonalny (LED: nie) | ❌ Nie |
| **Spektrum szumu** | Rozproszone (noise shaping) | Harmoniki częstotliwości PWM | Brak (analogowy) |
| **Efektywne EMI** | 🟢 Niskie | 🟡 Średnie (harmoniki) | 🟢 Brak |
| **Typowe zastosowanie** | LED dim, prosty DAC | PWM serwa, LED, silniki | Napięcie ref., audio |

### 8.2 Wizualne porównanie sygnałów

```
Wyjście LEDC PWM (5 kHz, 50% duty):
  ████████░░░░░░░░████████░░░░░░░░████████░░░░░░░░████████░░░░░░░░
  |← T=200µs →|
  |← 100µs →|     ← STAŁY okres, zmienna szerokość

Wyjście SDM (1 MHz sample rate, density=0):
  █░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░█░
  ← Impulsy o ZMIENNYM rozkładzie, średnia gęstość = 50%

Wyjście SDM (1 MHz sample rate, density=64):
  ██░██░██░█░██░██░██░█░██░██░██░█░██░██░██░█░██░██░██░█░██░██░██
  ← Wyższa gęstość ≈ 75% → wyższe średnie napięcie

Po filtrze RC oba dają ten sam efekt: napięcie DC proporcjonalne
do duty/density. Różnica jest w rozkładzie zakłóceń widmowych.
```

### 8.3 Kiedy co wybrać?

```
Potrzebujesz:                              Wybierz:
───────────────────────────────────────── ─────────────
Sterowanie jasnością LED                   LEDC lub SDM (oba OK)
LED z hardware fade (breathing)            LEDC ✅
Sterowanie serwomechanizmem                LEDC ✅ (precyzyjny duty + f)
Prosty DAC na dowolnym GPIO                SDM + filtr RC ✅
Prawdziwy DAC (analogowy)                  DAC ✅ (GPIO25/26)
Audio / waveform generation                DAC continuous (DMA) ✅
Minimalne EMI                              SDM ✅ (noise shaping)
```

---

## 9. Power Management, Thread Safety, IRAM Safe

### 9.1 Power Management

Gdy power management jest włączony (`CONFIG_PM_ENABLE`), system może zmienić częstotliwość APB przed wejściem w Light-sleep, co wpływa na sample rate SDM. Driver automatycznie pobiera blokadę `ESP_PM_APB_FREQ_MAX` po wywołaniu `sdm_channel_enable()` (jeśli wybrany clock source to APB) i zwalnia ją po `sdm_channel_disable()`.

### 9.2 Thread Safety

- `sdm_new_channel()` — **thread-safe** (można wywoływać z różnych tasków)
- `sdm_channel_set_pulse_density()` — **ISR-safe** (chroniona sekcją krytyczną)
- Pozostałe funkcje (`enable`, `disable`, `del`) — **NIE są thread-safe** — unikaj wywoływania z wielu tasków jednocześnie

### 9.3 IRAM Safe

Opcja `CONFIG_SDM_CTRL_FUNC_IN_IRAM` umieszcza `sdm_channel_set_pulse_density()` w pamięci IRAM, dzięki czemu funkcja działa nawet gdy cache flash jest wyłączony (np. podczas zapisu do Flash).

### 9.4 Opcje Kconfig

| Opcja | Opis |
|-------|------|
| `CONFIG_SDM_CTRL_FUNC_IN_IRAM` | Umieszczenie funkcji sterujących w IRAM |
| `CONFIG_SDM_ENABLE_DEBUG_LOG` | Włączenie logów debug SDM (zwiększa rozmiar firmware) |

---

## 10. Ćwiczenie: Sterowanie jasnością LED z SDM — porównanie z LEDC PWM

### 10.1 Cel ćwiczenia

Porównanie **dwóch metod** sterowania jasnością LED:
1. **SDM** (Sigma-Delta Modulation) — modulacja gęstości impulsów
2. **LEDC PWM** — klasyczna modulacja szerokości impulsu

Obie metody sterują tą samą diodą LED (na zmianę), a program loguje wartości i pozwala wizualnie porównać efekty.

### 10.2 Schemat podłączenia

```
ESP32 NodeMCU-32
┌──────────────────┐
│                  │
│  GPIO4 (SDM) ────┼──── [LED 🔴 #1] ──── [330Ω] ──── GND
│                  │
│  GPIO2 (LEDC) ───┼──── [LED 🟢 #2] ──── [330Ω] ──── GND
│                  │
│  GND ────────────┼──── GND (wspólna masa)
│                  │
└──────────────────┘

Opcjonalny filtr RC na wyjściu SDM (GPIO4):
  GPIO4 ──┤ 10kΩ ├──┬── Vout (do pomiaru oscyloskopem)
                     │
                    ═╤═ 150nF
                     │
                    GND

Elementy:
  • 2× LED (różne kolory dla rozróżnienia)
  • 2× Rezystor 220–470 Ω
  • Opcjonalnie: 1× rezystor 10 kΩ + 1× kondensator 150 nF (filtr RC)
  • Kable połączeniowe, płytka stykowa
```

### 10.3 Kompletny kod

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/sdm.h"
#include "driver/ledc.h"
#include "esp_log.h"

// ═══════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════
static const char *TAG = "SDM_vs_LEDC";

// SDM — konfiguracja
#define SDM_GPIO            GPIO_NUM_4
#define SDM_SAMPLE_RATE_HZ  (1 * 1000 * 1000)   // 1 MHz

// LEDC PWM — konfiguracja
#define LEDC_GPIO           GPIO_NUM_2
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT    // 8192 poziomów
#define LEDC_FREQUENCY      5000                  // 5 kHz
#define LEDC_DUTY_MAX       ((1 << 13) - 1)       // 8191

// Parametry efektu breathing
#define BREATH_STEPS        256     // Liczba kroków w jednym cyklu
#define BREATH_DELAY_MS     10      // Opóźnienie między krokami

// ═══════════════════════════════════════════
// Inicjalizacja SDM
// ═══════════════════════════════════════════
static sdm_channel_handle_t sdm_chan = NULL;

static void init_sdm(void)
{
    sdm_config_t sdm_cfg = {
        .clk_src        = SDM_CLK_SRC_DEFAULT,
        .sample_rate_hz = SDM_SAMPLE_RATE_HZ,
        .gpio_num       = SDM_GPIO,
    };
    ESP_ERROR_CHECK(sdm_new_channel(&sdm_cfg, &sdm_chan));
    ESP_ERROR_CHECK(sdm_channel_enable(sdm_chan));
    ESP_LOGI(TAG, "SDM: GPIO%d, sample rate: %d Hz", SDM_GPIO, SDM_SAMPLE_RATE_HZ);
}

// ═══════════════════════════════════════════
// Inicjalizacja LEDC PWM
// ═══════════════════════════════════════════
static void init_ledc(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t chan_cfg = {
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = LEDC_GPIO,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_cfg));
    ESP_LOGI(TAG, "LEDC: GPIO%d, %d Hz, %d-bit", LEDC_GPIO, LEDC_FREQUENCY, 13);
}

// ═══════════════════════════════════════════
// Główna funkcja
// ═══════════════════════════════════════════
void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 2.3: SDM vs LEDC PWM — porównanie ===");

    // ── Inicjalizacja obu metod ──
    init_sdm();
    init_ledc();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "LED #1 (GPIO%d): SDM — Sigma-Delta Modulation", SDM_GPIO);
    ESP_LOGI(TAG, "LED #2 (GPIO%d): LEDC — PWM 5 kHz, 13-bit", LEDC_GPIO);
    ESP_LOGI(TAG, "Oba LEDy wykonują równocześnie efekt 'breathing'");
    ESP_LOGI(TAG, "Porównaj wizualnie płynność przejść!");
    ESP_LOGI(TAG, "");

    uint32_t cycle = 0;

    while (1) {
        cycle++;
        ESP_LOGI(TAG, "── Cykl %lu: Rozjaśnianie ──", (unsigned long)cycle);

        // ═══ FADE UP: 0% → 100% ═══
        for (int step = 0; step < BREATH_STEPS; step++) {
            // SDM: mapowanie kroku (0..255) → density (-128..127)
            int8_t density = (int8_t)(step - 128);
            ESP_ERROR_CHECK(sdm_channel_set_pulse_density(sdm_chan, density));

            // LEDC: mapowanie kroku (0..255) → duty (0..8191)
            uint32_t duty = (uint32_t)((float)step / 255.0f * LEDC_DUTY_MAX);
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

            // Log co 64 kroki
            if (step % 64 == 0) {
                float pct = step * 100.0f / 255.0f;
                ESP_LOGI(TAG, "  Step %3d: SDM density=%4d | LEDC duty=%5lu | ~%.0f%%",
                         step, density, (unsigned long)duty, pct);
            }

            vTaskDelay(pdMS_TO_TICKS(BREATH_DELAY_MS));
        }

        ESP_LOGI(TAG, "── Cykl %lu: Ściemnianie ──", (unsigned long)cycle);

        // ═══ FADE DOWN: 100% → 0% ═══
        for (int step = BREATH_STEPS - 1; step >= 0; step--) {
            int8_t density = (int8_t)(step - 128);
            ESP_ERROR_CHECK(sdm_channel_set_pulse_density(sdm_chan, density));

            uint32_t duty = (uint32_t)((float)step / 255.0f * LEDC_DUTY_MAX);
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

            if (step % 64 == 0) {
                float pct = step * 100.0f / 255.0f;
                ESP_LOGI(TAG, "  Step %3d: SDM density=%4d | LEDC duty=%5lu | ~%.0f%%",
                         step, density, (unsigned long)duty, pct);
            }

            vTaskDelay(pdMS_TO_TICKS(BREATH_DELAY_MS));
        }

        // Pauza między cyklami
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 10.4 Oczekiwany wynik

```
I (325) SDM_vs_LEDC: === Moduł 2.3: SDM vs LEDC PWM — porównanie ===
I (330) SDM_vs_LEDC: SDM: GPIO4, sample rate: 1000000 Hz
I (338) SDM_vs_LEDC: LEDC: GPIO2, 5000 Hz, 13-bit
I (343) SDM_vs_LEDC:
I (346) SDM_vs_LEDC: LED #1 (GPIO4): SDM — Sigma-Delta Modulation
I (352) SDM_vs_LEDC: LED #2 (GPIO2): LEDC — PWM 5 kHz, 13-bit
I (358) SDM_vs_LEDC: Oba LEDy wykonują równocześnie efekt 'breathing'
I (365) SDM_vs_LEDC: Porównaj wizualnie płynność przejść!
I (371) SDM_vs_LEDC:
I (373) SDM_vs_LEDC: ── Cykl 1: Rozjaśnianie ──
I (378) SDM_vs_LEDC:   Step   0: SDM density=-128 | LEDC duty=    0 | ~0%
I (1023) SDM_vs_LEDC:  Step  64: SDM density= -64 | LEDC duty= 2055 | ~25%
I (1668) SDM_vs_LEDC:  Step 128: SDM density=   0 | LEDC duty= 4112 | ~50%
I (2313) SDM_vs_LEDC:  Step 192: SDM density=  64 | LEDC duty= 6168 | ~75%
I (2958) SDM_vs_LEDC: ── Cykl 1: Ściemnianie ──
...
```

### 10.5 Co obserwować

```
Obserwacje wizualne:
════════════════════

1. Płynność przejść:
   • SDM: 256 kroków (8-bit) — płynne przejścia, bardzo podobne do LEDC
   • LEDC: 8192 kroków (13-bit) — potencjalnie płynniejsze, ale ludzkie
     oko nie widzi różnicy powyżej ~256 poziomów

2. Migotanie (flicker):
   • SDM: Brak migotania — impulsy PDM mają wysoką częstotliwość (1 MHz)
   • LEDC: Brak migotania przy 5 kHz (próg widzialności ~100 Hz)

3. Z filtrem RC (oscyloskop):
   • SDM: Sygnał wyjściowy to szum wysoko-częstotliwościowy → po filtrze
     RC gładkie napięcie DC proporcjonalne do density
   • LEDC: Sygnał wyjściowy to prostokąt 5 kHz → po filtrze RC
     gładkie napięcie DC z mniejszym ripple przy wyższej f

4. Główna różnica widoczna na oscyloskopie:
   • PWM: widoczne harmoniki na częstotliwości PWM i jej wielokrotnościach
   • SDM: szum rozłożony równomiernie w szerokim paśmie (noise shaping)
```

---

## 11. Podsumowanie i dalsze kroki

### 11.1 Kluczowe wnioski

```
✅ SDM (Sigma-Delta Modulation) — to co zapamiętać:

1. ZASADA: Modulacja gęstości impulsów (PDM) — zmienna gęstość,
   nie szerokość impulsu jak w PWM.

2. API: 5 funkcji — sdm_new_channel(), sdm_channel_enable(),
   sdm_channel_set_pulse_density(), sdm_channel_disable(),
   sdm_del_channel().

3. DENSITY: int8_t (-128..127) — mapuje się na napięcie 0..VDD
   po filtrze RC. Zalecany zakres [-90, 90] dla lepszej jakości.

4. GPIO: Dowolny GPIO (nie jak DAC: tylko GPIO25/26).

5. KANAŁY: 8 niezależnych kanałów sprzętowych.

6. BEZ CPU: Po konfiguracji modulacja działa sprzętowo.

7. FILTR RC: Dla LED niepotrzebny (oko = filtr). Dla DAC/pomiarów
   wymagany filtr dolnoprzepustowy (pasywny RC lub aktywny Sallen-Key).

8. EMI: Niższe zakłócenia EMI niż PWM dzięki noise shaping.
```

### 11.2 Mapa powiązań

```
Moduł 2.3 (SDM) — powiązania z innymi modułami:
═════════════════════════════════════════════════

  Moduł 1.4 (LEDC)         ← LEDC PWM — alternatywa do sterowania LED
       ↕ porównanie
  Moduł 2.3 (SDM)          ← TEN MODUŁ — Sigma-Delta Modulation
       ↕ porównanie
  Moduł 2.2 (DAC)          ← DAC — prawdziwy przetwornik C/A (GPIO25/26)

  Moduł 1.1 (GPIO)         ← GPIO — konfiguracja pinów wyjściowych
  Moduł 2.1 (ADC)          ← ADC — pomiar napięcia po filtrze RC
  Moduł 6.1 (FreeRTOS)     ← Taski RTOS do dynamicznego sterowania
```

### 11.3 Dalsze kroki

- **Moduł 3.1:** UART — komunikacja szeregowa z PC
- **Moduł 3.2:** I2C — magistrala komunikacyjna do czujników
- **Moduł 3.3:** SPI — szybka komunikacja z wyświetlaczami

---

## 12. Źródła i dokumentacja

### 12.1 Oficjalna dokumentacja ESP-IDF

| Zasób | Link |
|-------|------|
| **SDM API Reference** | [docs.espressif.com/…/sdm.html](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/peripherals/sdm.html) |
| **SDM LED Example** | [github.com/espressif/…/sdm_led](https://github.com/espressif/esp-idf/tree/v5.4.1/examples/peripherals/sigma_delta/sdm_led) |
| **SDM DAC Example** | [github.com/espressif/…/sdm_dac](https://github.com/espressif/esp-idf/tree/v5.4.1/examples/peripherals/sigma_delta/sdm_dac) |
| **LEDC API Reference** | [docs.espressif.com/…/ledc.html](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/peripherals/ledc.html) |
| **ESP32 Technical Reference Manual** | [espressif.com/…/esp32_technical_reference_manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf) |

### 12.2 Pliki w workspace

| Plik | Opis |
|------|------|
| `esp32_technical_reference_manual_en.pdf` | Technical Reference Manual — rozdział o Sigma-Delta Modulator |
| `esp32_datasheet_en.pdf` | Datasheet ESP32 — specyfikacja peryferiów |
| `Modul_1_4_LEDC.md` | Dokumentacja LEDC PWM — do porównania |
| `Modul_2_2_DAC.md` | Dokumentacja DAC — do porównania |

### 12.3 Materiały dodatkowe

| Temat | Link |
|-------|------|
| **Delta-sigma modulation (Wikipedia)** | [en.wikipedia.org/wiki/Delta-sigma_modulation](https://en.wikipedia.org/wiki/Delta-sigma_modulation) |
| **Pulse-density modulation (Wikipedia)** | [en.wikipedia.org/wiki/Pulse-density_modulation](https://en.wikipedia.org/wiki/Pulse-density_modulation) |
| **Sallen-Key topology** | [en.wikipedia.org/wiki/Sallen–Key_topology](https://en.wikipedia.org/wiki/Sallen%E2%80%93Key_topology) |
| **RC Low-pass filter design** | [electronics-tutorials.ws](https://www.electronics-tutorials.ws/filter/filter_2.html) |
