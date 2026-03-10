# Moduł 1.1 — GPIO & RTC GPIO

> **Poziom:** 🟢 Laik · **Czas:** Tydzień 3–6 (Faza 1)  
> **Cel:** Opanowanie systemu GPIO ESP32 — konfiguracja pinów, przerwania, RTC GPIO dla deep sleep, oraz praktyczne ćwiczenia z LED i przyciskami.

---

## Spis treści

1. [Architektura GPIO w ESP32](#1-architektura-gpio-w-esp32)
2. [gpio_config() — konfiguracja pinów](#2-gpio_config--konfiguracja-pinów)
3. [Pull-up / Pull-down wewnętrzne](#3-pull-up--pull-down-wewnętrzne)
4. [Przerwania GPIO](#4-przerwania-gpio)
5. [RTC GPIO — piny w deep sleep](#5-rtc-gpio--piny-w-deep-sleep)
6. [Ćwiczenie 1: Miganie LED](#6-ćwiczenie-1-miganie-led)
7. [Ćwiczenie 2: Debouncing przycisku](#7-ćwiczenie-2-debouncing-przycisku)
8. [Podsumowanie i dalsze kroki](#8-podsumowanie-i-dalsze-kroki)
9. [Źródła i dokumentacja](#9-źródła-i-dokumentacja)

---

## 1. Architektura GPIO w ESP32

### 1.1 Czym jest GPIO?

**GPIO** (General Purpose Input/Output) to **piny mikrokontrolera**, które mogą być programowo skonfigurowane jako wejścia lub wyjścia cyfrowe. ESP32 posiada **34 piny GPIO** (GPIO0–GPIO39), ale nie wszystkie są dostępne do swobodnego użycia.

**Kluczowe fakty:**
- ESP32 ma **34 piny GPIO** ponumerowane od 0 do 39
- Piny **GPIO 34–39** są **tylko wejściami** (input-only) — nie mają wewnętrznych pull-up/pull-down
- Piny **GPIO 6–11** są zajęte przez **wewnętrzny SPI Flash** — **nie używaj ich!**
- Każdy pin GPIO może pełnić wiele funkcji (multipleksowanie) — UART, SPI, I2C, ADC, DAC, Touch itp.
- Napięcie logiczne: **3.3V** (NIGDY nie podłączaj 5V bezpośrednio!)

### 1.2 Mapa GPIO ESP32 — podział funkcjonalny

```
GPIO ESP32 — Podział funkcjonalny
═══════════════════════════════════════════════════════════════

 GPIO 0–5, 12–39  → Dostępne jako GPIO ogólnego przeznaczenia
 GPIO 6–11        → ⛔ ZAREZERWOWANE (SPI Flash) — NIE UŻYWAJ!
 GPIO 34–39       → ⚠️ Tylko INPUT (brak wyjścia, brak pull-up/down)
 GPIO 0, 2, 5,    → ⚠️ Strapping pins (wpływają na boot)
       12, 15

 RTC GPIO (18 pinów):
 GPIO 0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,37,38,39
 → Mogą działać w deep sleep pod kontrolą koprocesora ULP
```

### 1.3 Mapowanie GPIO na NodeMCU-32

```
              ┌──────────────────┐
              │    NodeMCU-32    │
              │   ESP32-WROVER-B │
              │                  │
     3V3  ────┤                  ├──── VIN (5V)
     GND  ────┤                  ├──── GND
  GPIO 36 ────┤ VP (input only)  ├──── GPIO 23 (MOSI)
  GPIO 39 ────┤ VN (input only)  ├──── GPIO 22 (SCL)
  GPIO 34 ────┤ (input only)     ├──── GPIO  1 (TX0)
  GPIO 35 ────┤ (input only)     ├──── GPIO  3 (RX0)
  GPIO 32 ────┤ (ADC1_CH4)       ├──── GPIO 21 (SDA)
  GPIO 33 ────┤ (ADC1_CH5)       ├──── GPIO 19 (MISO)
  GPIO 25 ────┤ (DAC1)           ├──── GPIO 18 (SCK)
  GPIO 26 ────┤ (DAC2)           ├──── GPIO  5 ⚠️ (strap)
  GPIO 27 ────┤                  ├──── GPIO 17 (TX2)
  GPIO 14 ────┤                  ├──── GPIO 16 (RX2)
  GPIO 12 ────┤ ⚠️ (strap)      ├──── GPIO  4
  GPIO 13 ────┤                  ├──── GPIO  2 ⚠️ (strap/LED)
  GPIO 15 ────┤ ⚠️ (strap)      ├──── GPIO  0 ⚠️ (strap/BOOT)
              └──────────────────┘

⚠️ = Strapping pin — ostrożność przy boot!
```

### 1.4 Strapping Pins — piny wpływające na bootowanie

| GPIO | Funkcja strapping | Stan podczas boot | Uwagi |
|------|-------------------|-------------------|-------|
| **GPIO 0** | Tryb boot | HIGH = Normal boot, LOW = Download mode | Przycisk BOOT na płytce |
| **GPIO 2** | Tryb boot | Musi być LOW lub floating przy download | Wbudowany LED na wielu płytkach |
| **GPIO 5** | Timing SDIO slave | HIGH = domyślne | Wpływa na timing SDIO |
| **GPIO 12** | Napięcie VDD_SDIO | LOW = 3.3V, HIGH = 1.8V | ⚠️ Błędny stan = crash PSRAM! |
| **GPIO 15** | Wyciszenie logów | LOW = cichy boot (brak logów) | Normalnie HIGH |

> **⚠️ WAŻNE:** GPIO 12 przy HIGH ustawia VDD_SDIO na 1.8V, co powoduje **crash PSRAM** na ESP32-WROVER-B (PSRAM wymaga 3.3V!). Jeśli musisz użyć GPIO 12, ustaw efuse: `espefuse.py set_flash_voltage 3.3V` lub skonfiguruj w menuconfig.

---

## 2. gpio_config() — konfiguracja pinów

### 2.1 Struktura konfiguracyjna

ESP-IDF używa **jednej struktury** `gpio_config_t` do konfiguracji pinów GPIO. To podejście jest bardziej efektywne niż wywoływanie wielu pojedynczych funkcji.

```c
#include "driver/gpio.h"

void configure_gpio_example(void)
{
    // Struktura konfiguracyjna GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_2),   // Maska bitowa pinów
        .mode         = GPIO_MODE_OUTPUT,        // Tryb pinu
        .pull_up_en   = GPIO_PULLUP_DISABLE,     // Pull-up wewnętrzny
        .pull_down_en = GPIO_PULLDOWN_DISABLE,   // Pull-down wewnętrzny
        .intr_type    = GPIO_INTR_DISABLE,       // Typ przerwania
    };

    // Zastosuj konfigurację
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
    }
}
```

### 2.2 Pola struktury gpio_config_t

| Pole | Typ | Opis |
|------|-----|------|
| `pin_bit_mask` | `uint64_t` | Maska bitowa pinów do skonfigurowania |
| `mode` | `gpio_mode_t` | Tryb pinu: INPUT, OUTPUT, itp. |
| `pull_up_en` | `gpio_pullup_t` | Włączenie wewnętrznego pull-up |
| `pull_down_en` | `gpio_pulldown_t` | Włączenie wewnętrznego pull-down |
| `intr_type` | `gpio_int_type_t` | Typ przerwania (zbocze/poziom) |

### 2.3 Maska bitowa — konfiguracja wielu pinów

```c
// Konfiguracja JEDNEGO pinu
.pin_bit_mask = (1ULL << GPIO_NUM_2)

// Konfiguracja WIELU pinów jednocześnie (OR bitowy)
.pin_bit_mask = (1ULL << GPIO_NUM_2) | (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_16)

// Makro pomocnicze — to samo co wyżej
#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_NUM_2) | (1ULL << GPIO_NUM_4))
```

> **💡 Dlaczego `1ULL`?** `pin_bit_mask` to `uint64_t` (64-bitowy). Użycie `1` (32-bit) przy przesunięciu o > 31 pozycji dałoby **undefined behavior**. `1ULL` gwarantuje operację na 64-bitowej wartości.

### 2.4 Tryby GPIO (gpio_mode_t)

```
Tryb                        Opis                              Zastosowanie
───────────────────────────────────────────────────────────────────────────
GPIO_MODE_DISABLE           Pin wyłączony                     Oszczędność energii
GPIO_MODE_INPUT             Wejście cyfrowe                   Przycisk, czujnik cyfrowy
GPIO_MODE_OUTPUT            Wyjście push-pull                 LED, przekaźnik
GPIO_MODE_OUTPUT_OD         Wyjście open-drain                I2C (alternatywa), LED
GPIO_MODE_INPUT_OUTPUT      Wejście + wyjście push-pull       Bidirectional, debug
GPIO_MODE_INPUT_OUTPUT_OD   Wejście + wyjście open-drain      Magistrale 1-Wire, I2C
```

#### Tryb OUTPUT (push-pull)

```
           VCC (3.3V)
            │
          ┌─┤─┐
          │PMOS│  ← Włączony gdy OUTPUT = HIGH
          └─┬─┘
            ├──── GPIO Pin ──── Wyjście (0V lub 3.3V)
          ┌─┤─┐
          │NMOS│  ← Włączony gdy OUTPUT = LOW
          └─┬─┘
            │
           GND

  • HIGH → PMOS ON, NMOS OFF → pin = VCC (3.3V)
  • LOW  → PMOS OFF, NMOS ON → pin = GND (0V)
  • Może aktywnie wymusić oba stany
```

#### Tryb OPEN_DRAIN

```
            ├──── GPIO Pin ──── Wyjście
          ┌─┤─┐
          │NMOS│  ← Jedyny tranzystor
          └─┬─┘
            │
           GND

  • LOW  → NMOS ON → pin = GND (0V)
  • HIGH → NMOS OFF → pin "wisi" (Hi-Z) → potrzebny ZEWNĘTRZNY pull-up!
  • Nie może aktywnie wymusić HIGH — potrzebny rezystor
  • Zastosowanie: magistrale współdzielone (I2C), wired-AND
```

### 2.5 Alternatywne API — pojedyncze funkcje

Zamiast `gpio_config()` możesz używać indywidualnych funkcji. Jest to mniej efektywne, ale przydatne do szybkich zmian w runtime:

```c
#include "driver/gpio.h"

void configure_gpio_individual(void)
{
    // Resetuj pin do stanu domyślnego
    gpio_reset_pin(GPIO_NUM_2);

    // Ustaw kierunek
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    // Ustaw stan wyjścia
    gpio_set_level(GPIO_NUM_2, 1);   // HIGH
    gpio_set_level(GPIO_NUM_2, 0);   // LOW

    // Odczytaj stan pinu (zarówno INPUT jak i OUTPUT)
    int level = gpio_get_level(GPIO_NUM_4);

    // Włącz/wyłącz pull-up/pull-down
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);

    // Ustaw siłę wyjścia (drive strength)
    gpio_set_drive_capability(GPIO_NUM_2, GPIO_DRIVE_CAP_3);  // Najsilniejszy
}
```

### 2.6 Siła wyjścia (Drive Capability)

```c
// Siła wyjścia — ile prądu może dostarczyć pin
typedef enum {
    GPIO_DRIVE_CAP_0 = 0,   // ~5 mA  — najsłabszy
    GPIO_DRIVE_CAP_1 = 1,   // ~10 mA — słaby
    GPIO_DRIVE_CAP_2 = 2,   // ~20 mA — domyślny
    GPIO_DRIVE_CAP_3 = 3,   // ~40 mA — najsilniejszy
} gpio_drive_cap_t;

// Ustawienie siły wyjścia
gpio_set_drive_capability(GPIO_NUM_2, GPIO_DRIVE_CAP_2);  // 20 mA — domyślne

// Odczyt aktualnej siły
gpio_drive_cap_t cap;
gpio_get_drive_capability(GPIO_NUM_2, &cap);
```

> **⚠️ Uwaga:** Maksymalny prąd z **jednego pinu** to ~40 mA, ale **łączny prąd ze wszystkich pinów** nie powinien przekraczać ~1200 mA (patrz datasheet). Typowy LED wymaga 10–20 mA.

---

## 3. Pull-up / Pull-down wewnętrzne

### 3.1 Po co są rezystory pull-up / pull-down?

Pin GPIO w stanie wejścia, gdy **nie jest podłączony do żadnego sygnału**, jest w stanie **pływającym** (floating). Jego wartość jest losowa — może być 0 lub 1, zmienia się pod wpływem zakłóceń elektromagnetycznych.

```
Stan pływający (ŹRÓDŁO BŁĘDÓW!):
                                  Zakłócenia EMI
    GPIO Input ──── ???           ~~~~~ → losowe 0/1
    (niepodłączony)

Pull-UP (domyślnie HIGH):            Pull-DOWN (domyślnie LOW):
    VCC (3.3V)                             GPIO Input
       │                                      │
      [R] 45 kΩ (wewnętrzny)                [R] 45 kΩ (wewnętrzny)
       │                                      │
    GPIO Input                              GND
       │
   Przycisk → GND                      Przycisk → VCC
   (naciśnięcie = LOW)                 (naciśnięcie = HIGH)
```

### 3.2 Konfiguracja pull-up / pull-down

```c
#include "driver/gpio.h"

void configure_pullup_pulldown(void)
{
    // ══════════════════════════════════════════════════
    // Metoda 1: Przez gpio_config()
    // ══════════════════════════════════════════════════

    // Przycisk z wewnętrznym PULL-UP (domyślnie HIGH, LOW przy naciśnięciu)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,      // ← Pull-up WŁĄCZONY
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);

    // ══════════════════════════════════════════════════
    // Metoda 2: Przez gpio_set_pull_mode()
    // ══════════════════════════════════════════════════

    // Tylko pull-up
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);

    // Tylko pull-down
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLDOWN_ONLY);

    // Oba jednocześnie (rzadko używane — utrzymuje ~VCC/2)
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_FLOATING);

    // Wyłączenie pull-up i pull-down
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_FLOATING);
}
```

### 3.3 Parametry wewnętrznych rezystorów

| Parametr | Wartość | Uwagi |
|----------|---------|-------|
| Rezystancja pull-up | ~45 kΩ | Typowa (zakres: 30–80 kΩ) |
| Rezystancja pull-down | ~45 kΩ | Typowa (zakres: 30–80 kΩ) |
| Dostępność | GPIO 0–33 | **GPIO 34–39 NIE mają pull-up/down!** |

> **⚠️ Ważne:** GPIO 34–39 to piny **input-only** bez wewnętrznych rezystorów. Jeśli podłączasz do nich przycisk, **musisz dodać zewnętrzny rezystor** pull-up (4.7 kΩ – 10 kΩ) lub pull-down.

### 3.4 Wewnętrzny vs zewnętrzny pull-up

| Cecha | Wewnętrzny (~45 kΩ) | Zewnętrzny (4.7–10 kΩ) |
|-------|---------------------|----------------------|
| Wygoda | ✅ Brak dodatkowych elementów | ❌ Rezystor na płytce |
| Prąd spoczynkowy | ~73 µA (3.3V/45kΩ) | ~330 µA (3.3V/10kΩ) |
| Odporność na szum | ⚠️ Słaba (wysoka R) | ✅ Dobra (niska R) |
| Szybkość | ⚠️ Wolne zbocza (duża stała RC) | ✅ Szybkie zbocza |
| Deep sleep | ❌ Wyłączony (domyślnie) | ✅ Zawsze aktywny |

**Rekomendacja:** Wewnętrzny pull-up wystarczy do prostych zastosowań (przyciski UI). Do szybkich sygnałów, magistral komunikacyjnych i niezawodnych systemów używaj **zewnętrznego pull-up 4.7–10 kΩ**.

---

## 4. Przerwania GPIO

### 4.1 Czym są przerwania GPIO?

**Przerwanie** (interrupt) to mechanizm sprzętowy, który natychmiast powiadamia CPU o zmianie stanu pinu GPIO, **bez ciągłego odpytywania** (polling).

```
Polling (ZŁE — marnuje CPU):          Przerwanie (DOBRE — efektywne):
┌────────────────────┐                ┌────────────────────┐
│ while(1) {         │                │ // CPU robi inne   │
│   if(gpio==LOW) {  │                │ // rzeczy...       │
│     handle();      │                │                    │
│   }                │                │ ISR: ──────────┐   │
│   // CPU zajęte    │                │   signal_task();│  │
│   // 100% czasu!   │                │ ←──────────────┘   │
│ }                  │                │ // CPU wolne 99.9%  │
└────────────────────┘                └────────────────────┘
```

### 4.2 Typy przerwań GPIO

```c
typedef enum {
    GPIO_INTR_DISABLE     = 0,  // Przerwanie wyłączone
    GPIO_INTR_POSEDGE     = 1,  // Zbocze narastające (LOW → HIGH) ↑
    GPIO_INTR_NEGEDGE     = 2,  // Zbocze opadające (HIGH → LOW)   ↓
    GPIO_INTR_ANYEDGE     = 3,  // Oba zbocza (każda zmiana)       ↑↓
    GPIO_INTR_LOW_LEVEL   = 4,  // Poziom niski (ciągłe!)
    GPIO_INTR_HIGH_LEVEL  = 5,  // Poziom wysoki (ciągłe!)
} gpio_int_type_t;
```

```
Sygnał GPIO:     ___╱‾‾‾‾‾‾╲___╱‾‾‾╲___

POSEDGE (↑):        ↑              ↑         Narastające zbocze
NEGEDGE (↓):               ↓           ↓    Opadające zbocze
ANYEDGE (↑↓):      ↑      ↓       ↑    ↓   Każda zmiana
LOW_LEVEL:      ████            ████         Gdy pin = LOW (ciągłe!)
HIGH_LEVEL:          ██████████             Gdy pin = HIGH (ciągłe!)
```

> **⚠️ Uwaga:** Przerwania **poziomowe** (`LOW_LEVEL`, `HIGH_LEVEL`) są ciągle generowane dopóki pin ma dany stan — mogą zalać CPU przerwaniami! Używaj zbocz (`POSEDGE`, `NEGEDGE`, `ANYEDGE`) do przycisków.

### 4.3 Konfiguracja przerwań GPIO — kompletny przykład

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "GPIO_ISR";

#define BUTTON_GPIO     GPIO_NUM_0    // Przycisk BOOT na NodeMCU-32
#define LED_GPIO        GPIO_NUM_2    // Wbudowany LED

// Kolejka do komunikacji ISR → Task
static QueueHandle_t gpio_evt_queue = NULL;

// ══════════════════════════════════════════════════
// ISR Handler — MUSI być krótki i w IRAM!
// ══════════════════════════════════════════════════
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    // Wysyłamy numer GPIO do kolejki — task obsłuży resztę
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// ══════════════════════════════════════════════════
// Task obsługujący zdarzenia GPIO
// ══════════════════════════════════════════════════
static void gpio_task(void *arg)
{
    uint32_t gpio_num;
    static int led_state = 0;

    while (1) {
        // Czekaj na zdarzenie z ISR (blokujące — nie zużywa CPU)
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY)) {
            led_state = !led_state;
            gpio_set_level(LED_GPIO, led_state);
            ESP_LOGI(TAG, "GPIO[%lu] przerwanie → LED = %d",
                     (unsigned long)gpio_num, led_state);
        }
    }
}

void app_main(void)
{
    // 1. Konfiguracja LED (OUTPUT)
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);

    // 2. Konfiguracja przycisku (INPUT + przerwanie na zboczu opadającym)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,      // Pull-up — przycisk do GND
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,        // Przerwanie: HIGH → LOW
    };
    gpio_config(&btn_conf);

    // 3. Utwórz kolejkę do komunikacji ISR → Task
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 4. Zainstaluj serwis ISR GPIO (współdzielony handler)
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // 5. Podłącz handler ISR do konkretnego pinu
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);

    // 6. Utwórz task obsługujący zdarzenia
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Przerwania GPIO skonfigurowane. Naciśnij BOOT!");
}
```

### 4.4 Serwis ISR GPIO — szczegóły

```c
// ══════════════════════════════════════════════════
// gpio_install_isr_service() — instalacja współdzielonego serwisu ISR
// ══════════════════════════════════════════════════

// Flagi przerwań:
#define ESP_INTR_FLAG_DEFAULT  0              // Domyślne ustawienia
#define ESP_INTR_FLAG_LEVEL1   (1<<1)         // Priorytet 1 (najniższy)
#define ESP_INTR_FLAG_LEVEL2   (1<<2)         // Priorytet 2
#define ESP_INTR_FLAG_LEVEL3   (1<<3)         // Priorytet 3 (domyślny)
#define ESP_INTR_FLAG_IRAM     (1<<10)        // Handler w IRAM (wymagane!)
#define ESP_INTR_FLAG_SHARED   (1<<8)         // Współdzielone przerwanie
#define ESP_INTR_FLAG_EDGE     (1<<9)         // Przerwanie zboczem

// Typowe użycie:
gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
// lub z IRAM (bezpieczniejsze — działa nawet gdy Flash jest zajęty):
gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
```

### 4.5 Zarządzanie handlerami ISR

```c
// Dodanie handlera do pinu
gpio_isr_handler_add(GPIO_NUM_0, my_isr_handler, (void *)GPIO_NUM_0);

// Usunięcie handlera z pinu (pin nadal skonfigurowany, ale ISR nie wywoływane)
gpio_isr_handler_remove(GPIO_NUM_0);

// Włączenie/wyłączenie przerwania na pinie (bez usuwania handlera)
gpio_intr_disable(GPIO_NUM_0);   // Wyłącz przerwanie
gpio_intr_enable(GPIO_NUM_0);    // Włącz ponownie

// Zmiana typu przerwania w runtime
gpio_set_intr_type(GPIO_NUM_0, GPIO_INTR_POSEDGE);

// Odinstalowanie całego serwisu ISR (zwalnia zasoby)
gpio_uninstall_isr_service();
```

### 4.6 Zasady pisania ISR — co wolno, czego NIE

```c
// ═══════════════════════════════════════════
// ZŁOTA ZASADA: ISR musi być KRÓTKI i SZYBKI!
// ═══════════════════════════════════════════

// ✅ DOZWOLONE w ISR:
static void IRAM_ATTR good_isr(void *arg)
{
    // Ustaw flagę
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Wyślij do kolejki
    xQueueSendFromISR(queue, &data, &xHigherPriorityTaskWoken);

    // Daj semafor
    xSemaphoreGiveFromISR(semaphore, &xHigherPriorityTaskWoken);

    // Powiadom task
    vTaskNotifyGiveFromISR(task_handle, &xHigherPriorityTaskWoken);

    // Zmień stan GPIO
    gpio_set_level(LED_GPIO, 1);

    // Przełącz kontekst jeśli potrzeba
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// ❌ ZABRONIONE w ISR:
static void IRAM_ATTR bad_isr(void *arg)
{
    // ❌ Logowanie (mutex + UART = crash/deadlock)
    // ESP_LOGI(TAG, "...");

    // ❌ printf (jw.)
    // printf("ISR!\n");

    // ❌ malloc/free (nieatomowe, mutex)
    // void *p = malloc(100);

    // ❌ vTaskDelay (nie można czekać w ISR!)
    // vTaskDelay(pdMS_TO_TICKS(10));

    // ❌ xQueueReceive (blokujące oczekiwanie)
    // xQueueReceive(queue, &data, portMAX_DELAY);

    // ❌ Operacje na Flash (SPI — kolizja)
    // nvs_get_i32(...);
}
```

> **💡 Kluczowe:** Atrybut `IRAM_ATTR` umieszcza funkcję w pamięci **IRAM** (wewnętrzna RAM) zamiast Flash. Jest to **wymagane** dla ISR, ponieważ w momencie przerwania Flash może być zajęty operacją SPI.

---

## 5. RTC GPIO — piny w deep sleep

### 5.1 Czym jest RTC GPIO?

**RTC GPIO** to podzbiór pinów GPIO, które mogą zachować swoją funkcjonalność w trybie **deep sleep**. Są kontrolowane przez podsystem **RTC** (Real-Time Controller), który działa niezależnie od głównego CPU.

Podczas deep sleep:
- Główne CPU (Xtensa LX6) jest **wyłączone**
- Większość peryferiów jest **wyłączona**
- Pamięć SRAM jest **wyłączona** (oprócz RTC SLOW/FAST memory)
- **RTC controller działa** — monitoruje piny RTC GPIO
- Pobór prądu: **~10 µA** (vs ~240 mA podczas pracy)

### 5.2 Lista RTC GPIO na ESP32

```
RTC GPIO    ESP32 GPIO    Dodatkowe funkcje      Uwagi
─────────────────────────────────────────────────────────
RTC_GPIO0     GPIO 36     ADC1_CH0, VP           Input only
RTC_GPIO3     GPIO 39     ADC1_CH3, VN           Input only
RTC_GPIO4     GPIO 34     ADC1_CH6               Input only
RTC_GPIO5     GPIO 35     ADC1_CH7               Input only
RTC_GPIO6     GPIO 25     DAC1, ADC2_CH8
RTC_GPIO7     GPIO 26     DAC2, ADC2_CH9
RTC_GPIO8     GPIO 33     ADC1_CH5, Touch8
RTC_GPIO9     GPIO 32     ADC1_CH4, Touch9
RTC_GPIO10    GPIO  4     ADC2_CH0, Touch0
RTC_GPIO11    GPIO  0     ADC2_CH1, Touch1       ⚠️ Strapping
RTC_GPIO12    GPIO  2     ADC2_CH2, Touch2       ⚠️ Strapping/LED
RTC_GPIO13    GPIO 15     ADC2_CH3, Touch3       ⚠️ Strapping
RTC_GPIO14    GPIO 13     ADC2_CH4, Touch4
RTC_GPIO15    GPIO 12     ADC2_CH5, Touch5       ⚠️ Strapping
RTC_GPIO16    GPIO 14     ADC2_CH6, Touch6
RTC_GPIO17    GPIO 27     ADC2_CH7, Touch7
```

### 5.3 API RTC GPIO

```c
#include "driver/rtc_io.h"

void configure_rtc_gpio(void)
{
    // Inicjalizacja pinu jako RTC GPIO
    rtc_gpio_init(GPIO_NUM_27);

    // Ustawienie kierunku
    rtc_gpio_set_direction(GPIO_NUM_27, RTC_GPIO_MODE_OUTPUT_ONLY);

    // Ustawienie poziomu (utrzymany w deep sleep!)
    rtc_gpio_set_level(GPIO_NUM_27, 1);   // HIGH

    // Pull-up/pull-down w trybie RTC (działają w deep sleep!)
    rtc_gpio_pullup_en(GPIO_NUM_4);
    rtc_gpio_pulldown_dis(GPIO_NUM_4);

    // Utrzymanie stanu pinu w deep sleep
    rtc_gpio_hold_en(GPIO_NUM_27);        // ← KLUCZOWE!
    // Bez hold_en pin straci stan po wejściu w deep sleep

    // Zwolnienie hold (po wybudzeniu)
    rtc_gpio_hold_dis(GPIO_NUM_27);

    // Deinicjalizacja — przywróć pin do normalnego GPIO
    rtc_gpio_deinit(GPIO_NUM_27);
}
```

### 5.4 Wybudzanie z deep sleep przez GPIO (ext0 / ext1)

ESP32 obsługuje dwa mechanizmy wybudzania przez GPIO:

#### EXT0 — jeden pin

```c
#include "esp_sleep.h"
#include "driver/rtc_io.h"

void setup_deep_sleep_ext0(void)
{
    // Wybudzenie gdy GPIO_NUM_4 == LOW (np. naciśnięcie przycisku)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 0);  // 0 = LOW level

    // Włącz pull-up RTC (działa w deep sleep)
    rtc_gpio_pullup_en(GPIO_NUM_4);
    rtc_gpio_pulldown_dis(GPIO_NUM_4);

    ESP_LOGI(TAG, "Wchodzę w deep sleep... Naciśnij przycisk na GPIO 4");

    // Wejście w deep sleep
    esp_deep_sleep_start();

    // ← Kod NIGDY tu nie dotrze!
    // Po wybudzeniu ESP32 restartuje się (app_main() od nowa)
}

void app_main(void)
{
    // Sprawdź przyczynę wybudzenia
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wybudzony przez EXT0 (GPIO)!");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "Pierwszy start (power-on / reset)");
            break;
        default:
            ESP_LOGI(TAG, "Inna przyczyna wybudzenia: %d", cause);
    }

    // ... reszta logiki ...
    setup_deep_sleep_ext0();
}
```

#### EXT1 — wiele pinów

```c
#include "esp_sleep.h"
#include "driver/rtc_io.h"

void setup_deep_sleep_ext1(void)
{
    // Maska pinów do monitorowania
    const uint64_t ext1_mask = (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_27);

    // Wybudzenie gdy KTÓRYKOLWIEK pin == LOW
    esp_sleep_enable_ext1_wakeup(ext1_mask, ESP_EXT1_WAKEUP_ALL_LOW);

    // Opcje trybu:
    // ESP_EXT1_WAKEUP_ALL_LOW  — wybudź gdy WSZYSTKIE piny = LOW
    // ESP_EXT1_WAKEUP_ANY_HIGH — wybudź gdy KTÓRYKOLWIEK pin = HIGH

    // Pull-upy RTC dla obu pinów
    rtc_gpio_pullup_en(GPIO_NUM_4);
    rtc_gpio_pullup_en(GPIO_NUM_27);

    esp_deep_sleep_start();
}

// Po wybudzeniu — sprawdź który pin wybudził
void check_ext1_wakeup(void)
{
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pins & (1ULL << GPIO_NUM_4)) {
            ESP_LOGI(TAG, "Wybudzony przez GPIO 4");
        }
        if (wakeup_pins & (1ULL << GPIO_NUM_27)) {
            ESP_LOGI(TAG, "Wybudzony przez GPIO 27");
        }
    }
}
```

### 5.5 rtc_gpio_hold_en() — utrzymanie stanu w deep sleep

```c
// Przykład: LED włączony podczas deep sleep
void led_on_during_sleep(void)
{
    // 1. Zainicjuj jako RTC GPIO
    rtc_gpio_init(GPIO_NUM_2);
    rtc_gpio_set_direction(GPIO_NUM_2, RTC_GPIO_MODE_OUTPUT_ONLY);

    // 2. Ustaw stan HIGH
    rtc_gpio_set_level(GPIO_NUM_2, 1);

    // 3. Zablokuj stan (hold) — pin utrzyma HIGH w deep sleep
    rtc_gpio_hold_en(GPIO_NUM_2);

    // 4. Deep sleep — LED świeci nadal!
    esp_deep_sleep_start();
}

// Po wybudzeniu — zwolnij hold przed zmianą stanu
void after_wakeup(void)
{
    // Zwolnij WSZYSTKIE holdy na raz
    gpio_hold_dis(GPIO_NUM_2);
    // lub: rtc_gpio_hold_dis(GPIO_NUM_2);

    // Teraz możesz zmieniać stan pinu
    gpio_set_level(GPIO_NUM_2, 0);
}
```

---

## 6. Ćwiczenie 1: Miganie LED

### 6.1 Schemat połączeń

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

### 6.2 Kod — pełny projekt

```c
/* ==========================================================
 * Moduł 1.1 — Ćwiczenie 1: Miganie LED
 *
 * Demonstruje:
 * - gpio_config() z trybem OUTPUT
 * - gpio_set_level() do sterowania pinem
 * - vTaskDelay() do odmierzania czasu
 * - System logów ESP_LOGx
 * ========================================================== */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LED_BLINK";

#define LED_GPIO        GPIO_NUM_2      // Wbudowany LED
#define BLINK_PERIOD_MS 500             // Okres migania

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.1: Miganie LED ===");

    // Konfiguracja GPIO jako OUTPUT
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Błąd konfiguracji GPIO: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "LED na GPIO %d — miganie co %d ms", LED_GPIO, BLINK_PERIOD_MS);

    int led_state = 0;
    uint32_t cycle = 0;

    while (1) {
        led_state = !led_state;
        gpio_set_level(LED_GPIO, led_state);

        // Loguj co 10 cykli (żeby nie zalewać terminala)
        if (++cycle % 10 == 0) {
            ESP_LOGD(TAG, "Cykl %lu, LED = %s",
                     (unsigned long)cycle, led_state ? "ON" : "OFF");
        }

        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_MS));
    }
}
```

---

## 7. Ćwiczenie 2: Debouncing przycisku

### 7.1 Problem drgania styków (bouncing)

Mechaniczny przycisk nie przełącza się czysto — jego styki **drgają** przez 5–50 ms, generując wiele szybkich przejść 0↔1.

```
Idealne naciśnięcie:          Rzeczywiste naciśnięcie (bounce):
                               
 HIGH ────┐                    HIGH ────┐ ┌┐ ┌┐ ┌──┐
          │                             │ ││ ││ │  │
 LOW      └──────────          LOW      └─┘└─┘└─┘  └──────

  → 1 przerwanie                → 6-20 przerwań! (błędy!)
```

### 7.2 Schemat połączeń

```
ESP32 NodeMCU-32
┌──────────┐
│          │                    Przycisk
│  GPIO 0  ├───────────────────┤ ├──── GND
│  (BOOT)  │  (wewn. pull-up)
│          │
│  GPIO 2  ├──── [R 330Ω] ──── LED ──── GND
│  (LED)   │
│          │
│  GND     ├──── GND
└──────────┘

GPIO 0 ma wbudowany przycisk BOOT na NodeMCU-32 — nie trzeba nic dodawać!
```

### 7.3 Filtr sprzętowy (Glitch Filter)

ESP-IDF od wersji 5.x oferuje **sprzętowy filtr** GPIO, który odrzuca impulsy krótsze niż ustawiony próg:

```c
#include "driver/gpio_filter.h"

void setup_hardware_glitch_filter(void)
{
    // Konfiguracja sprzętowego filtru glitch
    gpio_glitch_filter_handle_t filter = NULL;

    gpio_pin_glitch_filter_config_t filter_config = {
        .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT,
        .gpio_num = GPIO_NUM_0,
    };

    // Utwórz filtr (odrzuca impulsy < ~12.5 ns przy 80 MHz APB)
    gpio_new_pin_glitch_filter(&filter_config, &filter);

    // Włącz filtr
    gpio_glitch_filter_enable(filter);

    ESP_LOGI(TAG, "Sprzętowy filtr glitch włączony na GPIO %d", GPIO_NUM_0);
}
```

> **⚠️ Uwaga:** Sprzętowy Glitch Filter ESP32 filtruje bardzo krótkie impulsy (nanosekundy). Do debouncingu mechanicznego przycisku (milisekundy) potrzebny jest **filtr programowy** lub sprzętowy RC.

### 7.4 Filtr sprzętowy RC (zewnętrzny)

Prosty filtr RC wygładza drgania styków:

```
           Przycisk
              │
    GPIO ─────┤ ├──── GND
              │
             [C] 100nF  ← kondensator do GND
              │
             GND

Stała czasowa: τ = R_pullup × C
  Wewn. pull-up: τ = 45kΩ × 100nF = 4.5 ms
  Zewn. 10kΩ:    τ = 10kΩ × 100nF = 1.0 ms

Efekt: impulsy krótsze niż ~3τ (3-13 ms) są wygładzane
```

### 7.5 Filtr programowy — debouncing w kodzie

```c
/* ==========================================================
 * Moduł 1.1 — Ćwiczenie 2: Debouncing przycisku
 *
 * Demonstruje:
 * - Przerwania GPIO z kolejką
 * - Programowy debouncing (timer)
 * - Sprzętowy filtr glitch (jeśli dostępny)
 * - Toggle LED przy stabilnym naciśnięciu
 * ========================================================== */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "DEBOUNCE";

#define BUTTON_GPIO         GPIO_NUM_0
#define LED_GPIO            GPIO_NUM_2
#define DEBOUNCE_TIME_MS    50      // Czas debouncingu (ms)

static QueueHandle_t gpio_evt_queue = NULL;
static int led_state = 0;

// ══════════════════════════════════════════════════
// ISR — przerwanie GPIO (jak najkrótsze!)
// ══════════════════════════════════════════════════
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// ══════════════════════════════════════════════════
// Task z programowym debouncingiem
// ══════════════════════════════════════════════════
static void debounce_task(void *arg)
{
    uint32_t gpio_num;
    int64_t last_press_time = 0;

    while (1) {
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY)) {
            // Pobierz aktualny czas (mikro sekundy)
            int64_t now = esp_timer_get_time();

            // Sprawdź czy minął czas debouncingu
            if ((now - last_press_time) > (DEBOUNCE_TIME_MS * 1000)) {
                last_press_time = now;

                // Sprawdź aktualny stan przycisku (potwierdzenie)
                if (gpio_get_level(gpio_num) == 0) {  // Przycisk wciśnięty (LOW)
                    led_state = !led_state;
                    gpio_set_level(LED_GPIO, led_state);
                    ESP_LOGI(TAG, "Przycisk GPIO[%lu] → LED = %s",
                             (unsigned long)gpio_num,
                             led_state ? "ON" : "OFF");
                }
            } else {
                // Bounce — ignoruj
                ESP_LOGD(TAG, "Bounce odfiltrowany (delta = %lld µs)",
                         (long long)(now - last_press_time));
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 1.1: Debouncing przycisku ===");

    // Konfiguracja LED
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_GPIO, 0);

    // Konfiguracja przycisku z przerwaniem
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,        // Zbocze opadające
    };
    gpio_config(&btn_conf);

    // Kolejka zdarzeń
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Instalacja serwisu ISR
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);

    // Task debouncingu
    xTaskCreate(debounce_task, "debounce", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Gotowe! Naciśnij BOOT aby toggle LED.");
    ESP_LOGI(TAG, "Debounce time: %d ms", DEBOUNCE_TIME_MS);
}
```

### 7.6 Porównanie metod debouncingu

| Metoda | Opóźnienie | CPU | Elementy | Niezawodność |
|--------|-----------|-----|----------|-------------|
| **Brak** | 0 ms | ❌ Wielokrotne ISR | Brak | ❌ Fałszywe triggery |
| **Programowy (timer)** | 20–50 ms | ✅ Minimalne | Brak | ✅ Dobra |
| **Sprzętowy RC** | 1–15 ms | ✅ Zero | Rezystor + kondensator | ✅ Bardzo dobra |
| **Sprzętowy glitch filter** | ~ns | ✅ Zero | Brak (wbudowany) | ⚠️ Za krótki na bounce |
| **RC + programowy** | 20–50 ms | ✅ Minimalne | R + C | ✅✅ Najlepsza |

**Rekomendacja:** Użyj **kombinacji** — filtr RC (100nF kondensator) wygładza drgania sprzętowo, a filtr programowy z 30–50 ms oknem eliminuje pozostałe artefakty.

---

## 8. Podsumowanie i dalsze kroki

### Podsumowanie API GPIO

| Funkcja | Opis |
|---------|------|
| `gpio_config()` | Konfiguracja pinu (tryb, pull, przerwanie) |
| `gpio_set_level()` | Ustawienie stanu wyjścia (0 / 1) |
| `gpio_get_level()` | Odczyt stanu pinu |
| `gpio_set_direction()` | Zmiana trybu pinu w runtime |
| `gpio_set_pull_mode()` | Zmiana pull-up/down w runtime |
| `gpio_reset_pin()` | Reset pinu do stanu domyślnego |
| `gpio_install_isr_service()` | Instalacja serwisu przerwań GPIO |
| `gpio_isr_handler_add()` | Dodanie handlera ISR do pinu |
| `gpio_isr_handler_remove()` | Usunięcie handlera ISR |
| `gpio_intr_enable/disable()` | Włączenie/wyłączenie przerwania |
| `rtc_gpio_init()` | Inicjalizacja pinu jako RTC GPIO |
| `rtc_gpio_hold_en()` | Utrzymanie stanu pinu w deep sleep |
| `esp_sleep_enable_ext0_wakeup()` | Wybudzenie z deep sleep (1 pin) |
| `esp_sleep_enable_ext1_wakeup()` | Wybudzenie z deep sleep (wiele pinów) |

### Dalsze kroki

- **Moduł 1.2 — GPTimer** → Precyzyjne odmierzanie czasu, generowanie sygnału prostokątnego
- **Moduł 1.3 — PCNT** → Sprzętowe zliczanie impulsów, enkodery obrotowe
- **Moduł 1.4 — LEDC** → Sprzętowy PWM, sterowanie jasnością LED

---

## 9. Źródła i dokumentacja

| Zasób | Link |
|-------|------|
| **GPIO API Reference** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html) |
| **RTC GPIO (Sleep Modes)** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/sleep_modes.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/sleep_modes.html) |
| **ESP32 Datasheet (GPIO)** | `esp32_datasheet_en.pdf` — Rozdział 4.1 |
| **ESP32 Technical Reference** | `esp32_technical_reference_manual_en.pdf` — Rozdział 4 (IO_MUX and GPIO Matrix) |
| **Przykłady ESP-IDF** | `examples/peripherals/gpio/generic_gpio/` |
| **NodeMCU-32 Schematic** | `NODEMCU32-V1.3.PDF` |
