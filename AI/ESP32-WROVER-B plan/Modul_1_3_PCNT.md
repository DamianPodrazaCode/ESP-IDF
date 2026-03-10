# Moduł 1.3 — Pulse Counter (PCNT)

> **Poziom:** 🟢 Laik · **Czas:** Tydzień 3–6 (Faza 1)  
> **Cel:** Opanowanie sprzętowego licznika impulsów ESP32 — konfiguracja jednostek i kanałów, tryby zliczania, dekoder kwadraturowy, filtrowanie glitchy, watch pointy, oraz praktyczne ćwiczenia z enkoderami obrotowymi.

---

## Spis treści

1. [Architektura PCNT w ESP32](#1-architektura-pcnt-w-esp32)
2. [API PCNT — przegląd](#2-api-pcnt--przegląd)
3. [Tworzenie jednostki PCNT — pcnt_new_unit()](#3-tworzenie-jednostki-pcnt--pcnt_new_unit)
4. [Kanały PCNT — pcnt_new_channel()](#4-kanały-pcnt--pcnt_new_channel)
5. [Akcje kanału — Edge i Level](#5-akcje-kanału--edge-i-level)
6. [Dekoder kwadraturowy](#6-dekoder-kwadraturowy)
7. [Filtrowanie glitchy — pcnt_unit_set_glitch_filter()](#7-filtrowanie-glitchy--pcnt_unit_set_glitch_filter)
8. [Watch Pointy i callback'i](#8-watch-pointy-i-callbacki)
9. [Cykl życia jednostki PCNT — maszyna stanów](#9-cykl-życia-jednostki-pcnt--maszyna-stanów)
10. [Zaawansowane: Kompensacja overflow, IRAM Safe, Power Management](#10-zaawansowane-kompensacja-overflow-iram-safe-power-management)
11. [Ćwiczenie 1: Odczyt taniego enkodera obrotowego](#11-ćwiczenie-1-odczyt-taniego-enkodera-obrotowego)
12. [Ćwiczenie 2: Precyzyjny enkoder 500 kroków z pozycją i kierunkiem](#12-ćwiczenie-2-precyzyjny-enkoder-500-kroków-z-pozycją-i-kierunkiem)
13. [Podsumowanie i dalsze kroki](#13-podsumowanie-i-dalsze-kroki)
14. [Źródła i dokumentacja](#14-źródła-i-dokumentacja)

---

## 1. Architektura PCNT w ESP32

### 1.1 Czym jest Pulse Counter?

**Pulse Counter (PCNT)** to sprzętowy peryferium ESP32, które zlicza impulsy (zbocza narastające i/lub opadające) sygnału wejściowego **bez udziału CPU**. Działa całkowicie autonomicznie — procesor nie musi poświęcać czasu na monitorowanie pinów GPIO.

**Kluczowe fakty:**
- ESP32 posiada **8 niezależnych jednostek PCNT** (PCNT Unit 0–7)
- Każda jednostka ma **2 kanały** (Channel 0 i Channel 1)
- Każdy kanał obsługuje **2 typy sygnałów**: edge (zbocze) i level (poziom)
- Wewnętrzny licznik jest **16-bitowy ze znakiem** (zakres: -32768 do +32767)
- Wbudowany **filtr glitchy** (sprzętowe odszumianie sygnału)
- Automatyczny reset licznika przy osiągnięciu limitu górnego/dolnego

**Typowe zastosowania:**
- Odczyt enkoderów obrotowych (rotary encoder)
- Dekodowanie sygnałów kwadraturowych (pozycja + kierunek)
- Pomiar częstotliwości sygnału (zliczanie impulsów w oknie czasowym)
- Licznik obiektów (np. na linii produkcyjnej)

### 1.2 Organizacja modułu PCNT

```
ESP32 — Pulse Counter Module
═══════════════════════════════════════════════════════════════

  ┌─────────────────────────────────────────────────────────┐
  │                    PCNT Module                          │
  │                                                         │
  │  ┌──────────────────┐  ┌──────────────────┐            │
  │  │   Unit 0          │  │   Unit 1          │   ...     │
  │  │  ┌────────────┐  │  │  ┌────────────┐  │            │
  │  │  │ Channel 0  │  │  │  │ Channel 0  │  │            │
  │  │  │ edge + lvl │  │  │  │ edge + lvl │  │            │
  │  │  └────────────┘  │  │  └────────────┘  │            │
  │  │  ┌────────────┐  │  │  ┌────────────┐  │   do       │
  │  │  │ Channel 1  │  │  │  │ Channel 1  │  │   Unit 7   │
  │  │  │ edge + lvl │  │  │  │ edge + lvl │  │            │
  │  │  └────────────┘  │  │  └────────────┘  │            │
  │  │                  │  │                  │            │
  │  │  [16-bit counter]│  │  [16-bit counter]│            │
  │  │  [glitch filter] │  │  [glitch filter] │            │
  │  │  [watch points]  │  │  [watch points]  │            │
  │  └──────────────────┘  └──────────────────┘            │
  └─────────────────────────────────────────────────────────┘

  Łącznie: 8 jednostek × 2 kanały = 16 kanałów wejściowych
  Licznik: 16-bit signed (-32768 do +32767)
  Filtr glitchy: konfigurowalny per jednostka
```

### 1.3 Schemat blokowy pojedynczej jednostki PCNT

```
                    Channel 0                    Channel 1
               ┌──────────────────┐         ┌──────────────────┐
  GPIO_A ──────┤  Edge Signal     │  GPIO_C─┤  Edge Signal     │
               │  (zbocza)        │         │  (zbocza)        │
               │                  │         │                  │
  GPIO_B ──────┤  Level Signal    │  GPIO_D─┤  Level Signal    │
               │  (kontrola)      │         │  (kontrola)      │
               └────────┬─────────┘         └────────┬─────────┘
                        │                            │
                        │   Edge Action:             │
                        │   ↑ INCREMENT              │
                        │   ↓ DECREMENT              │
                        │   ─ HOLD                   │
                        │                            │
                        │   Level Action:            │
                        │   KEEP / INVERSE           │
                        │                            │
                        ▼                            ▼
               ┌─────────────────────────────────────────────┐
               │         Wspólny 16-bit Counter              │
               │         (oba kanały operują na              │
               │          tym samym liczniku!)                │
               ├─────────────────────────────────────────────┤
               │  Glitch Filter  │  Watch Points  │ Limits   │
               │  (odszumianie)  │  (zdarzenia)   │ hi/lo    │
               └─────────────────────────────────────────────┘
```

> **💡 Ważne:** Oba kanały (Channel 0 i Channel 1) jednej jednostki operują na **tym samym liczniku**. To kluczowa właściwość, która umożliwia dekodowanie sygnałów kwadraturowych — jeden kanał reaguje na sygnał A, drugi na sygnał B enkodera.

### 1.4 PCNT vs zliczanie programowe (GPIO ISR)

| Cecha | PCNT (sprzętowy) | GPIO ISR (programowy) |
|-------|------------------|-----------------------|
| **Obciążenie CPU** | Zerowe — zlicza sprzętowo | Każdy impuls = przerwanie CPU |
| **Maks. częstotliwość** | ~40 MHz (z filtrem ~13 MHz) | ~100 kHz (ograniczenie ISR) |
| **Filtr glitchy** | ✅ Sprzętowy | ❌ Wymaga implementacji programowej |
| **Dekoder kwadraturowy** | ✅ Wbudowany (2 kanały) | ❌ Skomplikowana logika w ISR |
| **Ilość kanałów** | 8 jednostek × 2 kanały = 16 | Ograniczona liczbą GPIO |
| **Rozmiar licznika** | 16-bit signed (rozszerzalny) | Dowolny (zmienna w RAM) |
| **Precyzja** | Idealna — zero strat impulsów | Możliwa utrata przy dużej f |

> **💡 Kiedy używać PCNT?** Zawsze gdy zliczasz impulsy zewnętrzne! PCNT jest narzędziem z wyboru dla enkoderów obrotowych, czujników przepływu, tachometrów, i wszelkich aplikacji wymagających precyzyjnego zliczania zboczy sygnału.

---

## 2. API PCNT — przegląd

### 2.1 Nagłówek i CMake

```c
// Główny nagłówek — zawiera wszystkie funkcje PCNT
#include "driver/pulse_cnt.h"
```

W pliku `CMakeLists.txt` komponent `driver` jest dołączany automatycznie w standardowych projektach ESP-IDF.

### 2.2 Kluczowe funkcje API

```
Funkcja                                    Opis
────────────────────────────────────────── ──────────────────────────────────────────
pcnt_new_unit()                            Tworzenie nowej jednostki PCNT
pcnt_del_unit()                            Usunięcie jednostki (zwolnienie zasobów)
pcnt_new_channel()                         Dodanie kanału do jednostki
pcnt_del_channel()                         Usunięcie kanału
pcnt_channel_set_edge_action()             Ustawienie akcji na zbocza (edge)
pcnt_channel_set_level_action()            Ustawienie akcji na poziom (level)
pcnt_unit_set_glitch_filter()              Konfiguracja filtra glitchy
pcnt_unit_add_watch_point()                Dodanie wartości obserwowanej
pcnt_unit_remove_watch_point()             Usunięcie wartości obserwowanej
pcnt_unit_register_event_callbacks()       Rejestracja callback'ów (ISR)
pcnt_unit_enable()                         Włączenie jednostki (init → enable)
pcnt_unit_disable()                        Wyłączenie jednostki (enable → init)
pcnt_unit_start()                          Start zliczania (enable → run)
pcnt_unit_stop()                           Stop zliczania (run → enable)
pcnt_unit_clear_count()                    Zerowanie licznika
pcnt_unit_get_count()                      Odczyt bieżącej wartości licznika
```

### 2.3 Kluczowe struktury

| Struktura | Opis |
|-----------|------|
| `pcnt_unit_config_t` | Konfiguracja jednostki (limity, akumulator) |
| `pcnt_chan_config_t` | Konfiguracja kanału (GPIO edge, GPIO level) |
| `pcnt_glitch_filter_config_t` | Konfiguracja filtra glitchy |
| `pcnt_event_callbacks_t` | Struktura z callback'ami (on_reach) |
| `pcnt_watch_event_data_t` | Dane zdarzenia watch point |

### 2.4 Typy uchwytów

```c
// Uchwyt jednostki PCNT — opaque pointer
pcnt_unit_handle_t pcnt_unit = NULL;

// Uchwyt kanału PCNT — opaque pointer
pcnt_channel_handle_t pcnt_chan = NULL;

// Typowy cykl życia:
pcnt_new_unit(&unit_config, &pcnt_unit);     // Tworzenie jednostki
pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan); // Dodanie kanału
pcnt_unit_enable(pcnt_unit);                 // Włączenie
pcnt_unit_clear_count(pcnt_unit);            // Zerowanie
pcnt_unit_start(pcnt_unit);                  // Start zliczania

int count;
pcnt_unit_get_count(pcnt_unit, &count);      // Odczyt wartości

pcnt_unit_stop(pcnt_unit);                   // Stop
pcnt_unit_disable(pcnt_unit);                // Wyłączenie
pcnt_del_channel(pcnt_chan);                  // Usunięcie kanału
pcnt_del_unit(pcnt_unit);                    // Usunięcie jednostki
```

### 2.5 Enumeracje akcji

```c
// Akcje na zbocza sygnału edge
typedef enum {
    PCNT_CHANNEL_EDGE_ACTION_HOLD,       // Nic nie rób
    PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // Inkrementuj licznik
    PCNT_CHANNEL_EDGE_ACTION_DECREASE,   // Dekrementuj licznik
} pcnt_channel_edge_action_t;

// Akcje na poziom sygnału level (kontrolnego)
typedef enum {
    PCNT_CHANNEL_LEVEL_ACTION_KEEP,      // Zachowaj kierunek zliczania
    PCNT_CHANNEL_LEVEL_ACTION_INVERSE,   // Odwróć kierunek zliczania
    PCNT_CHANNEL_LEVEL_ACTION_HOLD,      // Wstrzymaj zliczanie
} pcnt_channel_level_action_t;
```

---

## 3. Tworzenie jednostki PCNT — pcnt_new_unit()

### 3.1 Struktura konfiguracyjna

```c
#include "driver/pulse_cnt.h"

void create_pcnt_unit_example(void)
{
    // Konfiguracja jednostki PCNT
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,       // Górny limit licznika
        .low_limit  = -100,      // Dolny limit licznika
        .accum_count = true,     // Włącz akumulator (rozszerza zakres)
    };

    // Uchwyt jednostki
    pcnt_unit_handle_t pcnt_unit = NULL;

    // Utworzenie instancji
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
    ESP_LOGI(TAG, "Jednostka PCNT utworzona pomyślnie");
}
```

### 3.2 Pola pcnt_unit_config_t

| Pole | Typ | Opis |
|------|-----|------|
| `high_limit` | `int` | Górny limit — licznik resetuje się do 0 po przekroczeniu |
| `low_limit` | `int` | Dolny limit — licznik resetuje się do 0 po przekroczeniu |
| `accum_count` | `bool` | Czy włączyć wewnętrzny akumulator (kompensacja overflow) |
| `intr_priority` | `int` | Priorytet przerwania (0 = domyślny) |

### 3.3 Limity licznika — jak działają?

```
high_limit = 100, low_limit = -100

  ▲ licznik
  │
 100 ─ ─ ─ ─ HIGH LIMIT ─ ─ ─ ─ → Reset do 0!
  │         ╱
  │       ╱
  │     ╱         (zliczanie w górę)
  │   ╱
  0 ─╱─────────────────────────────────
  │                   ╲
  │                     ╲
  │                       ╲   (zliczanie w dół)
  │                         ╲
-100 ─ ─ ─ ─ LOW LIMIT ─ ─ ─ → Reset do 0!
  │
  └──────────────────────────────► czas
```

> **⚠️ Ważne:** Limity muszą mieścić się w zakresie 16-bit signed: `high_limit` ∈ [1, 32767], `low_limit` ∈ [-32768, -1]. Gdy licznik osiąga limit, jest automatycznie resetowany do zera.

> **💡 Dla enkoderów:** Ustaw limity odpowiednio do rozdzielczości enkodera. Dla enkodera 500 kroków (2000 impulsów w trybie kwadraturowym) ustaw np. `high_limit = 2000`, `low_limit = -2000`.

---

## 4. Kanały PCNT — pcnt_new_channel()

### 4.1 Struktura konfiguracyjna kanału

```c
void add_channel_example(pcnt_unit_handle_t pcnt_unit)
{
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = GPIO_NUM_25,    // GPIO sygnału zboczowego (impulsy)
        .level_gpio_num = GPIO_NUM_26,   // GPIO sygnału kontrolnego (kierunek)
    };

    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));
    ESP_LOGI(TAG, "Kanał PCNT dodany: edge=GPIO25, level=GPIO26");
}
```

### 4.2 Pola pcnt_chan_config_t

| Pole | Typ | Opis |
|------|-----|------|
| `edge_gpio_num` | `int` | GPIO dla sygnału zboczowego (impulsy). -1 = virtual IO |
| `level_gpio_num` | `int` | GPIO dla sygnału kontrolnego (kierunek). -1 = virtual IO |
| `virt_edge_io_level` | `int` | Poziom logiczny virtualnego edge IO (0 lub 1) |
| `virt_level_io_level` | `int` | Poziom logiczny virtualnego level IO (0 lub 1) |
| `flags.invert_edge_input` | `bool` | Inwersja sygnału edge przez GPIO matrix |
| `flags.invert_level_input` | `bool` | Inwersja sygnału level przez GPIO matrix |

### 4.3 Virtual IO — oszczędność GPIO

Gdy jeden z sygnałów (edge lub level) nie jest potrzebny, można ustawić go jako **virtual IO** przypisując `-1` do odpowiedniego GPIO. Virtual IO jest wewnętrznie routowany do stałego poziomu logicznego.

```c
// Prosty licznik impulsów — tylko sygnał edge, bez kontroli poziomu
pcnt_chan_config_t chan_simple = {
    .edge_gpio_num = GPIO_NUM_25,     // Tylko impulsy
    .level_gpio_num = -1,             // Virtual IO — oszczędność GPIO!
};
```

### 4.4 Dwa kanały na jednej jednostce

```c
// Dekoder kwadraturowy — 2 kanały na jednej jednostce
// Kanał 0: edge = sygnał A enkodera, level = sygnał B enkodera
pcnt_chan_config_t chan_a_config = {
    .edge_gpio_num = ENCODER_GPIO_A,
    .level_gpio_num = ENCODER_GPIO_B,
};
pcnt_channel_handle_t pcnt_chan_a = NULL;
ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

// Kanał 1: edge = sygnał B enkodera, level = sygnał A enkodera
pcnt_chan_config_t chan_b_config = {
    .edge_gpio_num = ENCODER_GPIO_B,
    .level_gpio_num = ENCODER_GPIO_A,
};
pcnt_channel_handle_t pcnt_chan_b = NULL;
ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));
```

---

## 5. Akcje kanału — Edge i Level

### 5.1 Akcje na zbocza (Edge Actions)

Funkcja `pcnt_channel_set_edge_action()` definiuje, co ma się stać z licznikiem na **zboczu narastającym** i **zboczu opadającym** sygnału edge.

```c
// Zliczanie w górę na zboczu narastającym, nic na opadającym
ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // Na zboczu narastającym: +1
    PCNT_CHANNEL_EDGE_ACTION_HOLD));     // Na zboczu opadającym: nic

// Zliczanie w obie strony: +1 na narastającym, -1 na opadającym
ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // Rising edge: +1
    PCNT_CHANNEL_EDGE_ACTION_DECREASE)); // Falling edge: -1
```

```
Akcje na zbocza sygnału edge:

  Sygnał:  ┌──┐  ┌──┐  ┌──┐
           │  │  │  │  │  │
        ───┘  └──┘  └──┘  └──

           ↑  ↓  ↑  ↓  ↑  ↓
           R  F  R  F  R  F    (R=Rising, F=Falling)

  Przykład 1 (INCREASE/HOLD):
    Licznik: 0  0  1  1  2  2  3

  Przykład 2 (INCREASE/DECREASE):
    Licznik: 0  0  1  0  1  0  1
```

### 5.2 Akcje na poziom (Level Actions)

Funkcja `pcnt_channel_set_level_action()` definiuje, jak sygnał kontrolny **modyfikuje** zachowanie zliczania.

```c
// Gdy level HIGH → zliczaj normalnie, gdy level LOW → odwróć kierunek
ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan,
    PCNT_CHANNEL_LEVEL_ACTION_KEEP,      // High level: normalnie
    PCNT_CHANNEL_LEVEL_ACTION_INVERSE)); // Low level: odwróć
```

```
Sygnał level (kontrolny):
  HIGH ─────┐              ┌─────
            │              │
  LOW       └──────────────┘

Sygnał edge (impulsy):
        ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
        │ │ │ │ │ │ │ │ │ │ │ │
     ───┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─

Akcja: KEEP(high) / INVERSE(low)

  Level=HIGH: edge rising → +1 (normalnie)
  Level=LOW:  edge rising → -1 (odwrócono!)


  To jest DOKŁADNIE to, co robi dekoder kwadraturowy!
```

### 5.3 Tabela akcji — podsumowanie

| Edge Action | Rising Edge | Falling Edge |
|-------------|------------|--------------|
| `HOLD` | Nic | Nic |
| `INCREASE` | Licznik +1 | Licznik +1 |
| `DECREASE` | Licznik -1 | Licznik -1 |

| Level Action | Efekt |
|-------------|-------|
| `KEEP` | Akcja edge działa normalnie |
| `INVERSE` | Akcja edge jest odwrócona (INC→DEC, DEC→INC) |
| `HOLD` | Zliczanie wstrzymane (niezależnie od edge) |

---

## 6. Dekoder kwadraturowy

### 6.1 Czym jest sygnał kwadraturowy?

Enkoder obrotowy generuje **dwa sygnały** (A i B) przesunięte o 90°. Dzięki temu można określić zarówno **ilość kroków**, jak i **kierunek obrotu**.

```
Obrót w prawo (CW — clockwise):
  Sygnał A:  ┌──┐  ┌──┐  ┌──┐
             │  │  │  │  │  │
          ───┘  └──┘  └──┘  └──

  Sygnał B:    ┌──┐  ┌──┐  ┌──┐
               │  │  │  │  │  │
          ─────┘  └──┘  └──┘  └

  A prowadzi przed B o 90°


Obrót w lewo (CCW — counter-clockwise):
  Sygnał A:    ┌──┐  ┌──┐  ┌──┐
               │  │  │  │  │  │
          ─────┘  └──┘  └──┘  └

  Sygnał B:  ┌──┐  ┌──┐  ┌──┐
             │  │  │  │  │  │
          ───┘  └──┘  └──┘  └──

  B prowadzi przed A o 90°
```

### 6.2 Jak PCNT dekoduje sygnał kwadraturowy?

Potrzebujemy **2 kanały** na jednej jednostce PCNT, skonfigurowane następująco:

```
Kanał 0: edge=A, level=B
  Edge action:  rising=DECREASE, falling=INCREASE
  Level action: high=KEEP, low=INVERSE

Kanał 1: edge=B, level=A
  Edge action:  rising=INCREASE, falling=DECREASE
  Level action: high=KEEP, low=INVERSE
```

**Efekt:** Przy obrocie CW licznik rośnie (+4 na pełny cykl kwadraturowy), przy obrocie CCW maleje (-4 na pełny cykl).

```
Pełny cykl kwadraturowy (CW):
  A: ──┐  ┌──           Stan fazowy:
       │  │              00 → 10 → 11 → 01 → 00
  B: ────┐  ┌──
         │  │

  Kanał 0 (edge=A):  R(A)=DEC, ale B=LOW→INVERSE → +1
                      F(A)=INC, i B=HIGH→KEEP    → +1
  Kanał 1 (edge=B):  R(B)=INC, i A=HIGH→KEEP    → +1
                      F(B)=DEC, ale A=LOW→INVERSE → +1
                                                   ────
                                              Razem: +4
```

### 6.3 Kompletna konfiguracja dekodera kwadraturowego

```c
#include "driver/pulse_cnt.h"
#include "esp_log.h"

#define ENCODER_GPIO_A  GPIO_NUM_25
#define ENCODER_GPIO_B  GPIO_NUM_26

static const char *TAG = "QUADRATURE";

void setup_quadrature_decoder(pcnt_unit_handle_t pcnt_unit)
{
    // ═══ Kanał 0: edge = A, level = B ═══
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_GPIO_A,
        .level_gpio_num = ENCODER_GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,    // Rising A → DEC
        PCNT_CHANNEL_EDGE_ACTION_INCREASE));  // Falling A → INC

    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,       // B HIGH → normalnie
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));  // B LOW → odwróć

    // ═══ Kanał 1: edge = B, level = A ═══
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENCODER_GPIO_B,
        .level_gpio_num = ENCODER_GPIO_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,    // Rising B → INC
        PCNT_CHANNEL_EDGE_ACTION_DECREASE));  // Falling B → DEC

    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,       // A HIGH → normalnie
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));  // A LOW → odwróć

    ESP_LOGI(TAG, "Dekoder kwadraturowy skonfigurowany");
}
```

### 6.4 Rozdzielczość dekodera kwadraturowego

```
Enkoder mechaniczny (tani, 20 detentów na obrót):
  • 20 detentów × 4 impulsy/cykl = 80 impulsów na pełny obrót (360°)
  • Rozdzielczość: 360° / 80 = 4.5° na impuls

Enkoder optyczny (precyzyjny, 500 kroków na obrót):
  • 500 kroków × 4 impulsy/cykl = 2000 impulsów na pełny obrót (360°)
  • Rozdzielczość: 360° / 2000 = 0.18° na impuls

  "×4" bo dekoder kwadraturowy liczy WSZYSTKIE zbocza
  obu sygnałów (4 zdarzenia na jeden pełny cykl A+B)
```

---

## 7. Filtrowanie glitchy — pcnt_unit_set_glitch_filter()

### 7.1 Problem: szum i drgania styków

Enkodery mechaniczne (tanie) generują **drgania styków** (contact bounce) — przy każdym przełączeniu powstaje seria szybkich, fałszywych impulsów, które PCNT może policzyć jako prawdziwe kroki.

```
Idealne zbocze:          Rzeczywiste zbocze (z drganiami):
                          
  HIGH ─────┐              HIGH ─────┐ ┌┐ ┌──
            │                        │ ││ │
  LOW       └──────        LOW       └─┘└─┘

  1 zbocze = 1 impuls      1 zbocze = 5+ fałszywych impulsów!
```

### 7.2 Rozwiązanie: sprzętowy filtr glitchy

PCNT posiada **wbudowany filtr glitchy**, który ignoruje impulsy krótsze niż zadana szerokość. Filtr działa w pełni sprzętowo — zero obciążenia CPU.

```c
#include "driver/pulse_cnt.h"

void setup_glitch_filter(pcnt_unit_handle_t pcnt_unit)
{
    // Filtr glitchy — ignoruj impulsy krótsze niż 1000 ns (1 µs)
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,    // Maksymalna szerokość glitcha w ns
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));
    ESP_LOGI(TAG, "Filtr glitchy: max %u ns", filter_config.max_glitch_ns);
}
```

### 7.3 Dobór parametru max_glitch_ns

```
Typ enkodera          Zalecane max_glitch_ns    Uwagi
───────────────────── ─────────────────────── ───────────────────────────
Tani mechaniczny      1000–10000 (1–10 µs)     Duże drgania styków
Precyzyjny optyczny   100–1000 (0.1–1 µs)      Małe szumy, szybki sygnał
Sygnał cyfrowy        50–500                    Minimalne zakłócenia

Ograniczenia:
  • Minimum: > 12.5 ns (1 cykl APB @ 80 MHz)
  • Filtr zbyt szeroki → utrata prawdziwych impulsów!
  • Filtr zbyt wąski → przepuszcza glitchy

Zasada: max_glitch_ns < (okres_impulsu / 2)
  Np. enkoder 500 PPR @ 600 RPM:
  f = 500 × 600/60 = 5000 Hz → T = 200 µs → połowa = 100 µs
  → max_glitch_ns = 10000 (10 µs) jest bezpieczne ✅
```

### 7.4 Wyłączenie filtra

```c
// Przekazanie NULL wyłącza filtr glitchy
ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, NULL));
```

> **⚠️ Ważne:** Filtr glitchy działa na zegarze APB (80 MHz). Przy włączonym Dynamic Frequency Scaling (DFS) częstotliwość APB może się zmieniać, co wpływa na działanie filtra. Driver automatycznie instaluje power management lock, aby temu zapobiec.

> **💡 Kolejność:** `pcnt_unit_set_glitch_filter()` musi być wywołane **przed** `pcnt_unit_enable()` (w stanie `init`). W przeciwnym razie zwróci `ESP_ERR_INVALID_STATE`.

---

## 8. Watch Pointy i callback'i

### 8.1 Czym są Watch Pointy?

**Watch Point** to wartość licznika, którą chcesz obserwować. Gdy licznik osiąga watch point, generowane jest **zdarzenie** (event), które może wywołać callback w kontekście ISR.

```
Typowe watch pointy:
  • 0 (zero cross)          — wykrywanie przejścia przez zero
  • high_limit              — wykrywanie osiągnięcia górnego limitu
  • low_limit               — wykrywanie osiągnięcia dolnego limitu
  • dowolna wartość progowa — np. 100 kroków
```

### 8.2 Dodawanie i usuwanie watch pointów

```c
// Dodanie watch pointu na zero (wykrywanie przejścia przez zero)
ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, 0));

// Dodanie watch pointu na wartość progową
ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, 50));

// Dodanie watch pointu na ujemną wartość
ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, -50));

// Usunięcie watch pointu
ESP_ERROR_CHECK(pcnt_unit_remove_watch_point(pcnt_unit, 50));
```

> **⚠️ Ważne:** Po dodaniu watch pointu należy wywołać `pcnt_unit_clear_count()` aby aktywować go poprawnie. Ilość dostępnych watch pointów jest ograniczona sprzętowo.

### 8.3 Rejestracja callback'a

```c
#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t pcnt_evt_queue = NULL;

// ══════════════════════════════════════════════════
// Callback watch point — wykonywany w kontekście ISR!
// ══════════════════════════════════════════════════
static bool IRAM_ATTR pcnt_on_reach(pcnt_unit_handle_t unit,
    const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    // Wyślij wartość watch pointu do kolejki
    int watch_value = edata->watch_point_value;
    xQueueSendFromISR(queue, &watch_value, &high_task_awoken);

    return (high_task_awoken == pdTRUE);
}

void register_pcnt_callbacks(pcnt_unit_handle_t pcnt_unit)
{
    // Utwórz kolejkę
    pcnt_evt_queue = xQueueCreate(10, sizeof(int));

    // Rejestracja callback'a
    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_on_reach,
    };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs,
        pcnt_evt_queue));
}
```

### 8.4 Dane zdarzenia (pcnt_watch_event_data_t)

| Pole | Typ | Opis |
|------|-----|------|
| `watch_point_value` | `int` | Wartość licznika w momencie zdarzenia |
| `zero_cross_mode` | `pcnt_unit_zero_cross_mode_t` | Jak licznik przeszedł przez zero |

### 8.5 Zasady pisania callback'a ISR

Te same zasady co dla callback'ów GPTimer i GPIO ISR:

```c
// ✅ DOZWOLONE w callback'u:
xQueueSendFromISR(queue, &data, &woken);     // Kolejka
xSemaphoreGiveFromISR(sem, &woken);           // Semafor
vTaskNotifyGiveFromISR(task, &woken);         // Notyfikacja
gpio_set_level(LED_GPIO, 1);                  // Toggle GPIO

// ❌ ZABRONIONE w callback'u:
// ESP_LOGI(TAG, "...");    // Mutex + UART = crash!
// printf("...");            // To samo
// malloc/free               // Heap = niedeterministyczny
// vTaskDelay()              // Nie wolno czekać w ISR!
```

---

## 9. Cykl życia jednostki PCNT — maszyna stanów

### 9.1 Stany jednostki PCNT

```
                 pcnt_new_unit()
                        │
                        ▼
                   ┌─────────┐
          ┌───────│  INIT    │───────┐
          │       └─────────┘       │
          │            │            │
          │  pcnt_unit_enable()     │  pcnt_del_unit()
          │            │            │
          │            ▼            │
          │       ┌─────────┐       │
          │       │ ENABLE  │       │
          │       └─────────┘       │
          │       │         │       │
          │  pcnt_unit_start()      │  pcnt_unit_disable()
          │       │         │       │
          │       ▼         │       │
          │  ┌─────────┐   │       │
          │  │   RUN   │   │       │
          │  └─────────┘   │       │
          │       │         │       │
          │  pcnt_unit_stop()       │
          │       │         │       │
          │       ▼         │       │
          │  (→ ENABLE) ────┘       │
          │                         │
          └─── (← INIT) ───────────┘

Operacje dozwolone w każdym stanie:
  INIT:   set_glitch_filter, add/remove_watch_point,
          register_event_callbacks, new/del_channel,
          set_edge/level_action
  ENABLE: start, stop, clear_count, get_count
  RUN:    stop, get_count, clear_count
```

### 9.2 Poprawna sekwencja operacji

```c
void pcnt_lifecycle_example(void)
{
    pcnt_unit_handle_t pcnt_unit = NULL;
    pcnt_channel_handle_t pcnt_chan = NULL;

    // ── FAZA 1: Tworzenie (→ INIT) ──
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    // ── FAZA 2: Konfiguracja (w stanie INIT) ──
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = GPIO_NUM_25,
        .level_gpio_num = -1,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_HOLD));

    pcnt_glitch_filter_config_t filter = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter));

    // ── FAZA 3: Włączenie (INIT → ENABLE) ──
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));

    // ── FAZA 4: Start (ENABLE → RUN) ──
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    // ... PCNT zlicza impulsy ...

    // ── FAZA 5: Stop (RUN → ENABLE) ──
    ESP_ERROR_CHECK(pcnt_unit_stop(pcnt_unit));

    // ── FAZA 6: Wyłączenie (ENABLE → INIT) ──
    ESP_ERROR_CHECK(pcnt_unit_disable(pcnt_unit));

    // ── FAZA 7: Usunięcie (INIT → zwolniony) ──
    ESP_ERROR_CHECK(pcnt_del_channel(pcnt_chan));   // Najpierw kanały!
    ESP_ERROR_CHECK(pcnt_del_unit(pcnt_unit));       // Potem jednostka
}
```

### 9.3 Typowe błędy sekwencji

```c
// ❌ BŁĄD: start bez enable
pcnt_unit_start(pcnt_unit);  // ESP_ERR_INVALID_STATE!

// ❌ BŁĄD: filtr po enable
pcnt_unit_enable(pcnt_unit);
pcnt_unit_set_glitch_filter(pcnt_unit, &filter);  // ESP_ERR_INVALID_STATE!

// ❌ BŁĄD: usunięcie jednostki z podłączonymi kanałami
pcnt_del_unit(pcnt_unit);  // BŁĄD! Najpierw pcnt_del_channel()!

// ✅ POPRAWNA sekwencja zatrzymania:
pcnt_unit_stop(pcnt_unit);       // RUN → ENABLE
pcnt_unit_disable(pcnt_unit);    // ENABLE → INIT
pcnt_del_channel(pcnt_chan);     // Usunięcie kanałów
pcnt_del_unit(pcnt_unit);        // INIT → zwolniony
```

---

## 10. Zaawansowane: Kompensacja overflow, IRAM Safe, Power Management

### 10.1 Kompensacja overflow (accum_count)

Sprzętowy licznik PCNT jest 16-bitowy i resetuje się do zera przy osiągnięciu limitu. Aby nie tracić danych, włącz **akumulator**:

```c
pcnt_unit_config_t unit_config = {
    .high_limit = 1000,
    .low_limit = -1000,
    .accum_count = true,     // ← Włącz akumulator!
};
```

Z włączonym akumulatorem, `pcnt_unit_get_count()` zwraca wartość **uwzględniającą** wszystkie przepełnienia — licznik wewnętrznie sumuje overflow'y.

**Wymagania dla accum_count:**
1. Ustaw `accum_count = true` w konfiguracji
2. Dodaj `high_limit` i `low_limit` jako watch pointy
3. Teraz `pcnt_unit_get_count()` automatycznie kompensuje overflow

### 10.2 IRAM Safe

```c
// W menuconfig: CONFIG_PCNT_ISR_IRAM_SAFE = y
// Zapewnia, że callback ISR działa nawet gdy Flash SPI jest zajęty
```

Włączenie `CONFIG_PCNT_ISR_IRAM_SAFE` umieszcza ISR handler w wewnętrznej RAM (IRAM). Jest to ważne, gdy system jednocześnie operuje na Flash SPI (np. zapis do NVS, SPIFFS).

### 10.3 Power Management

Gdy filtr glitchy jest włączony, driver automatycznie zarządza blokadą power management. Dzięki temu częstotliwość APB (od której zależy filtr) nie jest zmieniana podczas zliczania.

### 10.4 Thread Safety

Większość funkcji PCNT driver jest **thread-safe** — mogą być wywoływane z różnych tasków FreeRTOS bez dodatkowej synchronizacji. Wyjątkiem jest `pcnt_unit_get_count()`, który odczytuje rejestr sprzętowy bezpośrednio.

---

## 11. Ćwiczenie 1: Odczyt taniego enkodera obrotowego

### 11.1 Cel ćwiczenia

Odczytanie pozycji i kierunku obrotu **taniego enkodera mechanicznego** (typ EC11, 20 detentów na obrót) za pomocą PCNT w trybie dekodera kwadraturowego. Wyświetlanie pozycji w terminalu.

### 11.2 Schemat podłączenia

```
Tani enkoder obrotowy (EC11)
═══════════════════════════════════════════

  Enkoder EC11          ESP32 (NodeMCU-32)
  ┌──────────┐         ┌──────────────────┐
  │          │         │                  │
  │  A ──────┼─────────┤ GPIO 25          │
  │          │         │                  │
  │  B ──────┼─────────┤ GPIO 26          │
  │          │         │                  │
  │  GND ────┼─────────┤ GND              │
  │          │         │                  │
  │  SW ─────┼────┐    │                  │  (opcjonalnie)
  └──────────┘    │    │                  │
                  └────┤ GPIO 27          │
                       └──────────────────┘

  Pull-up: wewnętrzny ESP32 (INPUT_PULLUP)
  Filtr glitchy: 1000 ns (1 µs) — konieczny dla enkodera mechanicznego!
```

### 11.3 Kompletny kod

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ENCODER_CHEAP";

// ═══ Piny enkodera ═══
#define ENCODER_GPIO_A   GPIO_NUM_25
#define ENCODER_GPIO_B   GPIO_NUM_26

// ═══ Limity licznika ═══
// Tani enkoder: 20 detentów × 4 (kwadraturowo) = 80 na obrót
// Ustawiamy szeroki zakres, aby nie tracić pozycji
#define PCNT_HIGH_LIMIT   1000
#define PCNT_LOW_LIMIT   -1000

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.3: Tani enkoder obrotowy (PCNT) ===");

    // ═══════════════════════════════════════════
    // 1. Tworzenie jednostki PCNT
    // ═══════════════════════════════════════════
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit  = PCNT_LOW_LIMIT,
        .accum_count = true,    // Kompensacja overflow
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
    ESP_LOGI(TAG, "Jednostka PCNT utworzona [%d, %d]",
             PCNT_LOW_LIMIT, PCNT_HIGH_LIMIT);

    // ═══════════════════════════════════════════
    // 2. Filtr glitchy — kluczowy dla enkodera mechanicznego!
    // ═══════════════════════════════════════════
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,    // 1 µs — filtruje drgania styków
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));
    ESP_LOGI(TAG, "Filtr glitchy: %u ns", filter_config.max_glitch_ns);

    // ═══════════════════════════════════════════
    // 3. Kanał 0: edge = A, level = B
    // ═══════════════════════════════════════════
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_GPIO_A,
        .level_gpio_num = ENCODER_GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    // Dekoder kwadraturowy — konfiguracja kanału A
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // ═══════════════════════════════════════════
    // 4. Kanał 1: edge = B, level = A
    // ═══════════════════════════════════════════
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENCODER_GPIO_B,
        .level_gpio_num = ENCODER_GPIO_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    // Dekoder kwadraturowy — konfiguracja kanału B
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // ═══════════════════════════════════════════
    // 5. Watch pointy (dla accum_count)
    // ═══════════════════════════════════════════
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, PCNT_HIGH_LIMIT));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, PCNT_LOW_LIMIT));

    // ═══════════════════════════════════════════
    // 6. Włączenie i start
    // ═══════════════════════════════════════════
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
    ESP_LOGI(TAG, "Enkoder uruchomiony — obracaj pokrętłem!");

    // ═══════════════════════════════════════════
    // 7. Pętla odczytu
    // ═══════════════════════════════════════════
    int prev_count = 0;
    while (1) {
        int pulse_count = 0;
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulse_count));

        if (pulse_count != prev_count) {
            // Oblicz pozycję w detentach (4 impulsy na detent)
            int detents = pulse_count / 4;
            const char *direction = (pulse_count > prev_count) ? "CW →" : "← CCW";

            ESP_LOGI(TAG, "Count: %+d | Detent: %+d | %s",
                     pulse_count, detents, direction);
            prev_count = pulse_count;
        }

        vTaskDelay(pdMS_TO_TICKS(10));   // Polluj co 10 ms
    }
}
```

### 11.4 Oczekiwany wynik

```
I (325) ENCODER_CHEAP: === Moduł 1.3: Tani enkoder obrotowy (PCNT) ===
I (335) ENCODER_CHEAP: Jednostka PCNT utworzona [-1000, 1000]
I (340) ENCODER_CHEAP: Filtr glitchy: 1000 ns
I (345) ENCODER_CHEAP: Enkoder uruchomiony — obracaj pokrętłem!
I (1050) ENCODER_CHEAP: Count: +4 | Detent: +1 | CW →
I (1200) ENCODER_CHEAP: Count: +8 | Detent: +2 | CW →
I (1350) ENCODER_CHEAP: Count: +12 | Detent: +3 | CW →
I (2100) ENCODER_CHEAP: Count: +8 | Detent: +2 | ← CCW
I (2250) ENCODER_CHEAP: Count: +4 | Detent: +1 | ← CCW
```

> **💡 Wskazówka:** Każdy „click" (detent) enkodera mechanicznego to 4 impulsy kwadraturowe. Dzieląc `pulse_count / 4` otrzymujesz liczbę detentów.

---

## 12. Ćwiczenie 2: Precyzyjny enkoder 500 kroków z pozycją i kierunkiem

### 12.1 Cel ćwiczenia

Odczytanie pozycji kątowej i kierunku obrotu **precyzyjnego enkodera optycznego 500 kroków** za pomocą PCNT. Wyświetlanie pozycji w stopniach z rozdzielczością 0.18°, prędkości obrotowej (RPM) i kierunku.

### 12.2 Schemat podłączenia

```
Precyzyjny enkoder optyczny 500 PPR
═══════════════════════════════════════════

  Enkoder 500 PPR        ESP32 (NodeMCU-32)
  ┌──────────┐          ┌──────────────────┐
  │          │          │                  │
  │  A ──────┼──────────┤ GPIO 25          │
  │          │          │                  │
  │  B ──────┼──────────┤ GPIO 26          │
  │          │          │                  │
  │  VCC ────┼──────────┤ 3.3V / 5V       │
  │          │          │                  │
  │  GND ────┼──────────┤ GND              │
  └──────────┘          └──────────────────┘

  Enkoder optyczny: sygnał czysty, pull-up wbudowany
  Filtr glitchy: 100 ns — lekki filtr (sygnał jest czysty)
  Rozdzielczość: 500 × 4 = 2000 impulsów/obrót (0.18°/impuls)
```

### 12.3 Kompletny kod

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ENCODER_PRECISION";

// ═══ Piny enkodera ═══
#define ENCODER_GPIO_A   GPIO_NUM_25
#define ENCODER_GPIO_B   GPIO_NUM_26

// ═══ Parametry enkodera ═══
#define ENCODER_PPR      500        // Pulses Per Revolution (kroków na obrót)
#define QUAD_PULSES_REV  (ENCODER_PPR * 4)  // 2000 impulsów kwadraturowych/obrót
#define DEGREES_PER_REV  360.0f

// ═══ Limity PCNT ═══
// Używamy accum_count do rozszerzenia zakresu
#define PCNT_HIGH_LIMIT   10000
#define PCNT_LOW_LIMIT   -10000

// ═══ Interwał odczytu ═══
#define READ_INTERVAL_MS  50        // 50 ms = 20 Hz odczytu

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.3: Precyzyjny enkoder 500 PPR (PCNT) ===");
    ESP_LOGI(TAG, "Rozdzielczość: %.2f° na impuls kwadraturowy",
             DEGREES_PER_REV / QUAD_PULSES_REV);

    // ═══════════════════════════════════════════
    // 1. Tworzenie jednostki PCNT z akumulatorem
    // ═══════════════════════════════════════════
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit  = PCNT_LOW_LIMIT,
        .accum_count = true,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    // ═══════════════════════════════════════════
    // 2. Filtr glitchy — lekki dla enkodera optycznego
    // ═══════════════════════════════════════════
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 100,     // 100 ns — enkoder optyczny ma czysty sygnał
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    // ═══════════════════════════════════════════
    // 3. Kanał 0: edge = A, level = B (dekoder kwadraturowy)
    // ═══════════════════════════════════════════
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_GPIO_A,
        .level_gpio_num = ENCODER_GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // ═══════════════════════════════════════════
    // 4. Kanał 1: edge = B, level = A (dekoder kwadraturowy)
    // ═══════════════════════════════════════════
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENCODER_GPIO_B,
        .level_gpio_num = ENCODER_GPIO_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // ═══════════════════════════════════════════
    // 5. Watch pointy dla accum_count
    // ═══════════════════════════════════════════
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, PCNT_HIGH_LIMIT));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, PCNT_LOW_LIMIT));

    // ═══════════════════════════════════════════
    // 6. Włączenie i start
    // ═══════════════════════════════════════════
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    ESP_LOGI(TAG, "Enkoder 500 PPR uruchomiony!");
    ESP_LOGI(TAG, "────────────────────────────────────────────");
    ESP_LOGI(TAG, " Count  │   Kąt    │   RPM   │ Kierunek");
    ESP_LOGI(TAG, "────────────────────────────────────────────");

    // ═══════════════════════════════════════════
    // 7. Pętla odczytu z obliczaniem RPM
    // ═══════════════════════════════════════════
    int prev_count = 0;
    int64_t prev_time_us = esp_timer_get_time();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(READ_INTERVAL_MS));

        int pulse_count = 0;
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulse_count));
        int64_t now_us = esp_timer_get_time();

        if (pulse_count != prev_count) {
            // Oblicz kąt w stopniach
            float angle = (float)(pulse_count % QUAD_PULSES_REV)
                          * DEGREES_PER_REV / QUAD_PULSES_REV;
            if (angle < 0) angle += DEGREES_PER_REV;

            // Oblicz RPM
            int delta_count = pulse_count - prev_count;
            float delta_time_s = (float)(now_us - prev_time_us) / 1000000.0f;
            float rpm = 0;
            if (delta_time_s > 0) {
                float revolutions = (float)delta_count / QUAD_PULSES_REV;
                rpm = (revolutions / delta_time_s) * 60.0f;
            }

            // Kierunek
            const char *direction;
            if (delta_count > 0) direction = "CW  →";
            else if (delta_count < 0) direction = "← CCW";
            else direction = " STOP";

            // Liczba pełnych obrotów
            int full_revs = pulse_count / QUAD_PULSES_REV;

            ESP_LOGI(TAG, "%+6d  │ %6.2f°  │ %+6.1f  │ %s  (obrót: %d)",
                     pulse_count, angle, rpm, direction, full_revs);

            prev_count = pulse_count;
            prev_time_us = now_us;
        }
    }
}
```

### 12.4 Oczekiwany wynik

```
I (325) ENCODER_PRECISION: === Moduł 1.3: Precyzyjny enkoder 500 PPR (PCNT) ===
I (330) ENCODER_PRECISION: Rozdzielczość: 0.18° na impuls kwadraturowy
I (335) ENCODER_PRECISION: Enkoder 500 PPR uruchomiony!
I (340) ENCODER_PRECISION: ────────────────────────────────────────────
I (345) ENCODER_PRECISION:  Count  │   Kąt    │   RPM   │ Kierunek
I (350) ENCODER_PRECISION: ────────────────────────────────────────────
I (550) ENCODER_PRECISION:   +12  │   2.16°  │  +7.2   │ CW  →  (obrót: 0)
I (600) ENCODER_PRECISION:   +48  │   8.64°  │ +43.2   │ CW  →  (obrót: 0)
I (650) ENCODER_PRECISION:  +124  │  22.32°  │ +91.2   │ CW  →  (obrót: 0)
I (700) ENCODER_PRECISION:  +256  │  46.08°  │ +158.4  │ CW  →  (obrót: 0)
I (2050) ENCODER_PRECISION: +2000  │   0.00°  │  +36.0  │ CW  →  (obrót: 1)
I (3200) ENCODER_PRECISION: +1800  │ 324.00°  │  -24.0  │ ← CCW  (obrót: 0)
```

### 12.5 Analiza kodu — kluczowe elementy

```
Obliczanie kąta:
  angle = (count % 2000) × 360° / 2000
  count = 500  → angle = 500 × 0.18° = 90°
  count = 1000 → angle = 1000 × 0.18° = 180°
  count = 2000 → angle = 0° (pełny obrót)

Obliczanie RPM:
  RPM = (Δcount / 2000) / Δtime_s × 60
  Δcount = 200 w Δtime = 0.05s
  RPM = (200/2000) / 0.05 × 60 = 0.1 / 0.05 × 60 = 120 RPM

Filtr glitchy:
  Enkoder optyczny: 100 ns wystarczy (brak drgań mechanicznych)
  Przy 600 RPM, f = 5 kHz, T = 200 µs → filtr 100 ns << 200 µs ✅
```

---

## 13. Podsumowanie i dalsze kroki

### 13.1 Co nauczyliśmy się w tym module?

| Temat | Kluczowe pojęcia |
|-------|------------------|
| **Architektura PCNT** | 8 jednostek × 2 kanały, 16-bit signed counter |
| **Sygnały edge/level** | Edge = impulsy (zbocza), Level = kontrola kierunku |
| **Dekoder kwadraturowy** | 2 kanały, sygnały A/B, rozdzielczość ×4 |
| **Filtr glitchy** | Sprzętowe odszumianie, `max_glitch_ns`, konieczny dla mechanicznych |
| **Watch pointy** | Obserwacja wartości licznika, callback ISR |
| **Cykl życia** | init → enable → run, poprawna sekwencja operacji |
| **Kompensacja overflow** | `accum_count = true`, rozszerzenie 16-bit do praktycznie nieograniczonego |

### 13.2 Najczęstsze problemy

| Problem | Rozwiązanie |
|---------|------------|
| Enkoder liczy podwójnie/poczwórnie | Dziel przez 4 (tryb kwadraturowy ×4) |
| Fałszywe impulsy (tani enkoder) | Zwiększ `max_glitch_ns` (1000–10000) |
| Licznik resetuje się do 0 | Ustaw szersze limity lub włącz `accum_count` |
| ESP_ERR_INVALID_STATE | Sprawdź kolejność: konfiguracja przed `enable()` |
| Brak reakcji na obrót | Sprawdź piny GPIO, pull-up, podłączenie A/B |
| Kierunek odwrócony | Zamień piny A↔B lub odwróć akcje edge |

### 13.3 Dalsze kroki

- **Moduł 1.4 (LEDC):** PWM sprzętowy — sterowanie jasnością LED z pozycji enkodera
- **Moduł 3.2 (I2C):** Wyświetlanie pozycji enkodera na OLED SSD1306
- **Moduł 6.3 (Queues):** Architektura RTOS — task enkodera → queue → task wyświetlania
- **Moduł 14.2:** Radar odległości — skanowanie z enkoderem + czujnik ToF

---

## 14. Źródła i dokumentacja

### 14.1 Oficjalna dokumentacja ESP-IDF

| Zasób | Link |
|-------|------|
| **PCNT API Reference** | [docs.espressif.com — Pulse Counter](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/pcnt.html) |
| **PCNT Rotary Encoder Example** | [github.com/espressif/esp-idf — rotary_encoder](https://github.com/espressif/esp-idf/tree/v5.5.3/examples/peripherals/pcnt/rotary_encoder) |
| **ESP32 Technical Reference** | `esp32_technical_reference_manual_en.pdf` — Rozdział "Pulse Counter" |

### 14.2 Pliki w workspace

| Plik | Zawartość |
|------|-----------|
| `esp32_technical_reference_manual_en.pdf` | Opis rejestrów PCNT na poziomie sprzętowym |
| `esp32_datasheet_en.pdf` | Specyfikacja pinów i ograniczenia elektryczne |
| `esp32-wrover-b_datasheet_en.pdf` | Specyfikacja modułu WROVER-B |

### 14.3 Nagłówki ESP-IDF (do podglądu źródeł)

```
Plik                              Zawartość
────────────────────────────────  ──────────────────────────────────
driver/pulse_cnt.h                Główny nagłówek API PCNT
hal/pcnt_types.h                  Typy, enumeracje (akcje, tryby)
soc/soc_caps.h                    SOC_PCNT_UNITS_PER_GROUP (= 8)
```

---

> *Moduł 1.3 — Pulse Counter (PCNT). Dokumentacja wygenerowana 09.03.2026. Sprzęt: enkoder mechaniczny EC11 (36), precyzyjny enkoder optyczny 500 PPR (35).*
