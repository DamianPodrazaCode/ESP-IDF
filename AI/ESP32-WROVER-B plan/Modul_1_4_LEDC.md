# Moduł 1.4 — LED Control (LEDC)

> **Poziom:** 🟢 Laik · **Czas:** Tydzień 3–6 (Faza 1)  
> **Cel:** Opanowanie sprzętowego kontrolera PWM (LEDC) w ESP32 — konfiguracja timerów i kanałów, rozdzielczość i częstotliwość, zmiana duty cycle, sprzętowy hardware fade, oraz praktyczne ćwiczenie z efektem breathing LED.

---

## Spis treści

1. [Architektura LEDC w ESP32](#1-architektura-ledc-w-esp32)
2. [API LEDC — przegląd](#2-api-ledc--przegląd)
3. [Konfiguracja timera — ledc_timer_config()](#3-konfiguracja-timera--ledc_timer_config)
4. [Konfiguracja kanału — ledc_channel_config()](#4-konfiguracja-kanału--ledc_channel_config)
5. [Rozdzielczość vs częstotliwość — zależność](#5-rozdzielczość-vs-częstotliwość--zależność)
6. [Zmiana duty cycle — tryb programowy](#6-zmiana-duty-cycle--tryb-programowy)
7. [Hardware fade — sprzętowe przejścia jasności](#7-hardware-fade--sprzętowe-przejścia-jasności)
8. [Tryby szybki i wolny (High / Low Speed)](#8-tryby-szybki-i-wolny-high--low-speed)
9. [Zaawansowane: Power Management, przerwania, callback fade](#9-zaawansowane-power-management-przerwania-callback-fade)
10. [Ćwiczenie 1: Breathing LED — płynne rozjaśnianie/ściemnianie](#10-ćwiczenie-1-breathing-led--płynne-rozjaśnianieściemnianie)
11. [Ćwiczenie 2: Sterowanie jasnością LED potencjometrem (ADC + LEDC)](#11-ćwiczenie-2-sterowanie-jasnością-led-potencjometrem-adc--ledc)
12. [Podsumowanie i dalsze kroki](#12-podsumowanie-i-dalsze-kroki)
13. [Źródła i dokumentacja](#13-źródła-i-dokumentacja)

---

## 1. Architektura LEDC w ESP32

### 1.1 Czym jest LEDC?

**LED Control (LEDC)** to sprzętowy kontroler PWM wbudowany w ESP32, zaprojektowany przede wszystkim do sterowania jasnością diod LED, ale nadający się do generowania sygnałów PWM do dowolnych celów (serwomechanizmy, przetworniki, buzzer, itp.).

**Kluczowe fakty:**
- ESP32 posiada **16 niezależnych kanałów PWM** — 8 szybkich (high-speed) + 8 wolnych (low-speed)
- **4 timery** dla grupy szybkiej i **4 timery** dla grupy wolnej (łącznie 8 timerów)
- Każdy kanał może być przypisany do dowolnego timera w swojej grupie
- Rozdzielczość duty cycle: od **1 do 20 bitów** (konfigurowana per timer)
- Wbudowany **hardware fade** — sprzętowe płynne przejścia jasności bez CPU
- Maksymalna częstotliwość PWM: **40 MHz** (przy rozdzielczości 1-bit = 50% duty)

**Typowe zastosowania:**
- Sterowanie jasnością LED (dimming, breathing effect)
- Sterowanie kolorem LED RGB / RGBW
- Generowanie sygnału PWM dla serwomechanizmów
- Sterowanie prędkością silników DC (przez H-bridge)
- Generowanie sygnałów taktujących (clock) dla peryferiów zewnętrznych
- Buzzer / generowanie dźwięku o zmiennej częstotliwości

### 1.2 Organizacja modułu LEDC

```
ESP32 — LED Control (LEDC) Module
═══════════════════════════════════════════════════════════════

  ┌──────────────────────────────────────────────────────────┐
  │                    LEDC Module                           │
  │                                                          │
  │  ┌───────────────────────────────────────────────────┐   │
  │  │  HIGH-SPEED GROUP (Grupa szybka)                  │   │
  │  │                                                   │   │
  │  │  Timery: HS_TIMER0  HS_TIMER1  HS_TIMER2  HS_TIMER3 │
  │  │                                                   │   │
  │  │  Kanały: HS_CH0  HS_CH1  HS_CH2  HS_CH3          │   │
  │  │          HS_CH4  HS_CH5  HS_CH6  HS_CH7          │   │
  │  │                                                   │   │
  │  │  Źródła zegara: APB_CLK (80 MHz), REF_TICK (1 MHz)│  │
  │  └───────────────────────────────────────────────────┘   │
  │                                                          │
  │  ┌───────────────────────────────────────────────────┐   │
  │  │  LOW-SPEED GROUP (Grupa wolna)                    │   │
  │  │                                                   │   │
  │  │  Timery: LS_TIMER0  LS_TIMER1  LS_TIMER2  LS_TIMER3 │
  │  │                                                   │   │
  │  │  Kanały: LS_CH0  LS_CH1  LS_CH2  LS_CH3          │   │
  │  │          LS_CH4  LS_CH5  LS_CH6  LS_CH7          │   │
  │  │                                                   │   │
  │  │  Źródła zegara: APB_CLK, REF_TICK, RC_FAST_CLK   │   │
  │  └───────────────────────────────────────────────────┘   │
  └──────────────────────────────────────────────────────────┘

  Łącznie: 8 timerów + 16 kanałów
  Wiele kanałów może współdzielić jeden timer (ta sama f i rozdzielczość)
```

### 1.3 Schemat blokowy — Timer + Channel

```
                 Źródło zegara
                 (APB 80 MHz)
                      │
                      ▼
              ┌───────────────┐
              │   Prescaler   │    freq_hz = clk_src / (prescaler × 2^duty_res)
              │   (18-bit)    │
              └───────┬───────┘
                      │
                      ▼
              ┌───────────────┐
              │    Timer N    │    Generuje bazową częstotliwość PWM
              │               │    Rozdzielczość: 1–20 bit
              └───────┬───────┘
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
    ┌──────────┐ ┌──────────┐ ┌──────────┐
    │ Channel A│ │ Channel B│ │ Channel C│   Wiele kanałów
    │ duty=25% │ │ duty=50% │ │ duty=75% │   na jednym timerze
    │ GPIO_18  │ │ GPIO_19  │ │ GPIO_21  │
    └──────────┘ └──────────┘ └──────────┘
         │            │            │
         ▼            ▼            ▼
       ┌──┐         ┌──┐         ┌──┐
       │🔴│ LED     │🟢│ LED     │🔵│ LED
       └──┘         └──┘         └──┘
```

> **💡 Ważne:** Timer definiuje **częstotliwość** i **rozdzielczość** PWM. Kanał definiuje **duty cycle** i **GPIO**. Wiele kanałów może współdzielić jeden timer — wszystkie będą miały tę samą częstotliwość, ale każdy może mieć inny duty cycle.

### 1.4 PWM — przypomnienie podstaw

```
Sygnał PWM o częstotliwości 1 kHz, duty cycle = 75%:

  HIGH ████████████░░░░████████████░░░░████████████░░░░
  LOW  ░░░░░░░░░░░░████░░░░░░░░░░░░████░░░░░░░░░░░░████

       |←── T=1ms ──→|
       |← 75% ON →|25%|

  duty_cycle = duty_value / (2^duty_resolution) × 100%

  Przy rozdzielczości 10-bit (1024 poziomów):
    duty = 768 → 768/1024 = 75%
    duty = 512 → 512/1024 = 50%
    duty = 256 → 256/1024 = 25%
    duty = 0   → 0%   (LED zgaszony)
    duty = 1023→ ~100% (LED pełna jasność)
```

### 1.5 LEDC vs programowy PWM (GPIO toggle w timerze)

| Cecha | LEDC (sprzętowy) | GPIO + GPTimer (programowy) |
|-------|------------------|-----------------------------|
| **Obciążenie CPU** | Zerowe — sprzęt generuje PWM | Każde przełączenie = ISR |
| **Ilość kanałów** | 16 (8 HS + 8 LS) | Ograniczona liczbą timerów |
| **Rozdzielczość duty** | Do 20-bit (>1M poziomów!) | Zależy od częstotliwości ISR |
| **Hardware fade** | ✅ Wbudowany sprzętowo | ❌ Wymaga logiki w ISR |
| **Jitter** | Zerowy — deterministyczny | Zmienny (zależy od schedulera) |
| **Max częstotliwość** | 40 MHz | ~100 kHz (ograniczenie ISR) |

> **💡 Kiedy używać LEDC?** Zawsze gdy potrzebujesz sygnału PWM! LEDC jest narzędziem z wyboru do sterowania LED, serw, i wszelkich zastosowań wymagających PWM.

---

## 2. API LEDC — przegląd

### 2.1 Nagłówek i CMake

```c
// Główny nagłówek — zawiera wszystkie funkcje LEDC
#include "driver/ledc.h"
```

W pliku `CMakeLists.txt` komponent `driver` jest dołączany automatycznie w standardowych projektach ESP-IDF.

### 2.2 Kluczowe funkcje API

```
Funkcja                              Opis
──────────────────────────────────── ──────────────────────────────────────────
ledc_timer_config()                  Konfiguracja timera (częstotliwość, rozdzielczość)
ledc_channel_config()                Konfiguracja kanału (GPIO, timer, duty)
ledc_set_duty()                      Ustawienie duty cycle (programowo)
ledc_update_duty()                   Zatwierdzenie zmiany duty cycle
ledc_get_duty()                      Odczyt bieżącego duty cycle
ledc_set_freq()                      Zmiana częstotliwości timera
ledc_get_freq()                      Odczyt bieżącej częstotliwości
ledc_fade_func_install()             Instalacja serwisu fade (wymagane dla fade)
ledc_fade_func_uninstall()           Odinstalowanie serwisu fade
ledc_set_fade_with_time()            Konfiguracja fade z czasem trwania
ledc_set_fade_with_step()            Konfiguracja fade z krokiem i interwałem
ledc_fade_start()                    Start operacji fade
ledc_stop()                          Zatrzymanie kanału PWM
ledc_bind_channel_timer()            Zmiana timera przypisanego do kanału
ledc_cb_register()                   Rejestracja callback'a fade end
ledc_find_suitable_duty_resolution() Znalezienie max rozdzielczości dla danej f
```

### 2.3 Kluczowe struktury

| Struktura | Opis |
|-----------|------|
| `ledc_timer_config_t` | Konfiguracja timera (mode, timer_num, freq_hz, duty_resolution, clk_cfg) |
| `ledc_channel_config_t` | Konfiguracja kanału (gpio_num, speed_mode, channel, timer_sel, duty) |
| `ledc_cbs_t` | Struktura callback'ów (fade_cb) |
| `ledc_cb_param_t` | Dane zdarzenia fade end |

### 2.4 Kluczowe enumeracje

```c
// Tryb szybkości
typedef enum {
    LEDC_HIGH_SPEED_MODE = 0,   // Grupa szybka (8 kanałów)
    LEDC_LOW_SPEED_MODE,        // Grupa wolna (8 kanałów)
} ledc_mode_t;

// Numery timerów (0–3 w każdej grupie)
typedef enum {
    LEDC_TIMER_0 = 0,
    LEDC_TIMER_1,
    LEDC_TIMER_2,
    LEDC_TIMER_3,
} ledc_timer_t;

// Numery kanałów (0–7 w każdej grupie)
typedef enum {
    LEDC_CHANNEL_0 = 0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
    LEDC_CHANNEL_4,
    LEDC_CHANNEL_5,
    LEDC_CHANNEL_6,
    LEDC_CHANNEL_7,
} ledc_channel_t;

// Rozdzielczość duty cycle (bity)
typedef enum {
    LEDC_TIMER_1_BIT = 1,      // 2 poziomy
    // ...
    LEDC_TIMER_10_BIT = 10,    // 1024 poziomów
    LEDC_TIMER_13_BIT = 13,    // 8192 poziomów
    // ...
    LEDC_TIMER_20_BIT = 20,    // 1 048 576 poziomów
} ledc_timer_bit_t;

// Tryb fade
typedef enum {
    LEDC_FADE_NO_WAIT = 0,     // Non-blocking (natychmiastowy powrót)
    LEDC_FADE_WAIT_DONE,       // Blocking (czeka na zakończenie fade)
} ledc_fade_mode_t;

// Źródło zegara
typedef enum {
    LEDC_AUTO_CLK = 0,         // Automatyczny wybór (zalecany)
    LEDC_USE_APB_CLK,          // APB 80 MHz
    LEDC_USE_REF_TICK,         // REF_TICK 1 MHz (DFS compatible)
    LEDC_USE_RC_FAST_CLK,      // RC_FAST ~8 MHz (light-sleep compatible)
} ledc_clk_cfg_t;
```

---

## 3. Konfiguracja timera — ledc_timer_config()

### 3.1 Struktura i przykład

```c
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "LEDC";

void setup_ledc_timer(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,     // Grupa wolna
        .timer_num       = LEDC_TIMER_0,             // Timer 0
        .duty_resolution = LEDC_TIMER_13_BIT,        // 13-bit = 8192 poziomów
        .freq_hz         = 5000,                     // 5 kHz
        .clk_cfg         = LEDC_AUTO_CLK,            // Automatyczny wybór zegara
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));
    ESP_LOGI(TAG, "Timer LEDC skonfigurowany: 5 kHz, 13-bit");
}
```

### 3.2 Pola ledc_timer_config_t

| Pole | Typ | Opis |
|------|-----|------|
| `speed_mode` | `ledc_mode_t` | Grupa: `LEDC_HIGH_SPEED_MODE` lub `LEDC_LOW_SPEED_MODE` |
| `timer_num` | `ledc_timer_t` | Numer timera: `LEDC_TIMER_0` – `LEDC_TIMER_3` |
| `duty_resolution` | `ledc_timer_bit_t` | Rozdzielczość duty w bitach (1–20) |
| `freq_hz` | `uint32_t` | Częstotliwość sygnału PWM w Hz |
| `clk_cfg` | `ledc_clk_cfg_t` | Źródło zegara (`LEDC_AUTO_CLK` = zalecane) |
| `deconfigure` | `bool` | Ustaw `true` aby zdekonfigurować timer |

### 3.3 Źródła zegara — porównanie

```
Źródło zegara       Częstotliwość  Grupy       Uwagi
──────────────────── ───────────── ─────────── ──────────────────────────────
APB_CLK              80 MHz        HS + LS     Domyślne, najwyższa rozdzielczość
REF_TICK             1 MHz         HS + LS     Kompatybilne z DFS
RC_FAST_CLK          ~8 MHz        Tylko LS    Kompatybilne z light-sleep
LEDC_AUTO_CLK        (auto)        HS + LS     Driver wybiera optymalnie ✅
```

> **💡 Wskazówka:** Używaj `LEDC_AUTO_CLK` — driver automatycznie dobierze najlepsze źródło zegara dla zadanej częstotliwości i rozdzielczości.

---

## 4. Konfiguracja kanału — ledc_channel_config()

### 4.1 Struktura i przykład

```c
void setup_ledc_channel(void)
{
    ledc_channel_config_t channel_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,   // Ta sama grupa co timer!
        .channel    = LEDC_CHANNEL_0,        // Kanał 0
        .timer_sel  = LEDC_TIMER_0,          // Przypisany timer 0
        .intr_type  = LEDC_INTR_DISABLE,     // Bez przerwań
        .gpio_num   = GPIO_NUM_2,            // GPIO wyjściowy
        .duty       = 0,                     // Początkowy duty = 0 (LED zgaszony)
        .hpoint     = 0,                     // Punkt startowy fazy (zwykle 0)
    };

    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    ESP_LOGI(TAG, "Kanał LEDC skonfigurowany: GPIO2, duty=0");
}
```

### 4.2 Pola ledc_channel_config_t

| Pole | Typ | Opis |
|------|-----|------|
| `speed_mode` | `ledc_mode_t` | Musi być taka sama jak timera! |
| `channel` | `ledc_channel_t` | Numer kanału (0–7) |
| `timer_sel` | `ledc_timer_t` | Numer timera do którego kanał jest przypisany |
| `intr_type` | `ledc_intr_type_t` | Typ przerwania (`LEDC_INTR_DISABLE` / `LEDC_INTR_FADE_END`) |
| `gpio_num` | `int` | Numer GPIO wyjściowego |
| `duty` | `uint32_t` | Początkowa wartość duty cycle (0 – 2^duty_resolution) |
| `hpoint` | `uint32_t` | Punkt startowy fazy w cyklu PWM (zwykle 0) |
| `sleep_mode` | `ledc_sleep_mode_t` | Zachowanie w light-sleep |

### 4.3 Minimalny kompletny setup: Timer + Channel

```c
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#define LEDC_GPIO       GPIO_NUM_2
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES   LEDC_TIMER_13_BIT    // 8192 poziomów
#define LEDC_FREQUENCY  5000                  // 5 kHz

static const char *TAG = "LEDC_BASIC";

void app_main(void)
{
    // ── KROK 1: Konfiguracja timera ──
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    // ── KROK 2: Konfiguracja kanału ──
    ledc_channel_config_t chan_cfg = {
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = LEDC_GPIO,
        .duty       = 4096,        // 50% z 8192
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_cfg));

    ESP_LOGI(TAG, "LED na GPIO%d świeci z 50%% jasnością", LEDC_GPIO);
}
```

> **⚠️ Ważne:** Zawsze konfiguruj **timer PRZED kanałem**! To gwarantuje, że kanał od razu generuje poprawny sygnał PWM o żądanej częstotliwości.

### 4.4 Schemat podłączenia LED

```
ESP32                    LED              Rezystor
┌──────────┐
│          │             ┌──┐
│   GPIO2  ├─────────────┤🔴├────┤ 330Ω ├──── GND
│          │             └──┘
│          │
│     GND  ├─────────────────────────────────── GND
└──────────┘

  Rezystor 220–470 Ω (zależy od koloru LED):
    Czerwona: 220 Ω (Vf ≈ 1.8V)
    Zielona:  330 Ω (Vf ≈ 2.2V)
    Niebieska: 100 Ω (Vf ≈ 3.0V)
    Biała:     100 Ω (Vf ≈ 3.0V)

  I = (3.3V - Vf) / R ≈ 5–15 mA
```

---

## 5. Rozdzielczość vs częstotliwość — zależność

### 5.1 Fundamentalna zasada

Częstotliwość i rozdzielczość są **odwrotnie proporcjonalne** — im wyższa częstotliwość, tym niższa maksymalna rozdzielczość, i odwrotnie.

```
Wzór:
  freq_hz = clk_src_hz / (2^duty_resolution)

  Maksymalna rozdzielczość dla danej częstotliwości (APB 80 MHz):
  duty_resolution_max = log2(80_000_000 / freq_hz)

Przykłady (APB_CLK = 80 MHz):
  ┌──────────────┬────────────────────┬──────────────┬───────────┐
  │ Częstotl. Hz │ Max rozdzielczość  │ Poziomów     │ Precyzja  │
  ├──────────────┼────────────────────┼──────────────┼───────────┤
  │     1 000    │ 16 bit             │ 65 536       │ 0.0015%   │
  │     5 000    │ 13 bit             │ 8 192        │ 0.012%    │
  │    10 000    │ 13 bit             │ 8 192        │ 0.012%    │
  │    25 000    │ 11 bit             │ 2 048        │ 0.049%    │
  │    40 000    │ 10 bit             │ 1 024        │ 0.098%    │
  │   100 000    │ 9 bit              │ 512          │ 0.20%     │
  │ 1 000 000    │ 6 bit              │ 64           │ 1.56%     │
  │40 000 000    │ 1 bit              │ 2            │ 50% only  │
  └──────────────┴────────────────────┴──────────────┴───────────┘
```

### 5.2 Funkcja pomocnicza

```c
// Znajdź maksymalną rozdzielczość dla danej częstotliwości
uint32_t max_res = ledc_find_suitable_duty_resolution(80000000, 5000);
ESP_LOGI(TAG, "Max rozdzielczość dla 5 kHz: %lu bit", (unsigned long)max_res);
// Wynik: 13
```

### 5.3 Zalecane konfiguracje dla typowych zastosowań

```
Zastosowanie           Częstotliwość   Rozdzielczość   Uwagi
────────────────────── ─────────────── ─────────────── ──────────────────
LED dimming            1–5 kHz         13 bit          Płynne przejścia
LED RGB                5 kHz           8–10 bit        256–1024 kolorów/kanał
Serwomechanizm         50 Hz           16 bit          Sygnał 1–2 ms
Buzzer / dźwięk        100–10000 Hz    1 bit           Tylko on/off
Silnik DC              20–25 kHz       10 bit          Powyżej progu słyszalności
Fast clock signal      1–40 MHz        1 bit           50% duty fixed
```

> **💡 Dla LED:** Częstotliwość 1–5 kHz jest optymalna. Ludzkie oko nie widzi migotania powyżej ~100 Hz, a niższa częstotliwość pozwala na wyższą rozdzielczość = płynniejsze przejścia.

---

## 6. Zmiana duty cycle — tryb programowy

### 6.1 Podstawowy sposób: ledc_set_duty() + ledc_update_duty()

```c
// Zmiana duty cycle wymaga DWÓCH wywołań:
// 1. ledc_set_duty()    → ustawia nową wartość w rejestrze shadow
// 2. ledc_update_duty() → zatwierdza zmianę (kopiuje do rejestru aktywnego)

// Ustaw LED na 75% jasności (przy 13-bit: 75% × 8192 = 6144)
ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 6144));
ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

// Odczytaj bieżącą wartość duty
uint32_t current_duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
ESP_LOGI(TAG, "Bieżące duty: %lu", (unsigned long)current_duty);
```

### 6.2 Przykład: Kilka poziomów jasności

```c
#define DUTY_MAX  ((1 << 13) - 1)    // 8191 przy 13-bit

void demo_brightness_levels(void)
{
    uint32_t levels[] = {
        0,                      // 0% — zgaszony
        DUTY_MAX / 4,           // 25%
        DUTY_MAX / 2,           // 50%
        DUTY_MAX * 3 / 4,       // 75%
        DUTY_MAX,               // ~100% — pełna jasność
    };

    for (int i = 0; i < 5; i++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, levels[i]);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        ESP_LOGI(TAG, "Jasność: %lu%% (duty=%lu)",
                 (unsigned long)(levels[i] * 100 / DUTY_MAX),
                 (unsigned long)levels[i]);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

> **⚠️ Ważne na ESP32:** Przy maksymalnej rozdzielczości timera, wartość duty **nie może** być równa dokładnie `2^duty_resolution` (np. 8192 przy 13-bit). Używaj `(2^duty_resolution) - 1` jako maksimum (np. 8191).

---

## 7. Hardware fade — sprzętowe przejścia jasności

### 7.1 Czym jest hardware fade?

**Hardware fade** to mechanizm LEDC, w którym sprzęt **automatycznie** zwiększa lub zmniejsza duty cycle w czasie, tworząc płynne przejście jasności **bez udziału CPU**. Jest to kluczowa funkcja dla efektów takich jak breathing LED.

```
Hardware Fade: 0% → 100% w 2000 ms

  Jasność:
  100% ─────────────────────────────────────── ████
   75% ───────────────────────────── ████████ │
   50% ───────────────────── ████████       │ │
   25% ───────────── ████████             │ │ │
    0% ────── ████████                   │ │ │ │
         ▲                                   ▲
     fade_start()                       fade zakończony
                                        (opcjonalnie: callback)

  CPU w tym czasie: WOLNY! Może robić inne rzeczy.
  Sprzęt sam zmienia duty cycle krok po kroku.
```

### 7.2 Sekwencja użycia fade

```
1. ledc_fade_func_install()      ← Jednorazowo! Instaluje serwis ISR
2. ledc_set_fade_with_time()     ← Konfiguracja: cel, czas
   LUB ledc_set_fade_with_step() ← Konfiguracja: cel, krok, interwał
3. ledc_fade_start()             ← Start fade (blocking lub non-blocking)
   ... fade trwa sprzętowo ...
4. (opcjonalnie) ledc_fade_func_uninstall() ← Zwolnienie zasobów
```

### 7.3 ledc_set_fade_with_time() — fade z czasem trwania

```c
#include "driver/ledc.h"

void fade_with_time_example(void)
{
    // ── Jednorazowa instalacja serwisu fade ──
    ESP_ERROR_CHECK(ledc_fade_func_install(0));  // 0 = domyślne flagi ISR

    // ── Fade z 0% do 100% w 2000 ms ──
    ESP_ERROR_CHECK(ledc_set_fade_with_time(
        LEDC_LOW_SPEED_MODE,    // Tryb
        LEDC_CHANNEL_0,         // Kanał
        8191,                   // Docelowe duty (100% przy 13-bit)
        2000                    // Czas fade w ms
    ));

    // ── Start fade (non-blocking) ──
    ESP_ERROR_CHECK(ledc_fade_start(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        LEDC_FADE_NO_WAIT       // Nie czekaj — CPU wolny
    ));

    ESP_LOGI(TAG, "Fade 0→100%% uruchomiony (2 sekundy)");
    // CPU może teraz robić inne rzeczy!
}
```

### 7.4 ledc_set_fade_with_step() — fade z krokiem

```c
void fade_with_step_example(void)
{
    // Fade z 0% do 100% z krokiem 100 co 50 ms
    // (krok = o ile zmienia się duty, interwał = co ile cykli PWM)
    ESP_ERROR_CHECK(ledc_set_fade_with_step(
        LEDC_LOW_SPEED_MODE,    // Tryb
        LEDC_CHANNEL_0,         // Kanał
        8191,                   // Docelowe duty
        100,                    // scale — krok zmiany duty
        10                      // cycle_num — co ile cykli PWM zmiana
    ));

    ESP_ERROR_CHECK(ledc_fade_start(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        LEDC_FADE_WAIT_DONE     // Czekaj na zakończenie (blocking)
    ));

    ESP_LOGI(TAG, "Fade zakończony!");
}
```

### 7.5 Parametry ledc_set_fade_with_time()

```c
esp_err_t ledc_set_fade_with_time(
    ledc_mode_t    speed_mode,     // LEDC_LOW_SPEED_MODE / HIGH
    ledc_channel_t channel,        // LEDC_CHANNEL_0 – 7
    uint32_t       target_duty,    // Docelowa wartość duty
    int            max_fade_time_ms // Czas fade w milisekundach
);
```

### 7.6 Tryby startu fade

```c
// NON-BLOCKING — natychmiastowy powrót, fade w tle
ledc_fade_start(mode, channel, LEDC_FADE_NO_WAIT);
// CPU jest wolny! Fade trwa sprzętowo.
// Użyj callback'a aby dowiedzieć się o zakończeniu.

// BLOCKING — czeka na zakończenie fade
ledc_fade_start(mode, channel, LEDC_FADE_WAIT_DONE);
// Funkcja wraca dopiero gdy fade się zakończy.
// Prostsze w użyciu, ale blokuje task.
```

> **⚠️ Ważne:** Nie można zatrzymać fade po jego uruchomieniu — sprzęt musi dojść do docelowej wartości. Kolejna operacja fade lub zmiana duty czeka na zakończenie bieżącego fade.

---

## 8. Tryby szybki i wolny (High / Low Speed)

### 8.1 Różnice między trybami

```
HIGH-SPEED MODE:
  ✅ Sprzętowa zmiana ustawień timera — glitch-free
  ✅ Zmiana parametrów aplikowana przy następnym overflow timera
  ✅ Gwarantuje płynne przejście bez artefaktów na wyjściu PWM

LOW-SPEED MODE:
  ⚠️ Zmiana ustawień wymaga software trigger (driver robi to automatycznie)
  ✅ Może używać RC_FAST_CLK (kompatybilny z light-sleep)
  ✅ Wystarczający do większości zastosowań LED

Dla LED dimming → LOW-SPEED MODE jest w 100% wystarczający
Dla precyzyjnych sygnałów z dynamiczną zmianą f → HIGH-SPEED MODE
```

### 8.2 Wybór trybu w praktyce

```c
// Dla 99% zastosowań LED — użyj LOW-SPEED MODE
#define LEDC_MODE  LEDC_LOW_SPEED_MODE

// Dla precyzyjnych sygnałów z dynamiczną zmianą parametrów
#define LEDC_MODE  LEDC_HIGH_SPEED_MODE
```

> **💡 Wskazówka:** W praktyce dla sterowania LED nie ma widocznej różnicy między trybami. `LEDC_LOW_SPEED_MODE` jest najpopularniejszym i zalecanym wyborem.

---

## 9. Zaawansowane: Power Management, przerwania, callback fade

### 9.1 Callback zakończenia fade

```c
#include "driver/ledc.h"

// Callback wywoływany po zakończeniu fade — kontekst ISR!
static IRAM_ATTR bool fade_end_cb(const ledc_cb_param_t *param, void *user_arg)
{
    BaseType_t high_task_awoken = pdFALSE;
    TaskHandle_t task = (TaskHandle_t)user_arg;

    // Powiadom task o zakończeniu fade
    vTaskNotifyGiveFromISR(task, &high_task_awoken);

    return (high_task_awoken == pdTRUE);
}

void setup_fade_callback(void)
{
    ledc_cbs_t callbacks = {
        .fade_cb = fade_end_cb,
    };

    ESP_ERROR_CHECK(ledc_cb_register(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        &callbacks,
        (void *)xTaskGetCurrentTaskHandle()   // user_arg
    ));
}
```

### 9.2 Power Management i Sleep

```c
// Domyślne zachowanie w light-sleep: PWM zatrzymany, ale LEDC nie wyłączony
// Konfiguracja sleep_mode w ledc_channel_config_t:

ledc_channel_config_t chan_cfg = {
    // ... inne pola ...
    .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,  // Domyślny: PWM off w sleep
    // .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE,    // PWM działa w sleep!
};
```

> **💡 KEEP_ALIVE** wymaga źródła zegara kompatybilnego z light-sleep (np. `RC_FAST_CLK` w grupie LS). Koszt: wyższy pobór prądu w sleep.

---

## 10. Ćwiczenie 1: Breathing LED — płynne rozjaśnianie/ściemnianie

### 10.1 Cel ćwiczenia

Implementacja efektu **breathing LED** — LED płynnie rozjaśnia się od 0% do 100%, a następnie płynnie ściemnia z 100% do 0%, w nieskończonej pętli. Cały efekt realizowany **sprzętowo** (hardware fade) — CPU jest wolny!

### 10.2 Schemat podłączenia

```
ESP32 NodeMCU-32
┌──────────────┐
│              │
│     GPIO2    ├──────── [LED 🔴] ──── [330Ω] ──── GND
│              │
│     GND      ├──────────────────────────────────── GND
└──────────────┘

Elementy:
  • 1× LED (dowolny kolor)
  • 1× Rezystor 220–470 Ω
  • Kable połączeniowe
```

### 10.3 Kompletny kod

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

// ═══════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════
#define LEDC_GPIO           GPIO_NUM_2
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT     // 8192 poziomów
#define LEDC_FREQUENCY      5000                   // 5 kHz
#define LEDC_DUTY_MAX       ((1 << 13) - 1)        // 8191
#define FADE_TIME_MS        2000                   // Czas jednego fade: 2s

static const char *TAG = "BREATHING_LED";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.4: Breathing LED (Hardware Fade) ===");

    // ═══ KROK 1: Konfiguracja timera LEDC ═══
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));
    ESP_LOGI(TAG, "Timer: %d Hz, %d-bit rozdzielczość", LEDC_FREQUENCY, 13);

    // ═══ KROK 2: Konfiguracja kanału LEDC ═══
    ledc_channel_config_t chan_cfg = {
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = LEDC_GPIO,
        .duty       = 0,           // Start: LED zgaszony
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_cfg));
    ESP_LOGI(TAG, "Kanał: GPIO%d, duty=0", LEDC_GPIO);

    // ═══ KROK 3: Instalacja serwisu fade ═══
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    ESP_LOGI(TAG, "Serwis fade zainstalowany");

    // ═══ KROK 4: Nieskończona pętla breathing ═══
    ESP_LOGI(TAG, "Breathing LED uruchomiony!");
    ESP_LOGI(TAG, "Fade time: %d ms (cykl: %d ms)", FADE_TIME_MS, FADE_TIME_MS * 2);

    uint32_t cycle = 0;
    while (1) {
        cycle++;

        // ── Fade UP: 0% → 100% ──
        ESP_ERROR_CHECK(ledc_set_fade_with_time(
            LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MAX, FADE_TIME_MS));
        ESP_ERROR_CHECK(ledc_fade_start(
            LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_WAIT_DONE));

        // ── Fade DOWN: 100% → 0% ──
        ESP_ERROR_CHECK(ledc_set_fade_with_time(
            LEDC_MODE, LEDC_CHANNEL, 0, FADE_TIME_MS));
        ESP_ERROR_CHECK(ledc_fade_start(
            LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_WAIT_DONE));

        ESP_LOGI(TAG, "Cykl breathing #%lu zakończony", (unsigned long)cycle);
    }

    // Nigdy nie dotrze tutaj, ale dla porządku:
    // ledc_fade_func_uninstall();
}
```

### 10.4 Wyjaśnienie działania

```
Cykl breathing LED (4 sekundy):

  Jasność
  100% ──────────╱╲──────────╱╲──────────
                ╱  ╲        ╱  ╲
               ╱    ╲      ╱    ╲
              ╱      ╲    ╱      ╲
    0% ──────╱        ╲──╱        ╲──────
         │← 2s →│← 2s →│← 2s →│← 2s →│
         │  UP   │ DOWN  │  UP   │ DOWN  │
         └───── cykl 1 ──┘───── cykl 2 ──┘

  LEDC_FADE_WAIT_DONE → blokuje task do końca fade
  → Prostota kodu: UP, czekaj, DOWN, czekaj, powtórz

  CPU: Zablokowany na ledc_fade_start() podczas fade.
       Ale sam fade jest SPRZĘTOWY — CPU nie przełącza GPIO!
```

### 10.5 Wersja non-blocking z callback'iem

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#define LEDC_GPIO       GPIO_NUM_2
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_DUTY_RES   LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY  5000
#define LEDC_DUTY_MAX   ((1 << 13) - 1)
#define FADE_TIME_MS    2000

static const char *TAG = "BREATHING_NB";
static SemaphoreHandle_t fade_sem = NULL;

// Callback ISR — wywoływany po zakończeniu fade
static IRAM_ATTR bool fade_end_cb(const ledc_cb_param_t *param, void *user_arg)
{
    BaseType_t high_task_awoken = pdFALSE;
    SemaphoreHandle_t sem = (SemaphoreHandle_t)user_arg;
    xSemaphoreGiveFromISR(sem, &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Breathing LED (Non-blocking + Callback) ===");

    // Timer
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_MODE, .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES, .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    // Kanał
    ledc_channel_config_t chan_cfg = {
        .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER, .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_GPIO, .duty = 0, .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_cfg));

    // Fade service + callback
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    fade_sem = xSemaphoreCreateBinary();

    ledc_cbs_t cbs = { .fade_cb = fade_end_cb };
    ESP_ERROR_CHECK(ledc_cb_register(LEDC_MODE, LEDC_CHANNEL, &cbs, fade_sem));

    // Breathing loop — non-blocking
    while (1) {
        // Fade UP
        ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MAX, FADE_TIME_MS);
        ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);

        // CPU jest wolny! Może robić inne rzeczy tutaj...
        ESP_LOGI(TAG, "Fade UP — CPU wolny");

        // Czekaj na callback
        xSemaphoreTake(fade_sem, portMAX_DELAY);

        // Fade DOWN
        ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 0, FADE_TIME_MS);
        ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);

        ESP_LOGI(TAG, "Fade DOWN — CPU wolny");
        xSemaphoreTake(fade_sem, portMAX_DELAY);
    }
}
```

---

## 11. Ćwiczenie 2: Sterowanie jasnością LED potencjometrem (ADC + LEDC)

### 11.1 Cel

Odczyt napięcia z potencjometru (ADC) i mapowanie go na duty cycle LEDC — obracanie gałki płynnie zmienia jasność LED.

### 11.2 Schemat

```
ESP32 NodeMCU-32
┌──────────────┐         Potencjometr 10kΩ
│              │         ┌────────────┐
│     3.3V     ├─────────┤  VCC       │
│     GPIO34   ├─────────┤  WIPER     │  (ADC1_CH6)
│     GND      ├─────────┤  GND       │
│              │         └────────────┘
│     GPIO2    ├──── [LED 🔴] ──── [330Ω] ──── GND
└──────────────┘
```

### 11.3 Kompletny kod

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#define LED_GPIO        GPIO_NUM_2
#define POT_ADC_CHANNEL ADC_CHANNEL_6     // GPIO34
#define LEDC_DUTY_MAX   ((1 << 13) - 1)   // 8191

static const char *TAG = "ADC_LED";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Sterowanie LED potencjometrem ===");

    // ── LEDC setup ──
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT, .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t chan = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0, .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_GPIO, .duty = 0, .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan));

    // ── ADC setup ──
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t adc_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t adc_chan_cfg = {
        .atten = ADC_ATTEN_DB_12,       // 0–3.3V
        .bitwidth = ADC_BITWIDTH_12,     // 12-bit (0–4095)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, POT_ADC_CHANNEL, &adc_chan_cfg));

    // ── Pętla główna ──
    while (1) {
        int adc_raw;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, POT_ADC_CHANNEL, &adc_raw));

        // Mapuj ADC (0–4095) na duty (0–8191)
        uint32_t duty = (uint32_t)adc_raw * LEDC_DUTY_MAX / 4095;

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ESP_LOGI(TAG, "ADC: %d → Duty: %lu (%lu%%)",
                 adc_raw, (unsigned long)duty,
                 (unsigned long)(duty * 100 / LEDC_DUTY_MAX));

        vTaskDelay(pdMS_TO_TICKS(50));   // 20 odczytów/s
    }
}
```

---

## 12. Podsumowanie i dalsze kroki

### 12.1 Co opanowałeś w tym module

```
✅ Architektura LEDC: 8 timerów + 16 kanałów (HS/LS)
✅ Konfiguracja timera: częstotliwość, rozdzielczość, źródło zegara
✅ Konfiguracja kanału: GPIO, timer, duty cycle
✅ Zależność: częstotliwość ↔ rozdzielczość
✅ Zmiana duty cycle programowo: ledc_set_duty() + ledc_update_duty()
✅ Hardware fade: ledc_set_fade_with_time(), ledc_fade_start()
✅ Tryby fade: blocking (WAIT_DONE) vs non-blocking (NO_WAIT)
✅ Callback zakończenia fade: ledc_cb_register()
✅ Tryby HS vs LS (glitch-free vs software-triggered)
✅ Praktyka: Breathing LED, sterowanie potencjometrem
```

### 12.2 Dalsze kroki

```
Moduł 1.5 → Sigma-Delta Modulation
Moduł 1.6 → DAC (Digital-to-Analog Converter)
Moduł 2.x → ADC (Analog-to-Digital Converter) — pogłębienie
```

---

## 13. Źródła i dokumentacja

### 13.1 Oficjalna dokumentacja ESP-IDF

| Zasób | Link |
|-------|------|
| **LEDC API Reference** | [docs.espressif.com — LEDC](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/ledc.html) |
| **LEDC Basic Example** | [github.com/espressif — ledc_basic](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/ledc/ledc_basic) |
| **LEDC Fade Example** | [github.com/espressif — ledc_fade](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/ledc/ledc_fade) |
| **ESP32 TRM — LED PWM** | esp32_technical_reference_manual_en.pdf, rozdział "LED PWM Controller" |

### 13.2 Dokumentacja w workspace

| Plik | Opis |
|------|------|
| `esp32_technical_reference_manual_en.pdf` | Szczegółowy opis rejestrów LEDC |
| `esp32_datasheet_en.pdf` | Specyfikacja ESP32 |
| `esp32-wrover-b_datasheet_en.pdf` | Specyfikacja modułu WROVER-B |

### 13.3 Przydatne wzory

```
Częstotliwość PWM:
  freq_hz = clk_src_hz / (2^duty_resolution × prescaler)

Maksymalna rozdzielczość:
  max_bits = floor(log2(clk_src_hz / freq_hz))

Duty cycle w procentach:
  duty_percent = duty_value / (2^duty_resolution) × 100%

Czas fade (przybliżony):
  Sprzęt dzieli przejście na kroki proporcjonalnie do czasu
```
