# Moduł 3.1 — UART (Universal Asynchronous Receiver/Transmitter)

> **Poziom:** 🟡 Początkujący · **Czas:** Tydzień 10–12 (Faza 3)
> **Cel:** Opanowanie komunikacji UART w ESP32 — konfiguracja parametrów (baud rate, bity danych, parzystość, stop bity), instalacja drivera z buforami TX/RX (ring buffer), obsługa przerwań UART vs polling, wzorzec zdarzeń z `uart_pattern_queue_reset()`, oraz praktyczne ćwiczenie komunikacji z PC.

---

## Spis treści

1. [Czym jest UART?](#1-czym-jest-uart)
2. [Architektura UART w ESP32](#2-architektura-uart-w-esp32)
3. [Konfiguracja parametrów komunikacji](#3-konfiguracja-parametrów-komunikacji)
4. [Instalacja drivera — uart_driver_install()](#4-instalacja-drivera--uart_driver_install)
5. [Wysyłanie i odbieranie danych](#5-wysyłanie-i-odbieranie-danych)
6. [Przerwania UART vs Polling](#6-przerwania-uart-vs-polling)
7. [System zdarzeń UART — Event Queue](#7-system-zdarzeń-uart--event-queue)
8. [Detekcja wzorców — Pattern Detection](#8-detekcja-wzorców--pattern-detection)
9. [Usuwanie drivera i zarządzanie zasobami](#9-usuwanie-drivera-i-zarządzanie-zasobami)
10. [Ćwiczenie: Komunikacja z PC — czujniki + komendy](#10-ćwiczenie-komunikacja-z-pc--czujniki--komendy)
11. [Podsumowanie i dalsze kroki](#11-podsumowanie-i-dalsze-kroki)
12. [Źródła i dokumentacja](#12-źródła-i-dokumentacja)

---

## 1. Czym jest UART?

### 1.1 Zasada działania

**UART (Universal Asynchronous Receiver/Transmitter)** to sprzętowy interfejs do asynchronicznej komunikacji szeregowej. W odróżnieniu od interfejsów synchronicznych (SPI, I²C) UART **nie wymaga wspólnego sygnału zegarowego** — nadajnik i odbiornik muszą jedynie uzgodnić prędkość transmisji (baud rate).

```
Komunikacja UART — ramka danych:

  Linia w stanie idle: HIGH (1)
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  IDLE ─┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌─ IDLE
  │        │ │D0│ │D1│ │D2│ │D3│ │D4│ │D5│ │D6│ │D7│ │P │ │
  │        └─┘  └─┘  └─┘  └─┘  └─┘  └─┘  └─┘  └─┘  └─┘  └─┘
  │        ↑                                          ↑   ↑  ↑
  │      START                                       bit  P STOP
  │       bit                                        7       bit
  │                                                            │
  │  1 ramka = [START] + [5-8 bitów danych] + [parzystość] + [STOP]
  └────────────────────────────────────────────────────────────┘

  Start bit:  zawsze LOW (0) — sygnalizuje początek ramki
  Data bits:  5, 6, 7 lub 8 bitów danych (LSB first)
  Parity bit: opcjonalny — even, odd lub brak
  Stop bit:   1, 1.5 lub 2 bity — zawsze HIGH (1)
```

### 1.2 Parametry komunikacji

| Parametr | Typowe wartości | Najczęściej używane |
|----------|----------------|---------------------|
| **Baud rate** | 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 | **115200** |
| **Data bits** | 5, 6, 7, 8 | **8** |
| **Parity** | None, Even, Odd | **None** |
| **Stop bits** | 1, 1.5, 2 | **1** |
| **Flow control** | None, RTS/CTS (hardware), XON/XOFF (software) | **None** |

> **💡 Standardowa konfiguracja:** `115200 8N1` oznacza: 115200 baud, 8 bitów danych, No parity, 1 stop bit. Jest to de facto standard dla komunikacji z konsolą/terminalem.

### 1.3 Obliczanie czasu transmisji

```
Czas transmisji jednego bajtu (przy 115200 8N1):

  Bity na ramkę = 1 (start) + 8 (dane) + 0 (parity) + 1 (stop) = 10 bitów
  Czas 1 bitu   = 1 / 115200 ≈ 8.68 µs
  Czas 1 bajtu  = 10 × 8.68 µs ≈ 86.8 µs
  Throughput     = 115200 / 10 = 11520 bajtów/s ≈ 11.25 KB/s

  Porównanie baud rate → throughput (8N1):
    9600 baud   →   960 B/s  ≈ 0.94 KB/s
    115200 baud → 11520 B/s  ≈ 11.25 KB/s
    921600 baud → 92160 B/s  ≈ 90 KB/s
```

---

## 2. Architektura UART w ESP32

### 2.1 Kontrolery UART

ESP32 posiada **3 niezależne kontrolery UART** (UART0, UART1, UART2), z których każdy obsługuje pełny zestaw funkcji:

```
ESP32 — Kontrolery UART
═══════════════════════════════════════════════════════════════

  ┌──────────────────────────────────────────────────────────┐
  │                    ESP32 UART System                      │
  │                                                          │
  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
  │  │   UART0      │  │   UART1      │  │   UART2      │  │
  │  │              │  │              │  │              │  │
  │  │ TX: GPIO1    │  │ TX: GPIO10   │  │ TX: GPIO17   │  │
  │  │ RX: GPIO3    │  │ RX: GPIO9    │  │ RX: GPIO16   │  │
  │  │ RTS: GPIO22  │  │ RTS: GPIO11  │  │ RTS: GPIO7   │  │
  │  │ CTS: GPIO19  │  │ CTS: GPIO6   │  │ CTS: GPIO8   │  │
  │  │              │  │              │  │              │  │
  │  │ ⚠️ Konsola   │  │ ⚠️ Flash SPI │  │ ✅ Wolny     │  │
  │  │   /monitor   │  │ (domyślnie)  │  │   do użycia  │  │
  │  └──────────────┘  └──────────────┘  └──────────────┘  │
  │                                                          │
  │  Cechy wspólne:                                          │
  │  • TX/RX FIFO: 128 bajtów każdy                         │
  │  • Baud rate: do 5 Mbps                                 │
  │  • Piny mapowalne przez GPIO Matrix na dowolne GPIO      │
  │  • Hardware flow control (RTS/CTS)                       │
  │  • Detekcja wzorców (pattern detection)                  │
  │  • Tryby: UART, RS485 half-duplex, IrDA                 │
  └──────────────────────────────────────────────────────────┘
```

### 2.2 Ważne informacje o kontrolerach

| Kontroler | Domyślne piny | Uwagi |
|-----------|--------------|-------|
| **UART0** | TX=GPIO1, RX=GPIO3 | Używany przez **USB-UART** (konsola, `idf.py monitor`). Można go współdzielić, ale ostrożnie! |
| **UART1** | TX=GPIO10, RX=GPIO9 | Domyślne piny podłączone do **wewnętrznego Flash SPI** — **MUSISZ** przemapować na inne GPIO! |
| **UART2** | TX=GPIO17, RX=GPIO16 | Wolny do użycia. Zalecany dla komunikacji z urządzeniami zewnętrznymi |

> **⚠️ KRYTYCZNE:** UART1 domyślne piny (GPIO6–11) są podłączone do wbudowanego Flash SPI. Użycie ich spowoduje crash. **Zawsze** przemapuj UART1 na inne GPIO za pomocą `uart_set_pin()`.

### 2.3 Architektura drivera UART (bufory i przerwania)

```
Architektura wewnętrzna drivera UART ESP-IDF:

  ┌─────────────┐     ┌──────────────────────────────────┐
  │ Aplikacja   │     │          UART Driver              │
  │             │     │                                    │
  │ uart_write_ │────►│  ┌─────────────┐   ┌──────────┐  │
  │ bytes()     │     │  │ TX Ring     │──►│ TX FIFO  │──►── TX pin
  │             │     │  │ Buffer     │   │ (128 B)  │  │
  │             │     │  │ (user size)│   └──────────┘  │
  │             │     │  └─────────────┘                  │
  │             │     │                                    │
  │ uart_read_  │◄───│  ┌─────────────┐   ┌──────────┐  │
  │ bytes()     │     │  │ RX Ring     │◄──│ RX FIFO  │◄─── RX pin
  │             │     │  │ Buffer     │   │ (128 B)  │  │
  │             │     │  │ (user size)│   └──────────┘  │
  │             │     │  └─────────────┘                  │
  │             │     │                                    │
  │ xQueueRcv() │◄───│  ┌─────────────┐                  │
  │             │     │  │ Event Queue │ ◄── ISR (przerwania)
  │             │     │  │ (FreeRTOS)  │                  │
  │             │     │  └─────────────┘                  │
  └─────────────┘     └──────────────────────────────────┘

  FIFO (sprzętowy): 128 bajtów TX + 128 bajtów RX
  Ring Buffer (RAM): rozmiar konfigurowany przez użytkownika
  Event Queue: opcjonalna kolejka zdarzeń UART (FreeRTOS)
```

**Przepływ danych TX:**
1. `uart_write_bytes()` kopiuje dane do **TX Ring Buffer** (RAM)
2. ISR automatycznie przenosi dane z Ring Buffer → **TX FIFO** (sprzętowy, 128B)
3. UART FSM serializuje dane z FIFO i wysyła na pin TX

**Przepływ danych RX:**
1. UART FSM odbiera dane z pinu RX → **RX FIFO** (sprzętowy, 128B)
2. ISR automatycznie przenosi dane z RX FIFO → **RX Ring Buffer** (RAM)
3. `uart_read_bytes()` odczytuje dane z Ring Buffer

---

## 3. Konfiguracja parametrów komunikacji

### 3.1 Nagłówek i zależności

```c
#include "driver/uart.h"       // Główny nagłówek UART API
#include "driver/gpio.h"       // Mapowanie pinów (opcjonalnie)
```

W `CMakeLists.txt` komponent `driver` jest dodawany automatycznie.

### 3.2 Metoda 1: Konfiguracja jednoetapowa — uart_param_config()

Wszystkie parametry ustawiane jednocześnie przez strukturę `uart_config_t`:

```c
const uart_port_t uart_num = UART_NUM_2;

uart_config_t uart_config = {
    .baud_rate  = 115200,                    // Prędkość transmisji
    .data_bits  = UART_DATA_8_BITS,          // 8 bitów danych
    .parity     = UART_PARITY_DISABLE,       // Brak bitu parzystości
    .stop_bits  = UART_STOP_BITS_1,          // 1 bit stopu
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,  // Brak kontroli przepływu
    .source_clk = UART_SCLK_DEFAULT,         // Domyślne źródło zegara (APB)
};

// Zastosuj konfigurację
ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
```

### 3.3 Struktura uart_config_t — pola

| Pole | Typ | Opis |
|------|-----|------|
| `baud_rate` | `int` | Prędkość transmisji (bps). Max: `UART_BITRATE_MAX` (~5 Mbps) |
| `data_bits` | `uart_word_length_t` | `UART_DATA_5_BITS` / `6` / `7` / **`UART_DATA_8_BITS`** |
| `parity` | `uart_parity_t` | **`UART_PARITY_DISABLE`** / `UART_PARITY_EVEN` / `UART_PARITY_ODD` |
| `stop_bits` | `uart_stop_bits_t` | **`UART_STOP_BITS_1`** / `UART_STOP_BITS_1_5` / `UART_STOP_BITS_2` |
| `flow_ctrl` | `uart_hw_flowcontrol_t` | **`UART_HW_FLOWCTRL_DISABLE`** / `CTS` / `RTS` / `CTS_RTS` |
| `rx_flow_ctrl_thresh` | `uint8_t` | Próg RTS (0–127). Aktywne gdy flow_ctrl ≠ DISABLE |
| `source_clk` | `uart_sclk_t` | **`UART_SCLK_DEFAULT`** (APB 80 MHz) / `UART_SCLK_REF_TICK` |

### 3.4 Metoda 2: Konfiguracja wieloetapowa

Poszczególne parametry można ustawiać indywidualnie:

```c
// Zmiana baud rate w trakcie działania
ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_2, 9600));

// Odczyt aktualnego baud rate
uint32_t current_baud;
ESP_ERROR_CHECK(uart_get_baudrate(UART_NUM_2, &current_baud));
ESP_LOGI(TAG, "Aktualny baud rate: %lu", (unsigned long)current_baud);
```

| Parametr | Setter | Getter |
|----------|--------|--------|
| Baud rate | `uart_set_baudrate()` | `uart_get_baudrate()` |
| Data bits | `uart_set_word_length()` | `uart_get_word_length()` |
| Parity | `uart_set_parity()` | `uart_get_parity()` |
| Stop bits | `uart_set_stop_bits()` | `uart_get_stop_bits()` |
| Flow control | `uart_set_hw_flow_ctrl()` | `uart_get_hw_flow_ctrl()` |

### 3.5 Mapowanie pinów — uart_set_pin()

ESP32 posiada GPIO Matrix, która pozwala mapować sygnały UART na dowolne GPIO:

```c
// Mapowanie pinów UART2 na GPIO17 (TX) i GPIO16 (RX)
ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2,
    17,                    // TX pin
    16,                    // RX pin
    UART_PIN_NO_CHANGE,    // RTS — nie zmieniaj / nie używaj
    UART_PIN_NO_CHANGE     // CTS — nie zmieniaj / nie używaj
));
```

> **💡 `UART_PIN_NO_CHANGE`** — makro oznaczające "pozostaw obecne przypisanie" lub "nie używaj tego pinu". Używaj go dla sygnałów RTS/CTS gdy nie potrzebujesz hardware flow control.

---

## 4. Instalacja drivera — uart_driver_install()

### 4.1 Sygnatura funkcji

```c
esp_err_t uart_driver_install(
    uart_port_t uart_num,        // Numer portu: UART_NUM_0 / 1 / 2
    int rx_buffer_size,          // Rozmiar RX ring buffer (bajty)
    int tx_buffer_size,          // Rozmiar TX ring buffer (0 = blokujący)
    int queue_size,              // Rozmiar kolejki zdarzeń (0 = brak)
    QueueHandle_t *uart_queue,   // Wskaźnik na uchwyt kolejki (NULL = brak)
    int intr_alloc_flags         // Flagi alokacji przerwań
);
```

### 4.2 Parametry szczegółowo

| Parametr | Opis | Zalecenie |
|----------|------|-----------|
| `rx_buffer_size` | Rozmiar RX ring buffer. **Musi być > `UART_HW_FIFO_LEN`** (128B) | Min. 256, typowo **1024–2048** |
| `tx_buffer_size` | Rozmiar TX ring buffer. Jeśli **0** → `uart_write_bytes()` blokuje do wysłania danych | Typowo **1024** lub **0** |
| `queue_size` | Głębokość kolejki zdarzeń FreeRTOS. **0** = brak kolejki | Typowo **10–20** |
| `uart_queue` | Wskaźnik na zmienną `QueueHandle_t`. **NULL** = brak kolejki | Podaj jeśli queue_size > 0 |
| `intr_alloc_flags` | Flagi `ESP_INTR_FLAG_*`. **NIE** ustawiaj `ESP_INTR_FLAG_IRAM` | Typowo **0** |

### 4.3 Przykład — pełna inicjalizacja UART

```c
#include "driver/uart.h"
#include "esp_log.h"

#define UART_PORT       UART_NUM_2
#define UART_TX_PIN     17
#define UART_RX_PIN     16
#define BUF_SIZE        1024

static QueueHandle_t uart_queue;

static void uart_init(void)
{
    // ═══ KROK 1: Konfiguracja parametrów ═══
    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

    // ═══ KROK 2: Mapowanie pinów ═══
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,
        UART_TX_PIN, UART_RX_PIN,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // ═══ KROK 3: Instalacja drivera ═══
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,
        BUF_SIZE * 2,    // RX buffer: 2048 bajtów
        BUF_SIZE,         // TX buffer: 1024 bajtów
        20,               // Event queue: 20 zdarzeń
        &uart_queue,      // Uchwyt kolejki zdarzeń
        0));              // Brak specjalnych flag przerwań
}
```

### 4.4 Ring Buffer — jak działa

```
Ring Buffer (bufor cykliczny) — schemat działania:

  Pamięć bufora (np. 2048 bajtów):
  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │   │   │ D │ A │ T │ A │   │   │   │   │   │   │
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
            ↑               ↑
          READ            WRITE
         pointer          pointer

  Zapis: nowe dane wpisywane na pozycji WRITE (ISR: FIFO → Ring)
  Odczyt: dane czytane od pozycji READ (uart_read_bytes)
  Gdy WRITE dochodzi do końca → zawija się na początek (circular)
  Gdy READ = WRITE → bufor pusty
  Gdy WRITE dogoni READ → bufor pełny (UART_BUFFER_FULL event)
```

> **⚠️ Ważne:** Jeśli `tx_buffer_size = 0`, driver **nie** tworzy TX ring buffer. W tym trybie `uart_write_bytes()` zapisuje bezpośrednio do TX FIFO i **blokuje** task aż wszystkie dane zostaną wysłane. Przydatne gdy chcesz mieć pewność, że dane zostały wysłane przed kontynuacją.

---

## 5. Wysyłanie i odbieranie danych

### 5.1 Wysyłanie — uart_write_bytes()

```c
// Wysłanie stringa
char *msg = "Hello from ESP32!\r\n";
int bytes_sent = uart_write_bytes(UART_PORT, msg, strlen(msg));
// Zwraca liczbę zapisanych bajtów (do ring buffer, nie na kabel!)

// Wysłanie danych binarnych
uint8_t data[] = {0x01, 0x02, 0x03, 0xFF};
uart_write_bytes(UART_PORT, (const char *)data, sizeof(data));
```

**Warianty wysyłania:**

| Funkcja | Opis |
|---------|------|
| `uart_write_bytes()` | Kopiuje do TX ring buffer, blokuje jeśli bufor pełny |
| `uart_write_bytes_with_break()` | Jak wyżej + dodaje sygnał BREAK na końcu |
| `uart_tx_chars()` | Pisze bezpośrednio do TX FIFO, **nie blokuje**, zwraca ile zmieścił |
| `uart_wait_tx_done()` | Czeka aż TX FIFO będzie pusty (wsz. dane wysłane) |

```c
// Wysyłanie z oczekiwaniem na zakończenie transmisji
uart_write_bytes(UART_PORT, msg, strlen(msg));
ESP_ERROR_CHECK(uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(1000)));
// Teraz mamy 100% pewność, że dane wyszły na kabel
```

### 5.2 Odbieranie — uart_read_bytes()

```c
uint8_t rx_buf[128];

// Odczyt z timeoutem 100ms — blokuje task do momentu
// odebrania danych lub upłynięcia timeout
int len = uart_read_bytes(UART_PORT, rx_buf, sizeof(rx_buf),
                          pdMS_TO_TICKS(100));
if (len > 0) {
    rx_buf[len] = '\0';  // Null-terminate jeśli to string
    ESP_LOGI(TAG, "Odebrano %d bajtów: %s", len, (char *)rx_buf);
} else if (len == 0) {
    // Timeout — brak danych
}
```

### 5.3 Pomocnicze funkcje odbioru

```c
// Sprawdzenie ile bajtów czeka w RX ring buffer
size_t available;
ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, &available));
ESP_LOGI(TAG, "Dostępne bajty: %d", available);

// Wyczyszczenie RX ring buffer (porzuć wszystkie dane)
ESP_ERROR_CHECK(uart_flush(UART_PORT));          // Flush RX + czeka na TX
ESP_ERROR_CHECK(uart_flush_input(UART_PORT));     // Flush tylko RX
```

---

## 6. Przerwania UART vs Polling

### 6.1 Metoda 1: Polling (odpytywanie)

Polling polega na cyklicznym sprawdzaniu bufora RX w pętli:

```c
// ══ POLLING — prosta pętla odczytu ══
static void uart_polling_task(void *arg)
{
    uint8_t buf[128];
    while (1) {
        // Blokuje task na max 20ms, potem sprawdza ponownie
        int len = uart_read_bytes(UART_PORT, buf, sizeof(buf),
                                  pdMS_TO_TICKS(20));
        if (len > 0) {
            buf[len] = '\0';
            ESP_LOGI(TAG, "Polling: odebrano: %s", (char *)buf);
            // Echo — odesłanie odebranych danych
            uart_write_bytes(UART_PORT, (const char *)buf, len);
        }
        // Inne operacje w pętli...
    }
}
```

### 6.2 Metoda 2: Event Queue (zdarzenia z przerwaniami)

Driver UART konwertuje przerwania sprzętowe na zdarzenia FreeRTOS:

```c
// ══ EVENT QUEUE — reakcja na zdarzenia ══
static void uart_event_task(void *arg)
{
    uart_event_t event;
    uint8_t buf[128];

    while (1) {
        // Blokuje task do momentu pojawienia się zdarzenia
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
            case UART_DATA:
                // Dane odebrane — event.size = liczba bajtów
                uart_read_bytes(UART_PORT, buf, event.size,
                                pdMS_TO_TICKS(100));
                buf[event.size] = '\0';
                ESP_LOGI(TAG, "Data (%d B): %s", event.size, (char *)buf);
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "HW FIFO overflow!");
                uart_flush_input(UART_PORT);
                xQueueReset(uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring buffer full!");
                uart_flush_input(UART_PORT);
                xQueueReset(uart_queue);
                break;

            default:
                ESP_LOGI(TAG, "Event type: %d", event.type);
                break;
            }
        }
    }
}
```

### 6.3 Porównanie metod

```
Polling vs Event Queue:
════════════════════════════════════════════════════════════

  Cecha              Polling                Event Queue
  ────────────────── ────────────────────── ────────────────
  Mechanizm          Pętla + uart_read()    ISR → Queue → Task
  Latencja           Zależy od częstości    Natychmiastowa
                     odpytywania            (przerwanie)
  Zużycie CPU        Wyższe (ciągła pętla) Niższe (task śpi)
  Złożoność kodu     Prosta                 Średnia
  Obsługa błędów     Manualna               Automatyczna (eventy)
  Detekcja wzorców   ❌ Brak               ✅ UART_PATTERN_DET
  Wykrywanie FIFO    ❌ Brak               ✅ UART_FIFO_OVF
    overflow
  Zalecane użycie    Proste projekty,       Produkcja, złożone
                     prototypy              protokoły, IoT
```

> **💡 Zalecenie:** W większości przypadków używaj **Event Queue** — jest bardziej efektywna i bezpieczna. Polling jest OK tylko dla prostych testów lub gdy nie potrzebujesz natychmiastowej reakcji na dane.

---

## 7. System zdarzeń UART — Event Queue

### 7.1 Typy zdarzeń (uart_event_type_t)

Driver UART generuje zdarzenia na podstawie przerwań sprzętowych i pakuje je w strukturę `uart_event_t`:

```c
typedef struct {
    uart_event_type_t type;   // Typ zdarzenia
    size_t size;              // Rozmiar danych (dla UART_DATA)
    bool timeout_flag;        // Flaga timeout (dla UART_DATA)
} uart_event_t;
```

| Zdarzenie | Opis | Kiedy występuje |
|-----------|------|-----------------|
| **`UART_DATA`** | Odebrano dane | Dane w RX FIFO + timeout lub próg |
| **`UART_FIFO_OVF`** | Przepełnienie sprzętowego FIFO | RX FIFO (128B) pełny, nowe dane tracone |
| **`UART_BUFFER_FULL`** | Przepełnienie ring buffera | RX ring buffer pełny |
| **`UART_BREAK`** | Wykryto BREAK | Linia RX w stanie LOW dłużej niż ramka |
| **`UART_PARITY_ERR`** | Błąd parzystości | Odebrany bit parzystości nie zgadza się |
| **`UART_FRAME_ERR`** | Błąd ramki | Brak stop bitu w oczekiwanym miejscu |
| **`UART_PATTERN_DET`** | Wykryto wzorzec | Wykryto powtórzenie znaku (pattern) |
| **`UART_DATA_BREAK`** | Dane + BREAK | Dane zakończone sygnałem BREAK |
| **`UART_EVENT_MAX`** | — | Wartość graniczna enum |

### 7.2 Kompletna obsługa zdarzeń — wzorzec produkcyjny

```c
static void uart_event_task(void *arg)
{
    uart_event_t event;
    uint8_t *buf = (uint8_t *)malloc(BUF_SIZE);

    while (1) {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {
            memset(buf, 0, BUF_SIZE);

            switch (event.type) {
            case UART_DATA:
                ESP_LOGI(TAG, "[DATA] %d bajtów", event.size);
                uart_read_bytes(UART_PORT, buf, event.size,
                                pdMS_TO_TICKS(100));
                // Przetwarzanie odebranych danych...
                break;

            case UART_FIFO_OVF:
                // ⚠️ Sprzętowy FIFO pełny — utrata danych!
                ESP_LOGW(TAG, "[FIFO OVF] Hardware FIFO overflow!");
                // Rozwiązanie: flush + reset kolejki
                uart_flush_input(UART_PORT);
                xQueueReset(uart_queue);
                break;

            case UART_BUFFER_FULL:
                // ⚠️ Ring buffer pełny — zwiększ rozmiar bufora
                ESP_LOGW(TAG, "[BUF FULL] Ring buffer full!");
                uart_flush_input(UART_PORT);
                xQueueReset(uart_queue);
                break;

            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "[PARITY] Błąd parzystości!");
                break;

            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "[FRAME] Błąd ramki!");
                break;

            case UART_BREAK:
                ESP_LOGW(TAG, "[BREAK] Wykryto sygnał BREAK");
                break;

            case UART_PATTERN_DET:
                ESP_LOGI(TAG, "[PATTERN] Wykryto wzorzec!");
                // Patrz sekcja 8 — Pattern Detection
                break;

            default:
                ESP_LOGI(TAG, "Inny event: %d", event.type);
                break;
            }
        }
    }
    free(buf);
    vTaskDelete(NULL);
}
```

### 7.3 Konfiguracja przerwań — uart_intr_config()

Dla zaawansowanych zastosowań można skonfigurować progi przerwań bezpośrednio:

```c
uart_intr_config_t uart_intr = {
    .intr_enable_mask = UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT,
    .rxfifo_full_thresh = 100,   // Przerwanie gdy FIFO ma 100+ bajtów
    .rx_timeout_thresh  = 10,    // Timeout: 10 × czas jednego bajtu
};
ESP_ERROR_CHECK(uart_intr_config(UART_PORT, &uart_intr));
ESP_ERROR_CHECK(uart_enable_rx_intr(UART_PORT));
```

> **💡 Praktyka:** W większości przypadków konfiguracja przerwań jest automatyczna po `uart_driver_install()`. Ręczna konfiguracja jest potrzebna tylko gdy chcesz precyzyjnie kontrolować progi.

---

## 8. Detekcja wzorców — Pattern Detection

### 8.1 Czym jest Pattern Detection?

UART ESP32 posiada sprzętowy mechanizm detekcji **powtarzającego się znaku** (pattern). Gdy odebrane zostanie N kolejnych identycznych znaków, generowane jest przerwanie `UART_PATTERN_DET`.

```
Pattern Detection — jak działa:

  Przykład: detekcja 3× znak '+' (AT-komenda: "+++")

  Strumień danych:  H E L L O + + + W O R L D
                                ↑ ↑ ↑
                              PATTERN DETECTED!
                              (3 × '+' pod rząd)

  Zastosowania:
  • Przełączanie trybu AT-komend (np. moduły GSM/WiFi)
  • Separator ramek protokołu ("###" lub "!!!")
  • Wykrywanie końca wiadomości (np. 3× '\n')
```

### 8.2 Konfiguracja — uart_enable_pattern_det_baud_intr()

```c
// Detekcja wzorca: 3× znak '+' (jak AT-komenda "+++")
#define PATTERN_CHR    '+'    // Znak wzorca
#define PATTERN_NUM    3      // Liczba powtórzeń

uart_enable_pattern_det_baud_intr(UART_PORT,
    PATTERN_CHR,    // Znak do detekcji
    PATTERN_NUM,    // Liczba powtórzeń
    9,              // Timeout między znakami (9 bit-periods)
    0,              // Post-idle timeout (0 = wyłączony)
    0               // Pre-idle timeout (0 = wyłączony)
);

// WAŻNE: Reset kolejki pozycji wzorców (max 20 pozycji)
uart_pattern_queue_reset(UART_PORT, 20);
```

### 8.3 uart_pattern_queue_reset() — szczegóły

Funkcja `uart_pattern_queue_reset()` tworzy/resetuje wewnętrzną kolejkę **pozycji** wykrytych wzorców w buforze RX. Jest to kluczowe dla prawidłowego odczytu danych przed i po wzorcu:

```c
// Reset kolejki do przechowywania max 20 pozycji wzorców
uart_pattern_queue_reset(UART_PORT, 20);
```

```
Kolejka pozycji wzorców — schemat:

  RX Ring Buffer zawartość:
  ┌───────────────────────────────────────┐
  │ H E L L O + + + W O R L D + + + E N D│
  └───────────────────────────────────────┘
  pozycja:  0 1 2 3 4 5 6 7 8 ...   14 15 16

  Kolejka pozycji wzorców:
  ┌─────┬──────┐
  │  5  │  14  │    ← pozycje pierwszego znaku wzorca
  └─────┴──────┘

  uart_pattern_pop_pos() zwraca: 5 (pierwszy wzorzec)
  → czytasz 5 bajtów danych ("HELLO")
  → czytasz 3 bajty wzorca ("+++")
  → kolejny pop: 14, itd.
```

### 8.4 Obsługa zdarzenia UART_PATTERN_DET

```c
case UART_PATTERN_DET: {
    // Sprawdź ile danych jest w buforze
    size_t buffered_size;
    uart_get_buffered_data_len(UART_PORT, &buffered_size);

    // Pobierz pozycję wzorca z kolejki
    int pos = uart_pattern_pop_pos(UART_PORT);
    ESP_LOGI(TAG, "Pattern @ pos %d, buffered: %d", pos, buffered_size);

    if (pos == -1) {
        // Kolejka pozycji pełna — dane utracone
        // Rozwiązanie: zwiększ rozmiar w uart_pattern_queue_reset()
        ESP_LOGW(TAG, "Pattern queue full, flushing!");
        uart_flush_input(UART_PORT);
    } else {
        // Odczytaj dane PRZED wzorcem
        uint8_t *pre_data = malloc(pos + 1);
        uart_read_bytes(UART_PORT, pre_data, pos, pdMS_TO_TICKS(100));
        pre_data[pos] = '\0';
        ESP_LOGI(TAG, "Dane przed wzorcem: '%s'", (char *)pre_data);
        free(pre_data);

        // Odczytaj sam wzorzec (PATTERN_NUM bajtów)
        uint8_t pattern[PATTERN_NUM + 1];
        uart_read_bytes(UART_PORT, pattern, PATTERN_NUM,
                        pdMS_TO_TICKS(100));
        pattern[PATTERN_NUM] = '\0';
        ESP_LOGI(TAG, "Wzorzec: '%s'", (char *)pattern);
    }
    break;
}
```

> **⚠️ WAŻNE:** Jeśli `uart_pattern_pop_pos()` zwraca **-1**, oznacza to, że kolejka pozycji jest pełna. Wywołaj `uart_pattern_queue_reset()` z większą wartością lub `uart_flush_input()`.

---

## 9. Usuwanie drivera i zarządzanie zasobami

### 9.1 uart_driver_delete()

```c
// Usunięcie drivera — zwolnienie wszystkich zasobów
ESP_ERROR_CHECK(uart_driver_delete(UART_PORT));
// Ring buffers, event queue, przerwania — wszystko zwolnione
```

### 9.2 Sprawdzanie stanu drivera

```c
if (uart_is_driver_installed(UART_PORT)) {
    ESP_LOGI(TAG, "Driver UART%d jest zainstalowany", UART_PORT);
}
```

### 9.3 Sekwencja cyklu życia drivera

```
Cykl życia drivera UART:

  1. uart_param_config()       ← Konfiguracja parametrów
  2. uart_set_pin()            ← Mapowanie GPIO
  3. uart_driver_install()     ← Alokacja zasobów (bufory, ISR)
  ─── Driver aktywny ───
  4. uart_write_bytes()        ← Wysyłanie danych
  5. uart_read_bytes()         ← Odbieranie danych
  6. xQueueReceive()           ← Obsługa zdarzeń
  ─── Zakończenie ───
  7. uart_driver_delete()      ← Zwolnienie zasobów
```

---

## 10. Ćwiczenie: Komunikacja z PC — czujniki + komendy

### 10.1 Cel ćwiczenia

Budowa dwukierunkowej komunikacji UART z PC:
1. **ESP32 → PC:** Cykliczne wysyłanie danych z czujników (ADC, wewnętrzny pomiar temperatury)
2. **PC → ESP32:** Odbiór komend tekstowych sterujących LED i parametrami
3. **Pattern Detection:** Użycie wzorca `+++` do przełączania trybu AT-komend

### 10.2 Schemat podłączenia

```
ESP32 NodeMCU-32 — komunikacja z PC
┌──────────────────┐
│                  │         Kabel USB
│  USB ────────────┼──────── PC (terminal: idf.py monitor / PuTTY)
│  (UART0)         │         115200 8N1
│                  │
│  GPIO4 (ADC1_0)──┼──── [LDR + 10kΩ] ──── Vcc/GND (czujnik światła)
│                  │
│  GPIO2 ──────────┼──── [LED 🔴] ──── [330Ω] ──── GND
│                  │
│  GND ────────────┼──── GND (wspólna masa)
│                  │
└──────────────────┘

Elementy:
  • NodeMCU-32 (wbudowany USB-UART na UART0)
  • LDR (fotorezystor) + rezystor 10 kΩ (dzielnik napięcia)
  • LED + rezystor 330 Ω
  • Kabel USB (komunikacja + zasilanie)

Komunikacja: przez wbudowany USB-UART (UART0, GPIO1/GPIO3)
  ⚠️ UART0 jest współdzielony z konsolą/monitorem.
  Program używa UART0 do komunikacji z PC.
```

### 10.3 Kompletny kod

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// ═══════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════
static const char *TAG = "UART_PC";

#define UART_PORT           UART_NUM_0      // USB-UART (konsola)
#define UART_BAUD_RATE      115200
#define BUF_SIZE            1024

#define LED_GPIO            GPIO_NUM_2
#define ADC_CHANNEL         ADC_CHANNEL_0   // GPIO36 (VP)
#define ADC_ATTEN           ADC_ATTEN_DB_12

// Pattern detection — 3× '+'
#define PATTERN_CHR         '+'
#define PATTERN_NUM         3

// ═══════════════════════════════════════════
// Zmienne globalne
// ═══════════════════════════════════════════
static QueueHandle_t uart_queue;
static adc_oneshot_unit_handle_t adc_handle;
static bool led_state = false;
static int send_interval_ms = 2000;  // Interwał wysyłania danych
static bool at_mode = false;         // Tryb AT-komend

// ═══════════════════════════════════════════
// Inicjalizacja UART
// ═══════════════════════════════════════════
static void init_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,
        BUF_SIZE * 2, BUF_SIZE, 20, &uart_queue, 0));

    // Pattern detection: 3× '+'
    uart_enable_pattern_det_baud_intr(UART_PORT,
        PATTERN_CHR, PATTERN_NUM, 9, 0, 0);
    uart_pattern_queue_reset(UART_PORT, 20);

    ESP_LOGI(TAG, "UART%d: %d baud, 8N1, pattern='%c'x%d",
             UART_PORT, UART_BAUD_RATE, PATTERN_CHR, PATTERN_NUM);
}

// ═══════════════════════════════════════════
// Inicjalizacja ADC (czujnik światła)
// ═══════════════════════════════════════════
static void init_adc(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle,
        ADC_CHANNEL, &chan_cfg));
    ESP_LOGI(TAG, "ADC1 kanał %d skonfigurowany", ADC_CHANNEL);
}

// ═══════════════════════════════════════════
// Inicjalizacja LED
// ═══════════════════════════════════════════
static void init_led(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(LED_GPIO, 0);
}

// ═══════════════════════════════════════════
// Wysyłanie odpowiedzi UART
// ═══════════════════════════════════════════
static void uart_send(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    uart_write_bytes(UART_PORT, buf, len);
}

// ═══════════════════════════════════════════
// Przetwarzanie komend
// ═══════════════════════════════════════════
static void process_command(const char *cmd)
{
    // Usunięcie \r\n z końca
    char clean[128];
    strncpy(clean, cmd, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    char *p = clean;
    while (*p) {
        if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        p++;
    }

    ESP_LOGI(TAG, "Komenda: '%s'", clean);

    if (strcmp(clean, "LED ON") == 0 || strcmp(clean, "led on") == 0) {
        led_state = true;
        gpio_set_level(LED_GPIO, 1);
        uart_send("OK: LED ON\r\n");
    }
    else if (strcmp(clean, "LED OFF") == 0 || strcmp(clean, "led off") == 0) {
        led_state = false;
        gpio_set_level(LED_GPIO, 0);
        uart_send("OK: LED OFF\r\n");
    }
    else if (strcmp(clean, "STATUS") == 0 || strcmp(clean, "status") == 0) {
        int adc_val;
        adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_val);
        uart_send("STATUS: LED=%s, ADC=%d, interval=%dms\r\n",
                  led_state ? "ON" : "OFF", adc_val, send_interval_ms);
    }
    else if (strncmp(clean, "INTERVAL ", 9) == 0) {
        int val = atoi(clean + 9);
        if (val >= 100 && val <= 60000) {
            send_interval_ms = val;
            uart_send("OK: interval=%dms\r\n", send_interval_ms);
        } else {
            uart_send("ERR: interval 100-60000\r\n");
        }
    }
    else if (strcmp(clean, "HELP") == 0 || strcmp(clean, "help") == 0) {
        uart_send("\r\n=== ESP32 UART Commands ===\r\n");
        uart_send("LED ON       - Włącz LED\r\n");
        uart_send("LED OFF      - Wyłącz LED\r\n");
        uart_send("STATUS       - Stan czujników i LED\r\n");
        uart_send("INTERVAL N   - Interwał wysyłania (ms)\r\n");
        uart_send("HELP         - Ta pomoc\r\n");
        uart_send("+++          - Przełącz tryb AT\r\n");
        uart_send("===========================\r\n");
    }
    else if (strlen(clean) > 0) {
        uart_send("ERR: Unknown command '%s'. Type HELP\r\n", clean);
    }
}

// ═══════════════════════════════════════════
// Task: Obsługa zdarzeń UART (odbiór komend)
// ═══════════════════════════════════════════
static void uart_rx_task(void *arg)
{
    uart_event_t event;
    uint8_t *buf = (uint8_t *)malloc(BUF_SIZE);

    while (1) {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
            case UART_DATA:
                memset(buf, 0, BUF_SIZE);
                uart_read_bytes(UART_PORT, buf, event.size,
                                pdMS_TO_TICKS(100));
                process_command((char *)buf);
                break;

            case UART_PATTERN_DET: {
                size_t buffered;
                uart_get_buffered_data_len(UART_PORT, &buffered);
                int pos = uart_pattern_pop_pos(UART_PORT);

                if (pos >= 0) {
                    // Odczytaj dane przed wzorcem
                    if (pos > 0) {
                        uart_read_bytes(UART_PORT, buf, pos,
                                        pdMS_TO_TICKS(100));
                    }
                    // Odczytaj wzorzec
                    uint8_t pat[PATTERN_NUM];
                    uart_read_bytes(UART_PORT, pat, PATTERN_NUM,
                                    pdMS_TO_TICKS(100));

                    at_mode = !at_mode;
                    uart_send("\r\n*** Tryb AT: %s ***\r\n",
                              at_mode ? "AKTYWNY" : "WYŁĄCZONY");
                } else {
                    uart_flush_input(UART_PORT);
                }
                break;
            }

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "FIFO overflow!");
                uart_flush_input(UART_PORT);
                xQueueReset(uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring buffer full!");
                uart_flush_input(UART_PORT);
                xQueueReset(uart_queue);
                break;

            default:
                break;
            }
        }
    }
    free(buf);
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════
// Task: Wysyłanie danych z czujników
// ═══════════════════════════════════════════
static void sensor_tx_task(void *arg)
{
    uint32_t counter = 0;

    while (1) {
        if (!at_mode) {    // Nie wysyłaj w trybie AT
            int adc_val;
            adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_val);

            // Format CSV: counter, adc_raw, led_state, uptime_ms
            uart_send("DATA,%lu,%d,%d,%lu\r\n",
                      (unsigned long)counter++,
                      adc_val,
                      led_state ? 1 : 0,
                      (unsigned long)(xTaskGetTickCount() *
                                      portTICK_PERIOD_MS));
        }
        vTaskDelay(pdMS_TO_TICKS(send_interval_ms));
    }
}

// ═══════════════════════════════════════════
// Główna funkcja
// ═══════════════════════════════════════════
void app_main(void)
{
    ESP_LOGI(TAG, "=== Moduł 3.1: UART — Komunikacja z PC ===");

    // Inicjalizacja peryferiów
    init_uart();
    init_adc();
    init_led();

    // Wiadomość powitalna
    uart_send("\r\n========================================\r\n");
    uart_send("  ESP32 UART Communication Module 3.1\r\n");
    uart_send("  Baud: %d, Format: 8N1\r\n", UART_BAUD_RATE);
    uart_send("  Type HELP for commands\r\n");
    uart_send("========================================\r\n\r\n");

    // Uruchomienie tasków
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 12, NULL);
    xTaskCreate(sensor_tx_task, "sensor_tx", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "System uruchomiony. Wpisz HELP w terminalu.");
}
```

### 10.4 Oczekiwany wynik

```
========================================
  ESP32 UART Communication Module 3.1
  Baud: 115200, Format: 8N1
  Type HELP for commands
========================================

DATA,0,2847,0,1250
DATA,1,2851,0,3260
DATA,2,2843,0,5270

> LED ON                          ← Komenda z PC
OK: LED ON

DATA,3,2849,1,7280
DATA,4,2123,1,9290                ← Zmiana światła → zmiana ADC

> STATUS                          ← Komenda z PC
STATUS: LED=ON, ADC=2123, interval=2000ms

> INTERVAL 500                    ← Zmiana interwału
OK: interval=500ms

DATA,5,2125,1,9800
DATA,6,2130,1,10300               ← Teraz co 500ms

> +++                             ← Pattern detection
*** Tryb AT: AKTYWNY ***
                                  ← Dane przestają być wysyłane
> +++
*** Tryb AT: WYŁĄCZONY ***
DATA,7,2128,1,15400               ← Dane wznowione
```

### 10.5 Testowanie z PC

```
Sposób 1: idf.py monitor
──────────────────────────
  $ idf.py monitor
  Dane z czujników wyświetlają się automatycznie.
  Wpisuj komendy bezpośrednio w terminalu monitora.
  Ctrl+] aby wyjść.

Sposób 2: PuTTY / RealTerm / minicom
──────────────────────────────────────
  Port: COMx (Windows) lub /dev/ttyUSBx (Linux)
  Baud: 115200
  Data bits: 8, Parity: None, Stop bits: 1
  Flow control: None
  Line ending: CR+LF (\r\n)

Sposób 3: Python (pyserial)
────────────────────────────
  import serial
  ser = serial.Serial('COM3', 115200, timeout=1)
  while True:
      line = ser.readline().decode('utf-8').strip()
      if line.startswith('DATA,'):
          parts = line.split(',')
          print(f"ADC: {parts[2]}, LED: {parts[3]}")
      elif line:
          print(f"ESP32: {line}")
  # Wysyłanie komendy:
  ser.write(b'LED ON\r\n')
```

---

## 11. Podsumowanie i dalsze kroki

### 11.1 Kluczowe wnioski

```
✅ UART w ESP32 — to co zapamiętać:

1. KONTROLERY: 3 niezależne porty UART (UART0/1/2).
   UART0 = konsola USB. UART1 wymaga remapowania pinów!

2. KONFIGURACJA: uart_config_t + uart_param_config()
   Standardowo: 115200 8N1 (baud, 8 data, no parity, 1 stop)

3. DRIVER: uart_driver_install() — alokuje ring buffers + ISR
   RX buffer > 128B (FIFO size). TX buffer = 0 → blokujący

4. BUFORY: Sprzętowy FIFO (128B) + Ring Buffer (RAM, user-defined)
   ISR przenosi dane: FIFO ↔ Ring Buffer automatycznie

5. TX/RX: uart_write_bytes() / uart_read_bytes()
   uart_wait_tx_done() — czeka na zakończenie transmisji

6. EVENTY: uart_event_t + FreeRTOS Queue
   UART_DATA, UART_FIFO_OVF, UART_BUFFER_FULL, UART_PATTERN_DET

7. PATTERN: uart_enable_pattern_det_baud_intr() + uart_pattern_queue_reset()
   Sprzętowa detekcja powtarzającego się znaku
```

### 11.2 Najczęstsze błędy

| Błąd | Przyczyna | Rozwiązanie |
|------|-----------|-------------|
| Śmieci na konsoli | Niezgodny baud rate | Sprawdź baud rate po obu stronach |
| Crash po starcie UART1 | Piny GPIO6-11 = Flash SPI | Przemapuj UART1 na inne GPIO |
| `UART_FIFO_OVF` | Za wolne przetwarzanie | Zwiększ RX buffer, dodaj flow control |
| `UART_BUFFER_FULL` | Za mały ring buffer | Zwiększ `rx_buffer_size` w `uart_driver_install()` |
| Pattern nie działa | Brak `uart_pattern_queue_reset()` | Wywołaj po `uart_enable_pattern_det_baud_intr()` |
| Utrata danych | Brak obsługi zdarzeń | Użyj event queue zamiast polling |

### 11.3 Dalsze kroki

- **Moduł 3.2 — SPI:** Synchroniczna komunikacja szeregowa (full-duplex)
- **Moduł 3.3 — I²C:** Dwuprzewodowy interfejs do czujników
- **UART + DMA:** Zaawansowane przesyłanie danych bez obciążenia CPU
- **RS485:** Komunikacja half-duplex dla przemysłu (`UART_MODE_RS485_HALF_DUPLEX`)

---

## 12. Źródła i dokumentacja

| Źródło | Link |
|--------|------|
| **ESP-IDF UART API Reference** | [docs.espressif.com — UART](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/uart.html) |
| **ESP32 Technical Reference Manual** | [espressif.com — TRM (PDF)](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf#uart) — Rozdział: UART Controller |
| **Przykład: uart_echo** | [github.com — uart_echo](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/uart/uart_echo) |
| **Przykład: uart_events** | [github.com — uart_events](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/uart/uart_events) |
| **Przykład: uart_async_rxtxtasks** | [github.com — uart_async](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/uart/uart_async_rxtxtasks) |
| **Przykład: nmea0183_parser** | [github.com — nmea0183](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/uart/nmea0183_parser) |

---

> **📌 Moduł 3.1 zakończony.** Opanowałeś komunikację UART w ESP32: od konfiguracji parametrów, przez ring buffers i event queue, po detekcję wzorców i dwukierunkową komunikację z PC. W następnym module (3.2) poznasz synchroniczną komunikację SPI.
