# Moduł 1.2 — General Purpose Timer (GPTimer)

> **Poziom:** 🟢 Laik · **Czas:** Tydzień 3–6 (Faza 1)  
> **Cel:** Opanowanie sprzętowych timerów ESP32 — konfiguracja, prescaler, alarm, auto-reload, callback w kontekście ISR, oraz praktyczne ćwiczenia z generowaniem sygnałów.

---

## Spis treści

1. [Architektura timerów w ESP32](#1-architektura-timerów-w-esp32)
2. [API GPTimer — przegląd](#2-api-gptimer--przegląd)
3. [Tworzenie timera — gptimer_new_timer()](#3-tworzenie-timera--gptimer_new_timer)
4. [Prescaler i rozdzielczość](#4-prescaler-i-rozdzielczość)
5. [Alarm — wyzwalanie zdarzeń](#5-alarm--wyzwalanie-zdarzeń)
6. [Auto-reload — periodyczne alarmy](#6-auto-reload--periodyczne-alarmy)
7. [Callback timera w kontekście ISR](#7-callback-timera-w-kontekście-isr)
8. [Cykl życia timera — maszyna stanów](#8-cykl-życia-timera--maszyna-stanów)
9. [Zaawansowane: IRAM Safe, Power Management](#9-zaawansowane-iram-safe-power-management)
10. [Ćwiczenie 1: Precyzyjne odmierzanie czasu](#10-ćwiczenie-1-precyzyjne-odmierzanie-czasu)
11. [Ćwiczenie 2: Generowanie sygnału prostokątnego](#11-ćwiczenie-2-generowanie-sygnału-prostokątnego)
12. [Podsumowanie i dalsze kroki](#12-podsumowanie-i-dalsze-kroki)
13. [Źródła i dokumentacja](#13-źródła-i-dokumentacja)

---

## 1. Architektura timerów w ESP32

### 1.1 Czym jest timer sprzętowy?

**Timer sprzętowy** to peryferium mikrokontrolera, które liczy takty zegara niezależnie od CPU. Działa autonomicznie — CPU może wykonywać inne zadania, a timer precyzyjnie odmierza czas i wyzwala przerwania.

**Kluczowe fakty:**
- ESP32 posiada **2 grupy timerów** (Timer Group 0 i Timer Group 1)
- Każda grupa zawiera **2 timery** — łącznie **4 niezależne timery 64-bitowe**
- Każdy timer ma **16-bitowy prescaler** (dzielnik częstotliwości)
- Licznik 64-bitowy pozwala na odmierzanie **bardzo długich** okresów czasu
- Timery mogą liczyć **w górę** lub **w dół**
- Źródło zegara: **APB Clock (80 MHz)** przy CPU 160/240 MHz

### 1.2 Organizacja grup timerów

```
ESP32 — Timer Groups
═══════════════════════════════════════════════════════

  ┌─────────────────────────────────┐
  │       Timer Group 0             │
  │  ┌────────────┐ ┌────────────┐  │
  │  │  Timer 0   │ │  Timer 1   │  │
  │  │  64-bit    │ │  64-bit    │  │
  │  │  counter   │ │  counter   │  │
  │  │  16-bit    │ │  16-bit    │  │
  │  │  prescaler │ │  prescaler │  │
  │  └────────────┘ └────────────┘  │
  └─────────────────────────────────┘

  ┌─────────────────────────────────┐
  │       Timer Group 1             │
  │  ┌────────────┐ ┌────────────┐  │
  │  │  Timer 0   │ │  Timer 1   │  │
  │  │  64-bit    │ │  64-bit    │  │
  │  │  counter   │ │  counter   │  │
  │  │  16-bit    │ │  16-bit    │  │
  │  │  prescaler │ │  prescaler │  │
  │  └────────────┘ └────────────┘  │
  └─────────────────────────────────┘

  Źródło zegara: APB Clock (80 MHz)
  Prescaler: 16-bit (dzielnik 2–65536)
  Licznik: 64-bit (0 do 2^64-1)
```

### 1.3 Schemat blokowy pojedynczego timera

```
                    ┌──────────────┐
  APB Clock ────────┤  Prescaler   ├──────┐
  (80 MHz)          │  (16-bit)    │      │
                    └──────────────┘      │
                                          ▼
                              ┌───────────────────┐
                              │  64-bit Counter   │
                              │                   │
                              │  Kierunek:        │
                              │  ↑ COUNT_UP       │
                              │  ↓ COUNT_DOWN     │
                              └─────────┬─────────┘
                                        │
                                 Komparator
                                        │
                              ┌─────────▼─────────┐
                              │   Alarm Logic      │
                              │                    │
                              │  alarm_count == ?  │──── Przerwanie (ISR)
                              │  auto_reload?      │
                              └────────────────────┘
```

### 1.4 Timer sprzętowy vs vTaskDelay()

| Cecha | Timer sprzętowy (GPTimer) | vTaskDelay() |
|-------|--------------------------|--------------|
| **Precyzja** | ≤ 1 µs (rozdzielczość MHz) | ~1 ms (zależy od tick rate) |
| **Obciążenie CPU** | Zerowe — działa sprzętowo | Minimalne — task śpi |
| **Callback/ISR** | ✅ Tak — natychmiastowa reakcja | ❌ Nie — wymaga pętli w tasku |
| **Jitter** | Bardzo niski (~ns) | Wysoki (~ms, zależy od schedulera) |
| **Zastosowanie** | Precyzyjne PWM, pomiary, protokoły | Opóźnienia w UI, miganie LED |
| **Ilość** | 4 (ograniczone sprzętowo) | Nieograniczona (programowe) |

> **💡 Kiedy używać GPTimer?** Gdy potrzebujesz precyzji poniżej 1 ms, deterministycznego czasu reakcji, lub generowania sygnałów o dokładnej częstotliwości. Do prostego migania LED wystarczy `vTaskDelay()`.

---

## 2. API GPTimer — przegląd

### 2.1 Nagłówek i CMake

```c
// Główny nagłówek — zawiera wszystkie funkcje GPTimer
#include "driver/gptimer.h"
```

W pliku `CMakeLists.txt` komponent `driver` jest dołączany automatycznie w standardowych projektach ESP-IDF.

### 2.2 Kluczowe funkcje API

```
Funkcja                              Opis
──────────────────────────────────── ──────────────────────────────────────
gptimer_new_timer()                  Tworzenie nowej instancji timera
gptimer_del_timer()                  Usunięcie timera (zwolnienie zasobów)
gptimer_set_raw_count()              Ustawienie wartości licznika
gptimer_get_raw_count()              Odczyt bieżącej wartości licznika
gptimer_set_alarm_action()           Konfiguracja alarmu
gptimer_register_event_callbacks()   Rejestracja callback'ów (ISR)
gptimer_enable()                     Włączenie timera (init → enable)
gptimer_disable()                    Wyłączenie timera (enable → init)
gptimer_start()                      Start liczenia (enable → run)
gptimer_stop()                       Stop liczenia (run → enable)
```

### 2.3 Kluczowe struktury

| Struktura | Opis |
|-----------|------|
| `gptimer_config_t` | Konfiguracja timera (zegar, prescaler, kierunek) |
| `gptimer_alarm_config_t` | Konfiguracja alarmu (próg, reload, auto-reload) |
| `gptimer_event_callbacks_t` | Struktura z callback'ami (on_alarm) |
| `gptimer_alarm_event_data_t` | Dane zdarzenia przekazywane do callback'a |

### 2.4 Typ uchwytu

```c
// Uchwyt timera — opaque pointer zarządzany przez driver
gptimer_handle_t gptimer = NULL;

// Po utworzeniu timera, wszystkie operacje używają tego uchwytu
gptimer_new_timer(&config, &gptimer);    // Tworzenie
gptimer_enable(gptimer);                  // Włączenie
gptimer_start(gptimer);                   // Start
gptimer_stop(gptimer);                    // Stop
gptimer_disable(gptimer);                 // Wyłączenie
gptimer_del_timer(gptimer);              // Usunięcie
```

---

## 3. Tworzenie timera — gptimer_new_timer()

### 3.1 Struktura konfiguracyjna

```c
#include "driver/gptimer.h"

void create_timer_example(void)
{
    // Konfiguracja timera
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,   // Źródło zegara (APB 80MHz)
        .direction = GPTIMER_COUNT_UP,          // Kierunek liczenia
        .resolution_hz = 1000000,               // Rozdzielczość 1 MHz = 1 µs/tick
    };

    // Uchwyt timera
    gptimer_handle_t gptimer = NULL;

    // Utworzenie instancji timera
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_LOGI(TAG, "Timer utworzony pomyślnie");
}
```

### 3.2 Pola gptimer_config_t

| Pole | Typ | Opis |
|------|-----|------|
| `clk_src` | `gptimer_clock_source_t` | Źródło zegara timera |
| `direction` | `gptimer_count_direction_t` | Kierunek zliczania |
| `resolution_hz` | `uint32_t` | Żądana rozdzielczość w Hz |
| `intr_priority` | `int` | Priorytet przerwania (0 = domyślny) |
| `flags.intr_shared` | `bool` | Czy przerwanie jest współdzielone |
| `flags.allow_pd` | `bool` | Czy zezwolić na power-down w light sleep |

### 3.3 Źródła zegara (gptimer_clock_source_t)

```
Źródło                         Częstotliwość    Uwagi
────────────────────────────── ──────────────── ─────────────────────────
GPTIMER_CLK_SRC_DEFAULT        80 MHz (APB)     Domyślne, najpopularniejsze
GPTIMER_CLK_SRC_APB            80 MHz           To samo co DEFAULT na ESP32
GPTIMER_CLK_SRC_XTAL           40 MHz           Kryształ, stabilne w DFS
```

> **💡 Wskazówka:** Jeśli używasz Dynamic Frequency Scaling (DFS), wybierz `GPTIMER_CLK_SRC_XTAL` — częstotliwość kryształu nie zmienia się ze skalowaniem CPU, więc timer będzie stabilny.

### 3.4 Kierunek zliczania

```c
typedef enum {
    GPTIMER_COUNT_DOWN,   // Liczenie w dół (od wartości → 0)
    GPTIMER_COUNT_UP,     // Liczenie w górę (od 0 → wartości)
} gptimer_count_direction_t;
```

```
COUNT_UP:                          COUNT_DOWN:
  ▲ wartość                          ▲ wartość
  │      ╱╲      ╱╲                  │╲      ╱╲      ╱
  │    ╱    ╲  ╱    ╲              │  ╲    ╱  ╲    ╱
  │  ╱      ╲╱      ╲             │    ╲╱      ╲╱
  │╱                               │
  └──────────────► czas            └──────────────► czas
     alarm ↑   reload              alarm ↑   reload
     (max)     (0)                 (0)       (max)
```

### 3.5 Rozdzielczość — co to znaczy?

Parametr `resolution_hz` określa, ile "ticków" timera odpowiada jednej sekundzie. Driver automatycznie dobiera prescaler, aby uzyskać żądaną rozdzielczość.

```c
// resolution_hz = 1 000 000 (1 MHz)
// → 1 tick = 1 / 1 000 000 s = 1 µs
// → alarm_count = 1 000 000 → alarm co 1 sekundę

// resolution_hz = 10 000 000 (10 MHz)
// → 1 tick = 1 / 10 000 000 s = 100 ns
// → alarm_count = 10 000 000 → alarm co 1 sekundę

// resolution_hz = 1 000 (1 kHz)
// → 1 tick = 1 / 1 000 s = 1 ms
// → alarm_count = 1 000 → alarm co 1 sekundę
```

> **⚠️ Ważne:** Prescaler jest 16-bitowy, więc minimalna rozdzielczość z APB 80 MHz to ~1221 Hz (80 000 000 / 65536). Maksymalna rozdzielczość to 80 MHz (prescaler = 1, ale praktycznie driver może ograniczać).

---

## 4. Prescaler i rozdzielczość

### 4.1 Jak działa prescaler?

**Prescaler** to dzielnik częstotliwości — zmniejsza częstotliwość zegara wejściowego, zanim trafi do licznika.

```
APB Clock (80 MHz) ──┤ Prescaler (÷N) ├──► Timer Counter
                      │                │
                      │ N = 1...65536  │
                      └────────────────┘

Częstotliwość timera = APB_CLK / prescaler
Okres jednego ticka  = prescaler / APB_CLK

Przykłady:
  Prescaler = 80    → 80 MHz / 80   = 1 MHz    → 1 tick = 1 µs
  Prescaler = 8     → 80 MHz / 8    = 10 MHz   → 1 tick = 100 ns
  Prescaler = 800   → 80 MHz / 800  = 100 kHz  → 1 tick = 10 µs
  Prescaler = 80000 → ⛔ NIEMOŻLIWE! (max 65536)
```

### 4.2 Automatyczny dobór prescalera

W nowym API ESP-IDF **nie ustawiasz prescalera ręcznie** — podajesz żądaną `resolution_hz`, a driver sam oblicza optymalny prescaler:

```c
// Chcesz rozdzielczość 1 µs → podajesz 1 MHz
gptimer_config_t config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,   // APB = 80 MHz
    .resolution_hz = 1000000,              // 1 MHz
    .direction = GPTIMER_COUNT_UP,
};
// Driver oblicza: prescaler = 80 MHz / 1 MHz = 80
```

### 4.3 Obliczanie okresu alarmu

```
Okres alarmu [s] = alarm_count × (1 / resolution_hz)
Okres alarmu [s] = alarm_count / resolution_hz

Przykłady (resolution_hz = 1 MHz = 1 000 000):
  alarm_count = 1 000 000   → 1.0 s
  alarm_count = 500 000     → 0.5 s    = 500 ms
  alarm_count = 100 000     → 0.1 s    = 100 ms
  alarm_count = 1 000       → 0.001 s  = 1 ms
  alarm_count = 100         → 0.0001 s = 100 µs
  alarm_count = 1           → 0.000001 s = 1 µs (MIN)
```

> **⚠️ Minimalna wartość alarmu:** Ze względu na opóźnienie przerwań (interrupt latency), nie zaleca się ustawiania okresu alarmu poniżej **5 µs** (`alarm_count < 5` przy `resolution_hz = 1 MHz`).

---

## 5. Alarm — wyzwalanie zdarzeń

### 5.1 Czym jest alarm timera?

**Alarm** to mechanizm, w którym timer porównuje swoją bieżącą wartość z ustawioną wartością docelową (`alarm_count`). Gdy licznik osiąga tę wartość, generowane jest **zdarzenie alarmowe** — może ono wywołać przerwanie i callback.

```
Licznik timera:    0 ───────────────► alarm_count
                                           │
                                     ┌─────▼─────┐
                                     │ ALARM!    │
                                     │ → ISR     │
                                     │ → Callback│
                                     └───────────┘
```

### 5.2 Konfiguracja alarmu

```c
#include "driver/gptimer.h"

void setup_alarm_example(gptimer_handle_t gptimer)
{
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 1000000,                    // Alarm po 1 000 000 ticków
        .reload_count = 0,                         // Po alarmie reload do 0
        .flags.auto_reload_on_alarm = true,        // Auto-reload włączony
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    ESP_LOGI(TAG, "Alarm skonfigurowany: co 1s (przy 1 MHz)");
}
```

### 5.3 Pola gptimer_alarm_config_t

| Pole | Typ | Opis |
|------|-----|------|
| `alarm_count` | `uint64_t` | Wartość licznika wyzwalająca alarm |
| `reload_count` | `uint64_t` | Wartość wczytywana po alarmie (gdy auto-reload) |
| `flags.auto_reload_on_alarm` | `bool` | Czy automatycznie przeładować licznik |

### 5.4 Tryby alarmów

```
═══════════════════════════════════════════════════════
Tryb 1: ONE-SHOT (jednorazowy)
═══════════════════════════════════════════════════════
  auto_reload = false

  Licznik:  0 ────────────────► alarm_count ──────────► ∞
                                     │
                                   ALARM!
                           (timer dalej liczy)

═══════════════════════════════════════════════════════
Tryb 2: PERIODIC (auto-reload)
═══════════════════════════════════════════════════════
  auto_reload = true

  Licznik:  0 ──► alarm_count → 0 ──► alarm_count → 0 ──►
                       │                    │
                     ALARM!               ALARM!
                  (reload do 0)        (reload do 0)
```

### 5.5 Wyłączenie alarmu

```c
// Przekazanie NULL wyłącza alarm — timer liczy jako "zegar ścienny"
ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, NULL));
```

### 5.6 Dynamiczna zmiana alarmu z callback'a (one-shot chain)

```c
// W callback'u możesz zmienić alarm na nową wartość:
static bool IRAM_ATTR timer_alarm_cb(gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    // Ustaw nowy alarm — next alarm za 500ms od teraz
    gptimer_alarm_config_t new_alarm = {
        .alarm_count = edata->count_value + 500000,  // +500ms @ 1MHz
        .flags.auto_reload_on_alarm = false,
    };
    gptimer_set_alarm_action(timer, &new_alarm);
    return false;
}
```

---

## 6. Auto-reload — periodyczne alarmy

### 6.1 Mechanizm auto-reload

Gdy `auto_reload_on_alarm = true`, po osiągnięciu `alarm_count` licznik jest **natychmiast** (sprzętowo!) przeładowywany wartością `reload_count`. Zapewnia to precyzyjną periodyczność bez udziału CPU.

```
  reload_count = 0
  alarm_count  = 1000  (= 1 ms przy 1 MHz)

  ▲ licznik
  │
  1000 ─────╱│─────╱│─────╱│─────╱│
  │        ╱ │    ╱ │    ╱ │    ╱ │
  │      ╱   │  ╱   │  ╱   │  ╱   │
  │    ╱     │╱     │╱     │╱     │
  0 ──╱──────╱──────╱──────╱──────╱──► czas
      │ 1ms  │ 1ms  │ 1ms  │ 1ms
    ALARM! ALARM! ALARM! ALARM!
```

### 6.2 Kompletny przykład: periodyczny alarm co 500 ms

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gptimer.h"
#include "esp_log.h"

static const char *TAG = "GPTIMER_PERIODIC";

// Struktura zdarzenia przekazywanego przez kolejkę
typedef struct {
    uint64_t event_count;
} timer_event_t;

// Kolejka do komunikacji ISR → Task
static QueueHandle_t timer_queue = NULL;

// ══════════════════════════════════════════════════
// Callback alarmu — wykonywany w kontekście ISR!
// ══════════════════════════════════════════════════
static bool IRAM_ATTR timer_alarm_cb(gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    // Przygotuj dane zdarzenia
    timer_event_t evt = {
        .event_count = edata->count_value,
    };

    // Wyślij do kolejki — task obsłuży resztę
    xQueueSendFromISR(queue, &evt, &high_task_awoken);

    // Zwróć true jeśli trzeba przełączyć kontekst
    return (high_task_awoken == pdTRUE);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.2: Periodyczny alarm GPTimer ===");

    // 1. Utwórz kolejkę
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));

    // 2. Konfiguracja i tworzenie timera
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,   // 1 MHz → 1 tick = 1 µs
    };
    gptimer_handle_t gptimer = NULL;
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    // 3. Konfiguracja alarmu — co 500 ms
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 500000,                     // 500 000 µs = 500 ms
        .reload_count = 0,                         // Reload do 0
        .flags.auto_reload_on_alarm = true,        // Auto-reload
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    // 4. Rejestracja callback'a
    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, timer_queue));

    // 5. Włączenie i start timera
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    ESP_LOGI(TAG, "Timer uruchomiony — alarm co 500 ms");

    // 6. Pętla obsługi zdarzeń
    timer_event_t evt;
    uint32_t alarm_count = 0;
    while (1) {
        if (xQueueReceive(timer_queue, &evt, portMAX_DELAY)) {
            alarm_count++;
            ESP_LOGI(TAG, "Alarm #%lu | count = %llu",
                     (unsigned long)alarm_count,
                     (unsigned long long)evt.event_count);
        }
    }
}
```

### 6.3 Ważne ograniczenie

> **⚠️ `alarm_count` ≠ `reload_count` gdy auto-reload jest włączony!**
>
> ```c
> // ❌ BŁĄD — alarm_count = reload_count przy auto-reload
> gptimer_alarm_config_t bad = {
>     .alarm_count = 1000,
>     .reload_count = 1000,       // ← To samo! Bezsensowne!
>     .flags.auto_reload_on_alarm = true,
> };
>
> // ✅ POPRAWNIE
> gptimer_alarm_config_t good = {
>     .alarm_count = 1000,
>     .reload_count = 0,          // ← Różne od alarm_count
>     .flags.auto_reload_on_alarm = true,
> };
```

---

## 7. Callback timera w kontekście ISR

### 7.1 Rejestracja callback'a

Callback rejestrujemy **przed** wywołaniem `gptimer_enable()`. Funkcja `gptimer_register_event_callbacks()` instaluje serwis przerwań, ale go nie aktywuje — aktywacja następuje dopiero przy `gptimer_enable()`.

```c
#include "driver/gptimer.h"

// ══════════════════════════════════════════════════
// Prototyp callback'a alarmu
// ══════════════════════════════════════════════════
// Parametry:
//   timer     - uchwyt timera, który wyzwolił alarm
//   edata     - dane zdarzenia (count_value, alarm_value)
//   user_ctx  - kontekst użytkownika (np. uchwyt kolejki)
//
// Zwraca:
//   true  - jeśli wybudzono task o wyższym priorytecie (yield!)
//   false - nie ma potrzeby przełączania kontekstu
// ══════════════════════════════════════════════════

static bool IRAM_ATTR my_timer_alarm_cb(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    uint64_t count = edata->count_value;
    xQueueSendFromISR(queue, &count, &high_task_awoken);

    return (high_task_awoken == pdTRUE);
}

void register_callback_example(gptimer_handle_t gptimer, QueueHandle_t queue)
{
    // Struktura z callback'ami
    gptimer_event_callbacks_t cbs = {
        .on_alarm = my_timer_alarm_cb,    // Callback na alarm
    };

    // Rejestracja — queue jako user_data (kontekst)
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, queue));

    // Teraz można włączyć timer
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
}
```

### 7.2 Dane zdarzenia (gptimer_alarm_event_data_t)

```c
typedef struct {
    uint64_t count_value;   // Bieżąca wartość licznika w momencie alarmu
    uint64_t alarm_value;   // Wartość alarm_count, przy której wyzwolono
} gptimer_alarm_event_data_t;
```

### 7.3 Zasady pisania callback'a ISR timera

```c
// ═══════════════════════════════════════════════════
// Callback timera = kontekst ISR!
// Te same zasady co dla gpio_isr_handler
// ═══════════════════════════════════════════════════

// ✅ DOZWOLONE w callback'u:
static bool IRAM_ATTR good_timer_cb(gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    BaseType_t woken = pdFALSE;

    // ✅ Wysłanie do kolejki
    xQueueSendFromISR(queue, &data, &woken);

    // ✅ Danie semafora
    xSemaphoreGiveFromISR(sem, &woken);

    // ✅ Notyfikacja tasku
    vTaskNotifyGiveFromISR(task_handle, &woken);

    // ✅ Zmiana stanu GPIO
    gpio_set_level(LED_GPIO, 1);

    // ✅ Zmiana alarmu (dynamiczny alarm)
    gptimer_alarm_config_t new_alarm = { ... };
    gptimer_set_alarm_action(timer, &new_alarm);

    // ✅ Odczyt/zapis licznika
    uint64_t count;
    gptimer_get_raw_count(timer, &count);

    return (woken == pdTRUE);
}

// ❌ ZABRONIONE w callback'u:
static bool IRAM_ATTR bad_timer_cb(gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    // ❌ ESP_LOGI (mutex + UART = deadlock/crash)
    // ESP_LOGI(TAG, "alarm!");

    // ❌ printf
    // printf("tick\n");

    // ❌ malloc/free
    // void *p = malloc(100);

    // ❌ vTaskDelay (nie wolno czekać w ISR!)
    // vTaskDelay(pdMS_TO_TICKS(10));

    // ❌ Tworzenie/usuwanie timera
    // gptimer_del_timer(timer);

    return false;
}
```

### 7.4 Wzorzec ISR → Task (best practice)

```
┌──────────────────┐     Queue/Semafor     ┌──────────────────┐
│ Callback ISR     │ ──────────────────►   │ Worker Task      │
│ (krótki, szybki) │                       │ (pełna logika)   │
│                  │                       │                  │
│ • Set flag       │                       │ • ESP_LOGI()     │
│ • Send to queue  │                       │ • I2C read       │
│ • Toggle GPIO    │                       │ • WiFi send      │
│ • Return bool    │                       │ • LCD update     │
└──────────────────┘                       └──────────────────┘
     < 1 µs                                    Dowolny czas
```

### 7.5 Dlaczego IRAM_ATTR?

```
IRAM_ATTR — umieszcza funkcję w wewnętrznej RAM zamiast Flash

Problem bez IRAM_ATTR:
  ┌───────────────────────────────────┐
  │ Flash SPI ← zapis/odczyt danych  │──── SPI ZAJĘTY!
  │                                   │
  │ Timer ISR (na Flash) ← ????      │──── Nie mogę odczytać kodu!
  │                                   │     → CRASH lub hang
  └───────────────────────────────────┘

Rozwiązanie z IRAM_ATTR:
  ┌───────────────────────────────────┐
  │ Flash SPI ← zapis/odczyt danych  │──── SPI ZAJĘTY
  │                                   │
  │ IRAM ← Timer ISR (w RAM!)        │──── Działa niezależnie! ✅
  └───────────────────────────────────┘
```

> **💡 Wskazówka:** Włącz `CONFIG_GPTIMER_ISR_IRAM_SAFE` w menuconfig, aby driver automatycznie umieszczał ISR w IRAM i przechowywał obiekty w DRAM.

---

## 8. Cykl życia timera — maszyna stanów

### 8.1 Stany timera

```
                 gptimer_new_timer()
                        │
                        ▼
                   ┌─────────┐
          ┌───────│  INIT    │───────┐
          │       └─────────┘       │
          │            │            │
          │  gptimer_enable()       │  gptimer_del_timer()
          │            │            │
          │            ▼            │
          │       ┌─────────┐       │
          │       │ ENABLE  │       │
          │       └─────────┘       │
          │       │         │       │
          │  gptimer_start()│       │  gptimer_disable()
          │       │         │       │
          │       ▼         │       │
          │  ┌─────────┐   │       │
          │  │   RUN   │   │       │
          │  └─────────┘   │       │
          │       │         │       │
          │  gptimer_stop() │       │
          │       │         │       │
          │       ▼         │       │
          │  (→ ENABLE) ────┘       │
          │                         │
          └─── (← INIT) ───────────┘
```

### 8.2 Poprawna sekwencja operacji

```c
void timer_lifecycle_example(void)
{
    gptimer_handle_t gptimer = NULL;

    // ── FAZA 1: Tworzenie (→ INIT) ──
    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&config, &gptimer));

    // ── FAZA 2: Konfiguracja (w stanie INIT) ──
    gptimer_alarm_config_t alarm = {
        .alarm_count = 1000000,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm));

    gptimer_event_callbacks_t cbs = { .on_alarm = my_cb };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    // ── FAZA 3: Włączenie (INIT → ENABLE) ──
    ESP_ERROR_CHECK(gptimer_enable(gptimer));

    // ── FAZA 4: Start (ENABLE → RUN) ──
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    // ... timer działa ...

    // ── FAZA 5: Stop (RUN → ENABLE) ──
    ESP_ERROR_CHECK(gptimer_stop(gptimer));

    // ── FAZA 6: Wyłączenie (ENABLE → INIT) ──
    ESP_ERROR_CHECK(gptimer_disable(gptimer));

    // ── FAZA 7: Usunięcie (INIT → zwolniony) ──
    ESP_ERROR_CHECK(gptimer_del_timer(gptimer));
    gptimer = NULL;
}
```

### 8.3 Typowe błędy sekwencji

```c
// ❌ BŁĄD: start bez enable
gptimer_start(gptimer);  // ESP_ERR_INVALID_STATE!

// ❌ BŁĄD: rejestracja callback po enable
gptimer_enable(gptimer);
gptimer_register_event_callbacks(gptimer, &cbs, NULL);  // ESP_ERR_INVALID_STATE!

// ❌ BŁĄD: usunięcie bez disable
gptimer_stop(gptimer);
gptimer_del_timer(gptimer);  // ESP_ERR_INVALID_STATE! Trzeba najpierw disable!

// ✅ POPRAWNA sekwencja zatrzymania:
gptimer_stop(gptimer);       // RUN → ENABLE
gptimer_disable(gptimer);    // ENABLE → INIT
gptimer_del_timer(gptimer);  // INIT → zwolniony
```

---

## 9. Zaawansowane: IRAM Safe, Power Management

### 9.1 IRAM Safe (Kconfig)

```
menuconfig → Component config → Driver Configurations
            → GPTimer Configuration

  [*] Place GPTimer ISR function into IRAM
      → CONFIG_GPTIMER_ISR_IRAM_SAFE

  [*] Place GPTimer control functions into IRAM
      → CONFIG_GPTIMER_CTRL_FUNC_IN_IRAM
```

Gdy `CONFIG_GPTIMER_ISR_IRAM_SAFE` jest włączone:
- ISR działa nawet gdy cache jest wyłączony (np. zapis do Flash)
- Funkcje ISR automatycznie umieszczone w IRAM
- Obiekty drivera umieszczone w DRAM (nie w PSRAM)
- **Koszt:** większe zużycie IRAM

Gdy `CONFIG_GPTIMER_CTRL_FUNC_IN_IRAM` jest włączone, funkcje sterujące działają z wyłączonym cache:
- `gptimer_start()`, `gptimer_stop()`
- `gptimer_get_raw_count()`, `gptimer_set_raw_count()`
- `gptimer_set_alarm_action()`

### 9.2 Power Management

Jeśli Power Management jest włączone (`CONFIG_PM_ENABLE`):
- Timer automatycznie uzyskuje blokadę zasilania przy `gptimer_enable()`
- Blokada zapobiega zmianom częstotliwości APB podczas działania
- Blokada jest zwalniana przy `gptimer_disable()`
- Użyj `GPTIMER_CLK_SRC_XTAL` dla stabilności z DFS

### 9.3 Thread Safety

API GPTimer jest **thread-safe** — driver wewnętrznie używa mutexów. Możesz bezpiecznie wywoływać `gptimer_start()`, `gptimer_stop()` itp. z różnych tasków.

### 9.4 Timer jako zegar ścienny (wall clock)

```c
// Timer jako precyzyjny licznik czasu — bez alarmu
void wall_clock_example(void)
{
    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,   // 1 µs per tick
    };
    gptimer_handle_t gptimer = NULL;
    ESP_ERROR_CHECK(gptimer_new_timer(&config, &gptimer));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    // Pobierz timestamp w dowolnym momencie
    uint64_t count;
    ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &count));
    ESP_LOGI(TAG, "Czas od startu: %llu µs", (unsigned long long)count);

    // Oblicz czas trwania operacji
    uint64_t start, end;
    gptimer_get_raw_count(gptimer, &start);
    // ... operacja do zmierzenia ...
    gptimer_get_raw_count(gptimer, &end);
    ESP_LOGI(TAG, "Operacja trwała: %llu µs", (unsigned long long)(end - start));
}
```

---

## 10. Ćwiczenie 1: Precyzyjne odmierzanie czasu

### 10.1 Cel ćwiczenia

Użycie GPTimera do precyzyjnego odmierzania czasu — miganie LED z dokładnością do mikrosekund. Porównanie z `vTaskDelay()`.

### 10.2 Schemat połączeń

```
ESP32 NodeMCU-32                 Zewnętrzny LED
┌──────────┐
│          │
│  GPIO 2  ├────────┐
│          │        │
│          │       [R] 330Ω
│          │        │
│  GND     ├────── LED ── GND
│          │       (anoda↑, katoda↓)
└──────────┘

Uwaga: GPIO 2 na wielu płytkach NodeMCU-32 ma WBUDOWANY LED.
Możesz użyć go bez dodatkowych komponentów!
```

### 10.3 Kod — pełny projekt

```c
/* ==========================================================
 * Moduł 1.2 — Ćwiczenie 1: Precyzyjne odmierzanie czasu
 *
 * Demonstruje:
 * - Tworzenie GPTimera z rozdzielczością 1 MHz
 * - Periodyczny alarm z auto-reload
 * - Callback ISR → kolejka → task
 * - Pomiar precyzji odmierzania czasu
 * ========================================================== */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "PRECISE_TIMER";

#define LED_GPIO        GPIO_NUM_2
#define TIMER_PERIOD_US 500000      // 500 ms = 500 000 µs

// Dane zdarzenia
typedef struct {
    uint64_t count_value;
    uint64_t alarm_value;
} timer_event_t;

static QueueHandle_t timer_queue = NULL;

// ══════════════════════════════════════════════════
// Callback alarmu — kontekst ISR
// ══════════════════════════════════════════════════
static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    timer_event_t evt = {
        .count_value = edata->count_value,
        .alarm_value = edata->alarm_value,
    };

    xQueueSendFromISR(queue, &evt, &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}

// ══════════════════════════════════════════════════
// Task obsługi zdarzeń timera
// ══════════════════════════════════════════════════
static void timer_task(void *arg)
{
    timer_event_t evt;
    static int led_state = 0;
    uint32_t alarm_count = 0;
    uint64_t prev_count = 0;

    while (1) {
        if (xQueueReceive(timer_queue, &evt, portMAX_DELAY)) {
            alarm_count++;
            led_state = !led_state;
            gpio_set_level(LED_GPIO, led_state);

            // Oblicz rzeczywisty okres (powinien być ~500000 µs)
            uint64_t delta = evt.count_value - prev_count;
            prev_count = evt.count_value;

            if (alarm_count > 1) {  // Pomijamy pierwszy (brak prev)
                ESP_LOGI(TAG, "Alarm #%lu | LED=%d | delta=%llu µs (oczekiwane: %d µs)",
                         (unsigned long)alarm_count,
                         led_state,
                         (unsigned long long)delta,
                         TIMER_PERIOD_US);
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.2 — Ćwiczenie 1: Precyzyjne odmierzanie czasu ===");

    // 1. Konfiguracja LED
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    // 2. Utwórz kolejkę
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));

    // 3. Utwórz timer — rozdzielczość 1 MHz (1 µs/tick)
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    gptimer_handle_t gptimer = NULL;
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    // 4. Alarm periodyczny
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = TIMER_PERIOD_US,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    // 5. Callback
    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_on_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, timer_queue));

    // 6. Enable i Start
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    // 7. Task obsługi
    xTaskCreate(timer_task, "timer_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "Timer uruchomiony — LED miga co %d µs (%d ms)",
             TIMER_PERIOD_US, TIMER_PERIOD_US / 1000);
}
```

### 10.4 Oczekiwany wynik

```
I (300) PRECISE_TIMER: === Moduł 1.2 — Ćwiczenie 1: Precyzyjne odmierzanie czasu ===
I (305) PRECISE_TIMER: Timer uruchomiony — LED miga co 500000 µs (500 ms)
I (805) PRECISE_TIMER: Alarm #2 | LED=0 | delta=500000 µs (oczekiwane: 500000 µs)
I (1305) PRECISE_TIMER: Alarm #3 | LED=1 | delta=500000 µs (oczekiwane: 500000 µs)
I (1805) PRECISE_TIMER: Alarm #4 | LED=0 | delta=500000 µs (oczekiwane: 500000 µs)
```

> **💡 Eksperyment:** Zmień `TIMER_PERIOD_US` na mniejsze wartości (np. 1000 = 1 ms, 100 = 100 µs) i obserwuj, jak timer zachowuje precyzję nawet dla bardzo krótkich okresów. Porównaj z `vTaskDelay(pdMS_TO_TICKS(1))` — zauważysz jitter rzędu milisekund!

---

## 11. Ćwiczenie 2: Generowanie sygnału prostokątnego

### 11.1 Cel ćwiczenia

Generowanie sygnału prostokątnego o precyzyjnej częstotliwości na pinie GPIO. Sygnał można zweryfikować oscyloskopem lub mierzyć diodą LED (niskie częstotliwości).

### 11.2 Teoria — sygnał prostokątny

```
Sygnał prostokątny (square wave):
                    T_high          T_high
          ┌────────────────┐ ┌────────────────┐
          │                │ │                │
HIGH (1) ─┤                │ │                │
          │                │ │                │
LOW  (0) ─┘                └─┘                └─
          │←──── okres T ──→│

  Częstotliwość f = 1 / T
  Duty cycle 50%: T_high = T_low = T / 2

  Przykład: f = 1 kHz → T = 1 ms → półokres = 500 µs
            f = 10 Hz → T = 100 ms → półokres = 50 ms
```

### 11.3 Podejście: toggle GPIO w callback

Najprostrszy sposób — alarm timera z półokresem, w callback'u przełączamy GPIO:

```c
/* ==========================================================
 * Moduł 1.2 — Ćwiczenie 2: Generowanie sygnału prostokątnego
 *
 * Demonstruje:
 * - Generowanie sygnału o precyzyjnej częstotliwości
 * - Bezpośrednie sterowanie GPIO z callback'a ISR
 * - Minimalny jitter dzięki sprzętowemu timerowi
 *
 * Zmień SQUARE_WAVE_FREQ_HZ aby uzyskać inną częstotliwość.
 * ========================================================== */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "SQUARE_WAVE";

#define OUTPUT_GPIO         GPIO_NUM_2     // Pin wyjściowy (LED/oscyloskop)
#define SQUARE_WAVE_FREQ_HZ 1000           // Częstotliwość sygnału: 1 kHz

// Stan GPIO — przełączany w ISR
static volatile int gpio_state = 0;

// ══════════════════════════════════════════════════
// Callback — przełączenie GPIO (kontekst ISR!)
// ══════════════════════════════════════════════════
static bool IRAM_ATTR square_wave_cb(gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    gpio_state = !gpio_state;
    gpio_set_level(OUTPUT_GPIO, gpio_state);
    return false;   // Nie budzimy żadnego tasku
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.2 — Ćwiczenie 2: Sygnał prostokątny ===");
    ESP_LOGI(TAG, "Częstotliwość: %d Hz na GPIO %d",
             SQUARE_WAVE_FREQ_HZ, OUTPUT_GPIO);

    // 1. Konfiguracja GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OUTPUT_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // 2. Oblicz półokres w µs
    // Okres T = 1/f, półokres = T/2 = 1/(2*f)
    // W mikosekundach: half_period_us = 1 000 000 / (2 * freq)
    uint64_t half_period_us = 1000000 / (2 * SQUARE_WAVE_FREQ_HZ);
    ESP_LOGI(TAG, "Półokres: %llu µs", (unsigned long long)half_period_us);

    // 3. Utwórz timer — rozdzielczość 1 MHz
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,   // 1 tick = 1 µs
    };
    gptimer_handle_t gptimer = NULL;
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    // 4. Alarm — co półokres
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = half_period_us,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    // 5. Callback
    gptimer_event_callbacks_t cbs = {
        .on_alarm = square_wave_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    // 6. Start!
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    ESP_LOGI(TAG, "Sygnał prostokątny generowany. Podłącz oscyloskop do GPIO %d",
             OUTPUT_GPIO);

    // Główny task — wyświetla info co 5 sekund
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Sygnał aktywny: %d Hz, GPIO %d", 
                 SQUARE_WAVE_FREQ_HZ, OUTPUT_GPIO);
    }
}
```

### 11.4 Tabela częstotliwości — gotowe wartości

```
Częstotliwość   Okres T      Półokres      alarm_count (@ 1MHz)
──────────────  ───────────  ────────────  ────────────────────
     1 Hz       1 000 ms     500 ms         500 000
    10 Hz         100 ms      50 ms          50 000
   100 Hz          10 ms       5 ms           5 000
   500 Hz           2 ms       1 ms           1 000
  1 000 Hz          1 ms     500 µs             500
  5 000 Hz        200 µs     100 µs             100
 10 000 Hz        100 µs      50 µs              50
 50 000 Hz         20 µs      10 µs              10  ← bliski limit!
100 000 Hz         10 µs       5 µs               5  ← minimum zalecane
```

> **⚠️ Uwaga:** Powyżej ~50 kHz ISR może nie nadążać ze względu na latencję przerwań. Dla wyższych częstotliwości użyj **LEDC** (Moduł 1.4) lub **RMT** (Moduł 4.1), które generują sygnał sprzętowo bez ISR.

### 11.5 Weryfikacja oscyloskopem

```
Podłączenie oscyloskopu:
  ┌──────────┐
  │  ESP32   │
  │          │
  │  GPIO 2  ├──────── Sonda CH1 oscyloskopu
  │          │
  │  GND     ├──────── Masa oscyloskopu (krokodylek)
  └──────────┘

Na ekranie oscyloskopu powinieneś zobaczyć:
  - Sygnał prostokątny 0V ↔ 3.3V
  - Częstotliwość zgodna z SQUARE_WAVE_FREQ_HZ
  - Duty cycle ~50% (symetryczny)
  - Minimalny jitter (< 1 µs dla f < 10 kHz)
```

### 11.6 Eksperyment: zmienna częstotliwość

```c
// Bonus: Zmiana częstotliwości w runtime!
// Wystarczy zmienić alarm_count przy działającym timerze:

void change_frequency(gptimer_handle_t gptimer, uint32_t new_freq_hz)
{
    uint64_t half_period = 1000000 / (2 * new_freq_hz);

    gptimer_alarm_config_t new_alarm = {
        .alarm_count = half_period,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };

    // Można wywołać w trakcie działania timera!
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &new_alarm));
    ESP_LOGI(TAG, "Częstotliwość zmieniona na %lu Hz",
             (unsigned long)new_freq_hz);
}
```

---

## 12. Podsumowanie i dalsze kroki

### 12.1 Kluczowe wnioski

| Temat | Kluczowa informacja |
|-------|-------------------|
| **Architektura** | 4 timery 64-bit w 2 grupach, 16-bit prescaler |
| **API** | `gptimer_new_timer()` → `enable()` → `start()` → `stop()` → `disable()` → `del_timer()` |
| **Rozdzielczość** | `resolution_hz` zastąpił ręczny prescaler — driver dobiera automatycznie |
| **Alarm** | `alarm_count` = próg wyzwalania, `auto_reload_on_alarm` = periodyczność |
| **Callback** | Kontekst ISR! Krótki, `IRAM_ATTR`, tylko `*FromISR()` FreeRTOS API |
| **Precyzja** | ≤ 1 µs, minimalny period alarmu ≥ 5 µs |

### 12.2 Mapa mentalna

```
           GPTimer
         ╱    │    ╲
    Tworzenie  │  Konfiguracja
        │      │       │
   new_timer   │   alarm_config
        │      │       │
    config:    │    alarm_count
    • clk_src  │    reload_count
    • direction│    auto_reload
    • resolution
               │
          Callback (ISR)
               │
    on_alarm → xQueueSendFromISR
               │
          Worker Task
               │
        Pełna logika aplikacji
```

### 12.3 Co dalej?

- **Moduł 1.3 — Pulse Counter (PCNT):** Zliczanie impulsów sprzętowe — enkodery obrotowe
- **Moduł 1.4 — LEDC:** Sprzętowy PWM — generowanie sygnałów bez ISR
- **Moduł 6.4 — Task Notifications:** Lżejsza alternatywa dla kolejek w ISR → Task
- **Moduł 6.6 — Software Timers:** xTimerCreate() — programowe timery FreeRTOS

---

## 13. Źródła i dokumentacja

### Oficjalna dokumentacja ESP-IDF

| Zasób | Link |
|-------|------|
| **GPTimer API Reference** | [docs.espressif.com — GPTimer](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/gptimer.html) |
| **GPTimer Example** | [github.com/espressif — gptimer example](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/timer_group/gptimer) |
| **ESP32 Technical Reference** | `esp32_technical_reference_manual_en.pdf` (w workspace) — Rozdział 18: Timer Group |

### Dokumenty w workspace

- `esp32_technical_reference_manual_en.pdf` — pełny opis rejestrów sprzętowych Timer Group
- `esp32_datasheet_en.pdf` — specyfikacja elektryczna ESP32
- `esp32-wrover-b_datasheet_en.pdf` — specyfikacja modułu WROVER-B

### Powiązane moduły

| Moduł | Temat | Powiązanie z GPTimer |
|-------|-------|---------------------|
| 1.1 | GPIO & RTC GPIO | Sterowanie pinami z callback'a timera |
| 1.3 | Pulse Counter (PCNT) | Zliczanie impulsów generowanych timerem |
| 1.4 | LEDC | Alternatywa dla generowania PWM (sprzętowo) |
| 4.1 | RMT | Alternatywa dla generowania precyzyjnych sygnałów |
| 6.1 | FreeRTOS Basics | Kolejki, taski — obsługa zdarzeń z timera |
| 6.4 | Task Notifications | Lekka alternatywa dla kolejek w ISR |
| 6.6 | Software Timers | Programowe timery — porównanie z GPTimer |

---

> *Moduł 1.2 — General Purpose Timer (GPTimer). Dokumentacja oparta na ESP-IDF v5.4 API Reference. Marzec 2026.*
