# Moduł 3.2 — I2C (Inter-Integrated Circuit)

> **Poziom:** 🟡 Początkujący · **Czas:** Tydzień 10–12 (Faza 3)
> **Cel:** Opanowanie komunikacji I2C w ESP32 — konfiguracja magistrali master, dodawanie urządzeń (`i2c_master_bus_add_device()`), skanowanie magistrali, tryby prędkości Standard/Fast, oraz praktyczne ćwiczenia z wyświetlaczem OLED SSD1306, IMU MPU6050, czujnikiem ciśnienia BMP180, detektorem koloru TCS34725 i czujnikiem odległości VL53L0X.

---

## Spis treści

1. [Czym jest I2C?](#1-czym-jest-i2c)
2. [Architektura I2C w ESP32](#2-architektura-i2c-w-esp32)
3. [API I2C Master — ESP-IDF v5.x](#3-api-i2c-master--esp-idf-v5x)
4. [Skanowanie magistrali I2C](#4-skanowanie-magistrali-i2c)
5. [Prędkości I2C — Standard vs Fast](#5-prędkości-i2c--standard-vs-fast)
6. [Ćwiczenie 1: OLED SSD1306 0.96″ — wyświetlanie tekstu](#6-ćwiczenie-1-oled-ssd1306-096--wyświetlanie-tekstu)
7. [Ćwiczenie 2: IMU MPU6050 — akcelerometr + żyroskop](#7-ćwiczenie-2-imu-mpu6050--akcelerometr--żyroskop)
8. [Ćwiczenie 3: Czujnik ciśnienia BMP180](#8-ćwiczenie-3-czujnik-ciśnienia-bmp180)
9. [Ćwiczenie 4: Detektor koloru TCS34725](#9-ćwiczenie-4-detektor-koloru-tcs34725)
10. [Ćwiczenie 5: Czujnik odległości VL53L0X — Time-of-Flight](#10-ćwiczenie-5-czujnik-odległości-vl53l0x--time-of-flight)
11. [Podsumowanie i dalsze kroki](#11-podsumowanie-i-dalsze-kroki)
12. [Źródła i dokumentacja](#12-źródła-i-dokumentacja)

---

## 1. Czym jest I2C?

### 1.1 Zasada działania

**I2C (Inter-Integrated Circuit)**, znany też jako **I²C** lub **TWI (Two-Wire Interface)**, to synchroniczny, szeregowy protokół komunikacyjny umożliwiający podłączenie wielu urządzeń na wspólnej dwuprzewodowej magistrali. Został zaprojektowany przez firmę **Philips Semiconductors** (obecnie NXP) w 1982 roku.

```
Magistrala I2C — struktura fizyczna:

   Vcc (3.3V)
    │         │
   [Rp]      [Rp]       Rp = Rezystor pull-up (2.2 kΩ – 10 kΩ)
    │         │
    ├─────────┼────────────────────────────────────────── SDA (Serial Data)
    │         │         │            │            │
    │         ├─────────┼────────────┼────────────┼──── SCL (Serial Clock)
    │         │         │            │            │
  ┌─┴─────────┴─┐  ┌──┴────────┴──┐  ┌──────────┴──┐
  │   MASTER     │  │   SLAVE #1   │  │   SLAVE #2   │
  │   (ESP32)    │  │  (SSD1306)   │  │  (MPU6050)   │
  │  Generuje    │  │  Addr: 0x3C  │  │  Addr: 0x68  │
  │  SCL clock   │  │              │  │              │
  └──────────────┘  └──────────────┘  └──────────────┘

  • SDA — linia danych (bidirekcyjna, open-drain)
  • SCL — linia zegara (generowana przez master)
  • Oba sygnały: open-drain + zewnętrzne pull-upy do Vcc
  • Multi-slave: każde urządzenie ma unikalny adres (7-bit lub 10-bit)
```

### 1.2 Protokół komunikacji — ramka I2C

Komunikacja I2C składa się z następujących elementów:

```
Ramka zapisu I2C (Master → Slave):

  SDA: ─┐   ┌───────────────────┐ ┌─┐ ┌───────────────────┐ ┌─┐   ┌─
        │   │  Adres (7 bitów)  │ │A│ │  Dane (8 bitów)   │ │A│   │
        └───┘  A6 A5 A4 A3 A2 A1│ │C│ │  D7 D6 D5...D0   │ │C│   │
                                 │ │K│ │                   │ │K│   │
  SCL: ──┘└──┘└──┘└──┘└──┘└──┘└─┘ └─┘ └──┘└──┘└──┘└──┘└──┘ └─┘ └──
        ↑                       ↑   ↑                        ↑    ↑
      START            R/W bit(0=W) ACK                     ACK  STOP
                              (1=R)

  START:    SDA spada gdy SCL = HIGH
  STOP:     SDA rośnie gdy SCL = HIGH
  ACK:      Slave ciągnie SDA w dół (LOW) — potwierdzenie odbioru
  NACK:     SDA pozostaje HIGH — brak potwierdzenia (błąd/koniec)
  R/W bit:  0 = zapis (Master → Slave), 1 = odczyt (Slave → Master)
```

```
Ramka odczytu I2C (Master ← Slave):

  [START] [Adres+W] [ACK] [Reg.Addr] [ACK] [Sr] [Adres+R] [ACK] [Dane] [NACK] [STOP]
                                             ↑
                                        Repeated START
                                   (restart bez STOP — zmiana kierunku)

  Typowy odczyt rejestru:
  1. Master wysyła adres urządzenia + W (zapis)
  2. Master wysyła adres rejestru do odczytania
  3. Master wysyła Repeated START
  4. Master wysyła adres urządzenia + R (odczyt)
  5. Slave wysyła dane z rejestru
  6. Master wysyła NACK + STOP (koniec transferu)
```

### 1.3 Adresowanie urządzeń

Każde urządzenie I2C ma **unikalny adres** na magistrali:

| Format | Zakres | Uwagi |
|--------|--------|-------|
| **7-bitowy** | 0x00–0x7F (0–127) | Standard, najczęściej używany |
| **10-bitowy** | 0x000–0x3FF | Rzadko spotykany, więcej urządzeń |

> **⚠️ Uwaga:** Adresy 0x00–0x07 i 0x78–0x7F są zarezerwowane przez specyfikację I2C. Efektywny zakres adresów 7-bitowych: **0x08–0x77** (112 urządzeń).

| Urządzenie | Adres domyślny | Adres alternatywny |
|------------|----------------|-------------------|
| **SSD1306** (OLED) | **0x3C** | 0x3D (zmiana pinem SA0) |
| **MPU6050** (IMU) | **0x68** | 0x69 (AD0 = HIGH) |
| **BMP180** (ciśnienie) | **0x77** | — (stały) |
| **TCS34725** (kolor) | **0x29** | — (stały) |
| **VL53L0X** (ToF) | **0x29** | Programowalny |

> **💡 Konflikt adresów:** TCS34725 i VL53L0X mają domyślnie ten sam adres (0x29)! Aby użyć obu jednocześnie, należy zmienić adres VL53L0X programowo przez pin XSHUT lub użyć multipleksera I2C (np. TCA9548A).

---

## 2. Architektura I2C w ESP32

### 2.1 Kontrolery I2C

ESP32 posiada **2 niezależne kontrolery I2C** (I2C0, I2C1):

```
ESP32 — Kontrolery I2C
═══════════════════════════════════════════════════════════════

  ┌──────────────────────────────────────────────────────────┐
  │                    ESP32 I2C System                       │
  │                                                          │
  │  ┌──────────────────────┐  ┌──────────────────────┐     │
  │  │   I2C0 (Port 0)      │  │   I2C1 (Port 1)      │     │
  │  │                      │  │                      │     │
  │  │ SDA: dowolne GPIO    │  │ SDA: dowolne GPIO    │     │
  │  │ SCL: dowolne GPIO    │  │ SCL: dowolne GPIO    │     │
  │  │                      │  │                      │     │
  │  │ Master lub Slave     │  │ Master lub Slave     │     │
  │  │ (nie oba naraz!)     │  │ (nie oba naraz!)     │     │
  │  └──────────────────────┘  └──────────────────────┘     │
  │                                                          │
  │  Cechy wspólne:                                          │
  │  • Piny mapowalne przez GPIO Matrix (dowolne GPIO)       │
  │  • Standard mode: do 100 kHz                             │
  │  • Fast mode: do 400 kHz                                 │
  │  • Adresowanie 7-bit i 10-bit                            │
  │  • Filtr glitchy (glitch_ignore_cnt)                     │
  │  • Wewnętrzne pull-upy (słabe — zalecane zewnętrzne!)    │
  └──────────────────────────────────────────────────────────┘

  Zalecane GPIO dla I2C (NodeMCU-32):
    I2C0: SDA = GPIO21, SCL = GPIO22 (domyślny standard)
    I2C1: SDA = GPIO18, SCL = GPIO19 (lub inne wolne)
```

### 2.2 Model Bus-Device w ESP-IDF v5.x

ESP-IDF v5.x wprowadza nowy model **Bus-Device** dla I2C master:

```
Model Bus-Device — architektura:

  ┌────────────────────────────────────────────────┐
  │              i2c_new_master_bus()                │
  │     ┌───────────────────────────────────┐       │
  │     │    I2C Master Bus (bus_handle)     │       │
  │     │    Port: I2C_PORT_NUM_0           │       │
  │     │    SDA: GPIO21, SCL: GPIO22       │       │
  │     │    Glitch filter: 7               │       │
  │     └───────┬──────────┬────────────────┘       │
  │             │          │                         │
  │    ┌────────┴──┐ ┌────┴───────┐ ┌──────────┐   │
  │    │ Device #1 │ │ Device #2  │ │ Device #3│   │
  │    │ SSD1306   │ │ MPU6050    │ │ BMP180   │   │
  │    │ 0x3C      │ │ 0x68       │ │ 0x77     │   │
  │    │ 400 kHz   │ │ 400 kHz    │ │ 100 kHz  │   │
  │    │ dev_handle│ │ dev_handle │ │ dev_handle│  │
  │    └───────────┘ └────────────┘ └──────────┘   │
  │    i2c_master_bus_add_device() — dla każdego    │
  └────────────────────────────────────────────────┘

  Każde urządzenie ma własny dev_handle z indywidualną
  prędkością SCL i adresem. Magistrala jest współdzielona.
```

---

## 3. API I2C Master — ESP-IDF v5.x

### 3.1 Nagłówek i zależności

```c
#include "driver/i2c_master.h"    // Nowe API I2C master (ESP-IDF v5.x)
```

W `CMakeLists.txt` komponent `driver` jest dodawany automatycznie. Dodaj `esp_driver_i2c` jeśli wymagany:

```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2c)
```

### 3.2 Krok 1: Inicjalizacja magistrali — i2c_new_master_bus()

```c
i2c_master_bus_config_t bus_config = {
    .i2c_port   = I2C_PORT_NUM_0,          // Port I2C (0 lub 1)
    .sda_io_num = GPIO_NUM_21,             // Pin SDA
    .scl_io_num = GPIO_NUM_22,             // Pin SCL
    .clk_source = I2C_CLK_SRC_DEFAULT,     // Źródło zegara (APB 80 MHz)
    .glitch_ignore_cnt = 7,                // Filtr glitchy (typowo 7)
    .flags.enable_internal_pullup = true,   // Wewnętrzne pull-upy (słabe!)
};

i2c_master_bus_handle_t bus_handle;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
```

**Pola `i2c_master_bus_config_t`:**

| Pole | Typ | Opis |
|------|-----|------|
| `i2c_port` | `i2c_port_num_t` | `I2C_PORT_NUM_0` lub `I2C_PORT_NUM_1` |
| `sda_io_num` | `gpio_num_t` | Numer GPIO dla SDA |
| `scl_io_num` | `gpio_num_t` | Numer GPIO dla SCL |
| `clk_source` | `i2c_clock_source_t` | `I2C_CLK_SRC_DEFAULT` (APB 80 MHz) |
| `glitch_ignore_cnt` | `uint8_t` | Filtr glitchy — impulsy krótsze niż ta wartość są ignorowane |
| `intr_priority` | `int` | Priorytet przerwania (0 = auto, 1–3) |
| `trans_queue_depth` | `size_t` | Głębokość kolejki transakcji (async) |
| `enable_internal_pullup` | `bool` | Wewnętrzne pull-upy (~45 kΩ — za słabe na Fast mode!) |

> **⚠️ KRYTYCZNE:** Wewnętrzne pull-upy ESP32 mają rezystancję **~45 kΩ** — są **zbyt słabe** dla niezawodnej komunikacji, szczególnie przy 400 kHz. **Zawsze** używaj zewnętrznych rezystorów pull-up **2.2 kΩ – 4.7 kΩ** na liniach SDA i SCL.

### 3.3 Krok 2: Dodawanie urządzenia — i2c_master_bus_add_device()

```c
i2c_device_config_t dev_config = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,  // Adres 7-bitowy
    .device_address  = 0x3C,                 // Adres urządzenia (np. SSD1306)
    .scl_speed_hz    = 400000,               // Prędkość SCL: 400 kHz (Fast mode)
};

i2c_master_dev_handle_t dev_handle;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
```

**Pola `i2c_device_config_t`:**

| Pole | Typ | Opis |
|------|-----|------|
| `dev_addr_length` | `i2c_addr_bit_len_t` | `I2C_ADDR_BIT_LEN_7` lub `I2C_ADDR_BIT_LEN_10` |
| `device_address` | `uint16_t` | Adres slave (7-bit: 0x08–0x77) |
| `scl_speed_hz` | `uint32_t` | Prędkość SCL w Hz (100000 lub 400000) |
| `scl_wait_us` | `uint32_t` | Timeout oczekiwania na SCL (opcjonalne) |

### 3.4 Krok 3: Transmisja danych

**Zapis (Master → Slave):**

```c
uint8_t write_buf[] = {0x00, 0xAE};  // Np. komenda wyłączenia SSD1306
ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), -1));
// -1 = domyślny timeout (ustawiany przez driver)
```

**Odczyt (Slave → Master):**

```c
uint8_t read_buf[6];
ESP_ERROR_CHECK(i2c_master_receive(dev_handle, read_buf, sizeof(read_buf), -1));
```

**Zapis + Odczyt (typowy odczyt rejestru):**

```c
uint8_t reg_addr = 0x75;  // Rejestr WHO_AM_I w MPU6050
uint8_t data;
ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &reg_addr, 1, &data, 1, -1));
// Wysyła reg_addr, potem Repeated START, potem odbiera 1 bajt
ESP_LOGI(TAG, "WHO_AM_I = 0x%02X", data);  // Oczekiwane: 0x68
```

### 3.5 Krok 4: Usuwanie zasobów

```c
// Usunięcie urządzenia z magistrali
ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));

// Usunięcie magistrali (po usunięciu wszystkich urządzeń)
ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
```

### 3.6 Cykl życia I2C Master

```
Cykl życia I2C Master:

  1. i2c_new_master_bus()           ← Inicjalizacja magistrali
  2. i2c_master_bus_add_device()    ← Dodanie urządzenia (wielokrotnie)
  ─── Magistrala aktywna ───
  3. i2c_master_transmit()          ← Zapis danych
  4. i2c_master_receive()           ← Odczyt danych
  5. i2c_master_transmit_receive()  ← Zapis + odczyt (rejestr)
  6. i2c_master_probe()             ← Sprawdzenie obecności urządzenia
  ─── Zakończenie ───
  7. i2c_master_bus_rm_device()     ← Usunięcie urządzenia
  8. i2c_del_master_bus()           ← Zwolnienie magistrali
```

### 3.7 Pomocnicze funkcje I2C

Przydatna funkcja pomocnicza do zapisu/odczytu rejestrów:

```c
/**
 * Zapis jednego bajtu do rejestru urządzenia I2C
 */
static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, -1);
}

/**
 * Odczyt N bajtów z rejestru urządzenia I2C
 */
static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev,
                               uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

/**
 * Odczyt jednego bajtu z rejestru
 */
static esp_err_t i2c_read_reg_byte(i2c_master_dev_handle_t dev,
                                    uint8_t reg, uint8_t *val)
{
    return i2c_read_reg(dev, reg, val, 1);
}
```

---

## 4. Skanowanie magistrali I2C

### 4.1 Zasada działania

Skanowanie polega na wysłaniu adresu do każdego możliwego urządzenia (0x01–0x7F) i sprawdzeniu, czy otrzymamy **ACK** (urządzenie obecne) czy **NACK** (brak urządzenia). ESP-IDF oferuje funkcję `i2c_master_probe()` do tego celu.

### 4.2 Kompletny skaner I2C

```c
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "I2C_SCAN";

#define I2C_SDA_PIN     GPIO_NUM_21
#define I2C_SCL_PIN     GPIO_NUM_22

void app_main(void)
{
    // ═══ Inicjalizacja magistrali I2C ═══
    i2c_master_bus_config_t bus_config = {
        .i2c_port   = I2C_PORT_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // ═══ Skanowanie adresów 0x01 – 0x7F ═══
    ESP_LOGI(TAG, "Skanowanie magistrali I2C...");
    ESP_LOGI(TAG, "     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

    int devices_found = 0;

    for (uint8_t row = 0; row < 8; row++) {
        printf("0x%02X:", row * 16);
        for (uint8_t col = 0; col < 16; col++) {
            uint8_t addr = (row << 4) | col;

            if (addr < 0x08 || addr > 0x77) {
                // Adresy zarezerwowane
                printf("   ");
                continue;
            }

            esp_err_t ret = i2c_master_probe(bus_handle, addr, 50);
            if (ret == ESP_OK) {
                printf(" %02X", addr);
                devices_found++;
            } else {
                printf(" --");
            }
        }
        printf("\n");
    }

    ESP_LOGI(TAG, "Znaleziono %d urządzeń na magistrali I2C.", devices_found);

    // ═══ Czyszczenie ═══
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
}
```

**Przykładowy wynik skanowania:**

```
I2C_SCAN: Skanowanie magistrali I2C...
I2C_SCAN:      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
0x00:                         -- -- -- -- -- -- -- -- -- -- -- --
0x10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
0x20: -- -- -- -- -- -- -- -- -- 29 -- -- -- -- -- --
0x30: -- -- -- -- -- -- -- -- -- -- -- -- 3C -- -- --
0x40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
0x50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
0x60: -- -- -- -- -- -- -- -- 68 -- -- -- -- -- -- --
0x70: -- -- -- -- -- -- -- 77 -- -- -- -- -- -- -- --
I2C_SCAN: Znaleziono 4 urządzeń na magistrali I2C.
          0x29 = TCS34725, 0x3C = SSD1306, 0x68 = MPU6050, 0x77 = BMP180
```

---

## 5. Prędkości I2C — Standard vs Fast

### 5.1 Tryby prędkości

| Tryb | Prędkość SCL | Pull-up | Zastosowanie |
|------|-------------|---------|-------------|
| **Standard Mode (Sm)** | **100 kHz** | 4.7 kΩ – 10 kΩ | Czujniki, EEPROM, powolne urządzenia |
| **Fast Mode (Fm)** | **400 kHz** | 2.2 kΩ – 4.7 kΩ | Wyświetlacze, IMU, szybki transfer |

> **⚠️ ESP32 nie obsługuje** Fast Mode Plus (1 MHz) ani High-Speed Mode (3.4 MHz). Maksymalna częstotliwość SCL to **400 kHz**.

### 5.2 Dobór rezystorów pull-up

```
Dobór rezystorów pull-up:

  Vcc = 3.3V
   │
  [Rp]  ← Rezystor pull-up
   │
   ├──── SDA / SCL
   │
  [Cb]  ← Pojemność magistrali (zależy od długości kabli i liczby urządzeń)
   │
  GND

  Zasada: Im wyższa częstotliwość → mniejszy rezystor (więcej prądu)
           Im dłuższa magistrala → mniejszy rezystor

  Zalecenia:
  ┌──────────────┬────────────────────┬───────────────────┐
  │ Prędkość     │ Rp zalecane        │ Max poj. magistr. │
  ├──────────────┼────────────────────┼───────────────────┤
  │ 100 kHz (Sm) │ 4.7 kΩ – 10 kΩ    │ 400 pF            │
  │ 400 kHz (Fm) │ 2.2 kΩ – 4.7 kΩ   │ 400 pF            │
  └──────────────┴────────────────────┴───────────────────┘

  ⚠️ Większość modułów breakout (np. GY-521, GY-68) mają
     wbudowane pull-upy 10 kΩ. Dla 400 kHz warto dolutować
     zewnętrzne 2.2 kΩ lub wymienić na odpowiednie.
```

### 5.3 Konfiguracja prędkości per-device

W nowym API ESP-IDF v5.x prędkość ustawia się **per urządzenie**, nie per magistralę:

```c
// SSD1306 — szybki transfer (ramka ekranu)
i2c_device_config_t oled_cfg = {
    .device_address = 0x3C,
    .scl_speed_hz   = 400000,   // 400 kHz — Fast mode
};

// BMP180 — wolniejszy czujnik
i2c_device_config_t bmp_cfg = {
    .device_address = 0x77,
    .scl_speed_hz   = 100000,   // 100 kHz — Standard mode
};

// Oba urządzenia na tej samej magistrali, różne prędkości!
i2c_master_dev_handle_t oled_handle, bmp_handle;
i2c_master_bus_add_device(bus_handle, &oled_cfg, &oled_handle);
i2c_master_bus_add_device(bus_handle, &bmp_cfg, &bmp_handle);
```

---

## 6. Ćwiczenie 1: OLED SSD1306 0.96″ — wyświetlanie tekstu

### 6.1 Opis czujnika

**SSD1306** to kontroler wyświetlacza OLED o rozdzielczości **128×64 pikseli** lub 128×32. Komunikacja przez I2C (adres domyślny: **0x3C**). Wyświetlacz jest monochromatyczny (biały/niebieski na czarnym tle).

| Parametr | Wartość |
|----------|---------|
| **Adres I2C** | 0x3C (SA0=LOW) lub 0x3D (SA0=HIGH) |
| **Rozdzielczość** | 128 × 64 pikseli |
| **Napięcie** | 3.3V lub 5V (moduł z regulatorem) |
| **Prędkość I2C** | Do 400 kHz (Fast mode) |
| **GDDRAM** | 128×64 bit = 1024 bajtów (bufor ramki) |
| **Datasheet** | Solomon Systech SSD1306 Rev 1.1 |

### 6.2 Architektura pamięci SSD1306

```
Pamięć GDDRAM — organizacja w stronach (pages):

  128 kolumn (0–127)
  ┌──────────────────────────────────────────┐
  │ Page 0 (Y = 0–7)    │ 128 bajtów         │  Bit 0 = góra
  │ Page 1 (Y = 8–15)   │ 128 bajtów         │  Bit 7 = dół
  │ Page 2 (Y = 16–23)  │ 128 bajtów         │  (każda strona)
  │ Page 3 (Y = 24–31)  │ 128 bajtów         │
  │ Page 4 (Y = 32–39)  │ 128 bajtów         │
  │ Page 5 (Y = 40–47)  │ 128 bajtów         │
  │ Page 6 (Y = 48–55)  │ 128 bajtów         │
  │ Page 7 (Y = 56–63)  │ 128 bajtów         │
  └──────────────────────────────────────────┘
  Razem: 8 stron × 128 kolumn = 1024 bajtów

  Każdy bajt = 8 pikseli pionowo (bit0=góra, bit7=dół)
  Współrzędna piksela (x, y):
    page   = y / 8
    bit    = y % 8
    kolumna = x
    buffer[page * 128 + x] |= (1 << bit)   // zapal piksel
```

### 6.3 Protokół komunikacji SSD1306

SSD1306 rozróżnia dwa typy danych na podstawie **bajtu kontrolnego** (Co/D/C#):

```
Bajt kontrolny I2C dla SSD1306:

  [7] [6] [5:0]
   Co  D/C# 000000

  Co = 0:  następne bajty to dane/komendy (ciągły strumień)
  Co = 1:  po tym bajcie następuje kolejny bajt kontrolny

  D/C# = 0:  następny bajt to KOMENDA (command)
  D/C# = 1:  następny bajt to DANE (data — piksele do GDDRAM)

  Typowe wartości:
    0x00 = komenda (Co=0, D/C#=0)     → po nim stream komend
    0x40 = dane   (Co=0, D/C#=1)      → po nim stream danych pikseli
    0x80 = pojedyncza komenda (Co=1, D/C#=0)
```

### 6.4 Schemat podłączenia

```
ESP32 NodeMCU-32 — OLED SSD1306 I2C
┌──────────────────┐       ┌─────────────────┐
│                  │       │   SSD1306 OLED   │
│  GPIO21 (SDA) ───┼───────┤ SDA              │
│  GPIO22 (SCL) ───┼───────┤ SCL              │
│  3.3V ───────────┼───────┤ VCC              │
│  GND ────────────┼───────┤ GND              │
│                  │       └─────────────────┘
└──────────────────┘

   Pull-up: 4.7 kΩ na SDA i SCL do 3.3V
   (wiele modułów ma wbudowane pull-upy)
```

### 6.5 Kompletny kod — sterownik SSD1306

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "SSD1306";

// ═══════════════════════════════════════════════════════════
// Konfiguracja
// ═══════════════════════════════════════════════════════════
#define I2C_SDA_PIN         GPIO_NUM_21
#define I2C_SCL_PIN         GPIO_NUM_22
#define SSD1306_ADDR        0x3C
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64
#define SSD1306_PAGES       (SSD1306_HEIGHT / 8)  // 8 stron
#define SSD1306_BUF_SIZE    (SSD1306_WIDTH * SSD1306_PAGES)  // 1024 bajtów

// ═══════════════════════════════════════════════════════════
// Komendy SSD1306 (z datasheetu)
// ═══════════════════════════════════════════════════════════
#define SSD1306_CMD_DISPLAY_OFF          0xAE
#define SSD1306_CMD_DISPLAY_ON           0xAF
#define SSD1306_CMD_SET_CONTRAST         0x81
#define SSD1306_CMD_NORMAL_DISPLAY       0xA6
#define SSD1306_CMD_INVERT_DISPLAY       0xA7
#define SSD1306_CMD_SET_MUX_RATIO        0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET   0xD3
#define SSD1306_CMD_SET_START_LINE       0x40
#define SSD1306_CMD_SET_SEG_REMAP        0xA1  // kolumna 127 = SEG0
#define SSD1306_CMD_SET_COM_SCAN_DEC     0xC8  // skan COM od dołu
#define SSD1306_CMD_SET_COM_PINS         0xDA
#define SSD1306_CMD_SET_CLK_DIV          0xD5
#define SSD1306_CMD_SET_PRECHARGE        0xD9
#define SSD1306_CMD_SET_VCOMH            0xDB
#define SSD1306_CMD_CHARGE_PUMP          0x8D
#define SSD1306_CMD_MEMORY_MODE          0x20
#define SSD1306_CMD_SET_COL_ADDR         0x21
#define SSD1306_CMD_SET_PAGE_ADDR        0x22
#define SSD1306_CMD_ENTIRE_DISPLAY_RAM   0xA4

// ═══════════════════════════════════════════════════════════
// Czcionka 5x7 (ASCII 32–126)
// ═══════════════════════════════════════════════════════════
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' ' (32)
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x3E,0x41,0x5D,0x55,0x1E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
};

// ═══════════════════════════════════════════════════════════
// Zmienne globalne
// ═══════════════════════════════════════════════════════════
static i2c_master_dev_handle_t oled_dev;
static uint8_t oled_buffer[SSD1306_BUF_SIZE];  // Bufor ramki (1024B)

// ═══════════════════════════════════════════════════════════
// Funkcje sterownika SSD1306
// ═══════════════════════════════════════════════════════════

/** Wysłanie komendy do SSD1306 */
static esp_err_t ssd1306_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};  // 0x00 = bajt kontrolny (komenda)
    return i2c_master_transmit(oled_dev, buf, 2, -1);
}

/** Wysłanie bufora danych (pikseli) do GDDRAM */
static esp_err_t ssd1306_send_buffer(void)
{
    // Ustaw zakres kolumn i stron na cały ekran
    ssd1306_cmd(SSD1306_CMD_SET_COL_ADDR);
    ssd1306_cmd(0);    // kolumna start
    ssd1306_cmd(127);  // kolumna end
    ssd1306_cmd(SSD1306_CMD_SET_PAGE_ADDR);
    ssd1306_cmd(0);    // strona start
    ssd1306_cmd(7);    // strona end

    // Wyślij dane z bajtem kontrolnym 0x40 (dane)
    uint8_t send_buf[SSD1306_BUF_SIZE + 1];
    send_buf[0] = 0x40;  // bajt kontrolny: dane
    memcpy(&send_buf[1], oled_buffer, SSD1306_BUF_SIZE);
    return i2c_master_transmit(oled_dev, send_buf, SSD1306_BUF_SIZE + 1, -1);
}

/** Czyszczenie bufora (czarny ekran) */
static void ssd1306_clear(void)
{
    memset(oled_buffer, 0x00, SSD1306_BUF_SIZE);
}

/** Ustawienie piksela w buforze */
static void ssd1306_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    if (on)
        oled_buffer[(y / 8) * SSD1306_WIDTH + x] |= (1 << (y % 8));
    else
        oled_buffer[(y / 8) * SSD1306_WIDTH + x] &= ~(1 << (y % 8));
}

/** Wyświetlenie znaku na pozycji (x, y) — czcionka 5x7 */
static void ssd1306_draw_char(int x, int y, char c)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            ssd1306_set_pixel(x + col, y + row, (line >> row) & 1);
        }
    }
}

/** Wyświetlenie tekstu na pozycji (x, y) */
static void ssd1306_draw_string(int x, int y, const char *str)
{
    while (*str) {
        ssd1306_draw_char(x, y, *str);
        x += 6;  // 5 pikseli znaku + 1 piksel odstępu
        str++;
    }
}

/** Inicjalizacja SSD1306 (sekwencja z datasheetu) */
static esp_err_t ssd1306_init(i2c_master_bus_handle_t bus)
{
    // Dodanie SSD1306 do magistrali I2C
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = SSD1306_ADDR,
        .scl_speed_hz    = 400000,  // Fast mode
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &oled_dev));

    // Sekwencja inicjalizacji (z datasheetu SSD1306, sekcja 15.2)
    ssd1306_cmd(SSD1306_CMD_DISPLAY_OFF);          // Wyłącz ekran
    ssd1306_cmd(SSD1306_CMD_SET_CLK_DIV);
    ssd1306_cmd(0x80);                              // Domyślna częstotliwość
    ssd1306_cmd(SSD1306_CMD_SET_MUX_RATIO);
    ssd1306_cmd(0x3F);                              // MUX = 64 (64 linie)
    ssd1306_cmd(SSD1306_CMD_SET_DISPLAY_OFFSET);
    ssd1306_cmd(0x00);                              // Offset = 0
    ssd1306_cmd(SSD1306_CMD_SET_START_LINE);        // Start line = 0
    ssd1306_cmd(SSD1306_CMD_CHARGE_PUMP);
    ssd1306_cmd(0x14);                              // Charge pump ON (wewnętrzny)
    ssd1306_cmd(SSD1306_CMD_MEMORY_MODE);
    ssd1306_cmd(0x00);                              // Horizontal addressing mode
    ssd1306_cmd(SSD1306_CMD_SET_SEG_REMAP);         // Segment remap
    ssd1306_cmd(SSD1306_CMD_SET_COM_SCAN_DEC);      // COM scan direction
    ssd1306_cmd(SSD1306_CMD_SET_COM_PINS);
    ssd1306_cmd(0x12);                              // COM pins: alternative
    ssd1306_cmd(SSD1306_CMD_SET_CONTRAST);
    ssd1306_cmd(0xCF);                              // Kontrast
    ssd1306_cmd(SSD1306_CMD_SET_PRECHARGE);
    ssd1306_cmd(0xF1);                              // Pre-charge period
    ssd1306_cmd(SSD1306_CMD_SET_VCOMH);
    ssd1306_cmd(0x40);                              // VCOMH deselect level
    ssd1306_cmd(SSD1306_CMD_ENTIRE_DISPLAY_RAM);    // Wyświetl z RAM
    ssd1306_cmd(SSD1306_CMD_NORMAL_DISPLAY);        // Normalny (nie inwersja)

    // Wyczyść ekran i włącz
    ssd1306_clear();
    ssd1306_send_buffer();
    ssd1306_cmd(SSD1306_CMD_DISPLAY_ON);            // Włącz ekran

    ESP_LOGI(TAG, "SSD1306 zainicjalizowany (128x64, addr=0x%02X)", SSD1306_ADDR);
    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
// Aplikacja główna
// ═══════════════════════════════════════════════════════════
void app_main(void)
{
    // Inicjalizacja magistrali I2C
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = I2C_PORT_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    // Inicjalizacja SSD1306
    ssd1306_init(bus);

    // Wyświetlanie tekstu
    ssd1306_clear();
    ssd1306_draw_string(0,  0,  "=== ESP32 I2C ===");
    ssd1306_draw_string(0,  10, "SSD1306 OLED");
    ssd1306_draw_string(0,  20, "128x64 pikseli");
    ssd1306_draw_string(0,  30, "Czcionka 5x7");
    ssd1306_draw_string(0,  50, "Modul 3.2 I2C");
    ssd1306_send_buffer();

    // Animacja — licznik
    char buf[32];
    int counter = 0;
    while (1) {
        snprintf(buf, sizeof(buf), "Cnt: %d", counter++);
        // Wyczyść tylko obszar licznika (linia y=40)
        for (int x = 0; x < SSD1306_WIDTH; x++)
            for (int y = 40; y < 48; y++)
                ssd1306_set_pixel(x, y, false);
        ssd1306_draw_string(0, 40, buf);
        ssd1306_send_buffer();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

> **💡 Optymalizacja:** W produkcji zamiast wysyłać cały bufor 1024B przy każdej zmianie, warto śledzić „dirty pages" i wysyłać tylko zmienione strony. Zmniejsza to czas transferu I2C ~8×.

---

## 7. Ćwiczenie 2: IMU MPU6050 — akcelerometr + żyroskop

### 7.1 Opis czujnika

**MPU6050** (InvenSense/TDK) to 6-osiowy IMU (Inertial Measurement Unit) zawierający 3-osiowy akcelerometr i 3-osiowy żyroskop. Popularny moduł breakout: **GY-521**.

| Parametr | Wartość |
|----------|---------|
| **Adres I2C** | 0x68 (AD0=LOW) lub 0x69 (AD0=HIGH) |
| **Napięcie** | 3.3V (moduł GY-521: 3.3V–5V) |
| **Akcelerometr** | ±2g, ±4g, ±8g, ±16g (konfigurowalny) |
| **Żyroskop** | ±250, ±500, ±1000, ±2000 °/s |
| **ADC** | 16-bit na każdą oś |
| **Prędkość I2C** | Do 400 kHz |
| **WHO_AM_I** | Rejestr 0x75 → wartość 0x68 |
| **Datasheet** | InvenSense MPU-6000/MPU-6050 Register Map RM-MPU-6000A |

### 7.2 Mapa rejestrów (najważniejsze)

| Rejestr | Adres | Opis |
|---------|-------|------|
| `PWR_MGMT_1` | 0x6B | Zarządzanie zasilaniem (bit 6 = SLEEP) |
| `SMPLRT_DIV` | 0x19 | Dzielnik próbkowania |
| `CONFIG` | 0x1A | DLPF (Digital Low Pass Filter) |
| `GYRO_CONFIG` | 0x1B | Zakres żyroskopu (FS_SEL) |
| `ACCEL_CONFIG` | 0x1C | Zakres akcelerometru (AFS_SEL) |
| `ACCEL_XOUT_H` | 0x3B | Dane akcelerometru X (high byte) — 6 bajtów |
| `TEMP_OUT_H` | 0x41 | Temperatura (high byte) — 2 bajty |
| `GYRO_XOUT_H` | 0x43 | Dane żyroskopu X (high byte) — 6 bajtów |
| `WHO_AM_I` | 0x75 | Identyfikator urządzenia (= 0x68) |

### 7.3 Przeliczanie surowych wartości

```
Akcelerometr (surowe → g):
  AFS_SEL = 0: ±2g   → czułość = 16384 LSB/g
  AFS_SEL = 1: ±4g   → czułość = 8192  LSB/g
  AFS_SEL = 2: ±8g   → czułość = 4096  LSB/g
  AFS_SEL = 3: ±16g  → czułość = 2048  LSB/g

  accel_g = raw_value / czułość

Żyroskop (surowe → °/s):
  FS_SEL = 0: ±250°/s  → czułość = 131.0 LSB/(°/s)
  FS_SEL = 1: ±500°/s  → czułość = 65.5  LSB/(°/s)
  FS_SEL = 2: ±1000°/s → czułość = 32.8  LSB/(°/s)
  FS_SEL = 3: ±2000°/s → czułość = 16.4  LSB/(°/s)

  gyro_dps = raw_value / czułość

Temperatura:
  temp_C = (raw_value / 340.0) + 36.53
```

### 7.4 Schemat podłączenia

```
ESP32 NodeMCU-32 — MPU6050 + SSD1306
┌──────────────────┐
│                  │       ┌─────────────────┐
│  GPIO21 (SDA) ───┼──┬───┤ SDA   MPU6050   │
│  GPIO22 (SCL) ───┼──┼─┬─┤ SCL   (GY-521)  │
│  3.3V ───────────┼──┼─┼─┤ VCC             │
│  GND ────────────┼──┼─┼─┤ GND   AD0=GND   │
│                  │  │ │ └─────────────────┘
│                  │  │ │
│                  │  │ │ ┌─────────────────┐
│                  │  └─┼─┤ SDA   SSD1306   │
│                  │    └─┤ SCL   OLED      │
│  3.3V ───────────┼──────┤ VCC             │
│  GND ────────────┼──────┤ GND             │
│                  │      └─────────────────┘
└──────────────────┘

  Oba urządzenia na tej samej magistrali I2C!
  MPU6050: 0x68, SSD1306: 0x3C
  Pull-up: 4.7 kΩ na SDA i SCL (współdzielone)
```

### 7.5 Kompletny kod — MPU6050 + wyświetlanie na OLED

```c
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "MPU6050";

#define I2C_SDA_PIN     GPIO_NUM_21
#define I2C_SCL_PIN     GPIO_NUM_22
#define MPU6050_ADDR    0x68

// Rejestry MPU6050
#define MPU6050_WHO_AM_I     0x75
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_SMPLRT_DIV   0x19
#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_TEMP_OUT_H   0x41
#define MPU6050_GYRO_XOUT_H  0x43

static i2c_master_dev_handle_t mpu_dev;

// Funkcje pomocnicze I2C (zdefiniowane w sekcji 3.7)
static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, -1);
}
static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev,
                               uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

// ═══ Struktury danych MPU6050 ═══
typedef struct {
    float ax, ay, az;   // Akcelerometr [g]
    float gx, gy, gz;   // Żyroskop [°/s]
    float temp;          // Temperatura [°C]
} mpu6050_data_t;

// ═══ Inicjalizacja MPU6050 ═══
static esp_err_t mpu6050_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MPU6050_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &mpu_dev));

    // Weryfikacja WHO_AM_I
    uint8_t who;
    i2c_read_reg(mpu_dev, MPU6050_WHO_AM_I, &who, 1);
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X (oczekiwane: 0x68)", who);
    if (who != 0x68) {
        ESP_LOGE(TAG, "MPU6050 nie wykryty!");
        return ESP_FAIL;
    }

    // Wybudzenie (wyłącz tryb SLEEP)
    i2c_write_reg(mpu_dev, MPU6050_PWR_MGMT_1, 0x00);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Konfiguracja
    i2c_write_reg(mpu_dev, MPU6050_SMPLRT_DIV, 0x07);    // Sample rate = 1kHz / (1+7) = 125 Hz
    i2c_write_reg(mpu_dev, MPU6050_CONFIG, 0x06);         // DLPF = 5 Hz
    i2c_write_reg(mpu_dev, MPU6050_GYRO_CONFIG, 0x00);    // FS_SEL = 0: ±250°/s
    i2c_write_reg(mpu_dev, MPU6050_ACCEL_CONFIG, 0x00);   // AFS_SEL = 0: ±2g

    ESP_LOGI(TAG, "MPU6050 zainicjalizowany (±2g, ±250°/s)");
    return ESP_OK;
}

// ═══ Odczyt danych ═══
static esp_err_t mpu6050_read(mpu6050_data_t *out)
{
    uint8_t buf[14];
    // Odczyt 14 bajtów od ACCEL_XOUT_H (accel 6B + temp 2B + gyro 6B)
    ESP_ERROR_CHECK(i2c_read_reg(mpu_dev, MPU6050_ACCEL_XOUT_H, buf, 14));

    // Konwersja big-endian → int16_t
    int16_t raw_ax = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t raw_ay = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t raw_az = (int16_t)((buf[4]  << 8) | buf[5]);
    int16_t raw_t  = (int16_t)((buf[6]  << 8) | buf[7]);
    int16_t raw_gx = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t raw_gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t raw_gz = (int16_t)((buf[12] << 8) | buf[13]);

    // Przeliczenie na jednostki fizyczne (AFS_SEL=0, FS_SEL=0)
    out->ax = raw_ax / 16384.0f;
    out->ay = raw_ay / 16384.0f;
    out->az = raw_az / 16384.0f;
    out->gx = raw_gx / 131.0f;
    out->gy = raw_gy / 131.0f;
    out->gz = raw_gz / 131.0f;
    out->temp = (raw_t / 340.0f) + 36.53f;

    return ESP_OK;
}

// ═══ app_main ═══
void app_main(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = I2C_PORT_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    mpu6050_init(bus);
    // ssd1306_init(bus);  // ← Dodaj inicjalizację OLED z Ćwiczenia 1

    mpu6050_data_t data;
    char line[32];

    while (1) {
        mpu6050_read(&data);

        // Wyświetlanie w konsoli
        ESP_LOGI(TAG, "Accel: X=%.2fg Y=%.2fg Z=%.2fg", data.ax, data.ay, data.az);
        ESP_LOGI(TAG, "Gyro:  X=%.1f Y=%.1f Z=%.1f °/s", data.gx, data.gy, data.gz);
        ESP_LOGI(TAG, "Temp:  %.1f °C", data.temp);

        // Wyświetlanie na OLED (odkomentuj po dodaniu SSD1306):
        // ssd1306_clear();
        // ssd1306_draw_string(0, 0, "=== MPU6050 ===");
        // snprintf(line, sizeof(line), "AX: %+.2fg", data.ax);
        // ssd1306_draw_string(0, 10, line);
        // snprintf(line, sizeof(line), "AY: %+.2fg", data.ay);
        // ssd1306_draw_string(0, 20, line);
        // snprintf(line, sizeof(line), "AZ: %+.2fg", data.az);
        // ssd1306_draw_string(0, 30, line);
        // snprintf(line, sizeof(line), "T: %.1f C", data.temp);
        // ssd1306_draw_string(0, 50, line);
        // ssd1306_send_buffer();

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

---

## 8. Ćwiczenie 3: Czujnik ciśnienia BMP180

### 8.1 Opis czujnika

**BMP180** (Bosch Sensortec) to barometryczny czujnik ciśnienia i temperatury. Popularny moduł breakout: **GY-68**. Następca BMP085, kompatybilny programowo.

| Parametr | Wartość |
|----------|---------|
| **Adres I2C** | 0x77 (stały, niekonfigurowalny) |
| **Napięcie** | 1.8V – 3.6V (moduł: 3.3V–5V) |
| **Zakres ciśnienia** | 300 – 1100 hPa |
| **Dokładność ciśnienia** | ±1.0 hPa (tryb standard) |
| **Zakres temperatury** | −40°C – +85°C |
| **Dokładność temperatury** | ±1.0°C |
| **Rozdzielczość** | 0.01 hPa (ultra-high res.) |
| **Chip ID** | Rejestr 0xD0 → wartość 0x55 |
| **Datasheet** | Bosch BMP180 Digital Pressure Sensor BST-BMP180-DS000 |

### 8.2 Zasada pomiaru BMP180

BMP180 wymaga **kalibracji** — każdy egzemplarz ma unikalne współczynniki kalibracyjne przechowywane w EEPROM (rejestry 0xAA–0xBF). Proces pomiaru:

```
Algorytm pomiaru BMP180:

  1. Odczytaj 11 współczynników kalibracyjnych z EEPROM (jednorazowo)
     AC1, AC2, AC3, AC4, AC5, AC6, B1, B2, MB, MC, MD

  2. Pomiar temperatury:
     a) Wyślij komendę 0x2E do rejestru 0xF4 (start pomiaru temp.)
     b) Czekaj 4.5 ms
     c) Odczytaj surową temperaturę (UT) z rejestrów 0xF6–0xF7

  3. Pomiar ciśnienia:
     a) Wyślij komendę (0x34 + (oss<<6)) do rejestru 0xF4
        oss = oversampling: 0(ultra low), 1(standard), 2(high), 3(ultra high)
     b) Czekaj: 4.5ms (oss=0), 7.5ms (oss=1), 13.5ms (oss=2), 25.5ms (oss=3)
     c) Odczytaj surowe ciśnienie (UP) z rejestrów 0xF6–0xF8

  4. Kompensacja (algorytm z datasheetu):
     UT, UP + współczynniki → temperatura [°C] + ciśnienie [Pa]
```

### 8.3 Mapa rejestrów

| Rejestr | Adres | Opis |
|---------|-------|------|
| `CALIB_DATA` | 0xAA–0xBF | 22 bajty kalibracyjne (11 × int16) |
| `CHIP_ID` | 0xD0 | Identyfikator = 0x55 |
| `CTRL_MEAS` | 0xF4 | Rejestr sterujący pomiarem |
| `OUT_MSB` | 0xF6 | Wynik pomiaru (MSB) |
| `OUT_LSB` | 0xF7 | Wynik pomiaru (LSB) |
| `OUT_XLSB` | 0xF8 | Wynik pomiaru (XLSB, ciśnienie) |

### 8.4 Schemat podłączenia

```
ESP32 NodeMCU-32 — BMP180
┌──────────────────┐       ┌─────────────────┐
│                  │       │   BMP180 (GY-68) │
│  GPIO21 (SDA) ───┼───────┤ SDA              │
│  GPIO22 (SCL) ───┼───────┤ SCL              │
│  3.3V ───────────┼───────┤ VCC              │
│  GND ────────────┼───────┤ GND              │
│                  │       └─────────────────┘
└──────────────────┘
  Pull-up: 4.7 kΩ na SDA i SCL
```

### 8.5 Kompletny kod — BMP180

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "BMP180";

#define I2C_SDA_PIN     GPIO_NUM_21
#define I2C_SCL_PIN     GPIO_NUM_22
#define BMP180_ADDR     0x77

// Rejestry
#define BMP180_CHIP_ID_REG  0xD0
#define BMP180_CTRL_REG     0xF4
#define BMP180_DATA_REG     0xF6
#define BMP180_CALIB_REG    0xAA

// Komendy pomiaru
#define BMP180_CMD_TEMP     0x2E
#define BMP180_CMD_PRES_0   0x34  // oss=0 (ultra low power)
#define BMP180_CMD_PRES_1   0x74  // oss=1 (standard)
#define BMP180_CMD_PRES_2   0xB4  // oss=2 (high resolution)
#define BMP180_CMD_PRES_3   0xF4  // oss=3 (ultra high resolution)

#define BMP180_OSS          1     // Oversampling setting (0–3)

static i2c_master_dev_handle_t bmp_dev;

// Współczynniki kalibracyjne
static int16_t  ac1, ac2, ac3, b1, b2, mb, mc, md;
static uint16_t ac4, ac5, ac6;

// Funkcje pomocnicze I2C
static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, -1);
}
static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev,
                               uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

// ═══ Odczyt współczynników kalibracyjnych ═══
static esp_err_t bmp180_read_calibration(void)
{
    uint8_t buf[22];
    ESP_ERROR_CHECK(i2c_read_reg(bmp_dev, BMP180_CALIB_REG, buf, 22));

    ac1 = (int16_t)((buf[0]  << 8) | buf[1]);
    ac2 = (int16_t)((buf[2]  << 8) | buf[3]);
    ac3 = (int16_t)((buf[4]  << 8) | buf[5]);
    ac4 = (uint16_t)((buf[6]  << 8) | buf[7]);
    ac5 = (uint16_t)((buf[8]  << 8) | buf[9]);
    ac6 = (uint16_t)((buf[10] << 8) | buf[11]);
    b1  = (int16_t)((buf[12] << 8) | buf[13]);
    b2  = (int16_t)((buf[14] << 8) | buf[15]);
    mb  = (int16_t)((buf[16] << 8) | buf[17]);
    mc  = (int16_t)((buf[18] << 8) | buf[19]);
    md  = (int16_t)((buf[20] << 8) | buf[21]);

    ESP_LOGI(TAG, "Kalibracja: AC1=%d AC2=%d AC3=%d AC4=%u AC5=%u AC6=%u",
             ac1, ac2, ac3, ac4, ac5, ac6);
    ESP_LOGI(TAG, "            B1=%d B2=%d MB=%d MC=%d MD=%d",
             b1, b2, mb, mc, md);
    return ESP_OK;
}

// ═══ Odczyt surowej temperatury ═══
static int32_t bmp180_read_raw_temp(void)
{
    i2c_write_reg(bmp_dev, BMP180_CTRL_REG, BMP180_CMD_TEMP);
    vTaskDelay(pdMS_TO_TICKS(5));  // Min. 4.5 ms

    uint8_t buf[2];
    i2c_read_reg(bmp_dev, BMP180_DATA_REG, buf, 2);
    return (int32_t)((buf[0] << 8) | buf[1]);
}

// ═══ Odczyt surowego ciśnienia ═══
static int32_t bmp180_read_raw_pressure(void)
{
    uint8_t cmd = 0x34 + (BMP180_OSS << 6);
    i2c_write_reg(bmp_dev, BMP180_CTRL_REG, cmd);

    // Czas oczekiwania zależny od oss
    int delays[] = {5, 8, 14, 26};
    vTaskDelay(pdMS_TO_TICKS(delays[BMP180_OSS]));

    uint8_t buf[3];
    i2c_read_reg(bmp_dev, BMP180_DATA_REG, buf, 3);
    return (int32_t)(((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> (8 - BMP180_OSS));
}

// ═══ Kompensacja (algorytm z datasheetu Bosch) ═══
static void bmp180_calculate(int32_t ut, int32_t up, float *temp_c, float *press_hpa)
{
    // Temperatura
    int32_t x1 = ((ut - (int32_t)ac6) * (int32_t)ac5) >> 15;
    int32_t x2 = ((int32_t)mc << 11) / (x1 + md);
    int32_t b5 = x1 + x2;
    *temp_c = (b5 + 8) / 160.0f;  // °C (z dokładnością 0.1)

    // Ciśnienie
    int32_t b6 = b5 - 4000;
    x1 = (b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = (ac2 * b6) >> 11;
    int32_t x3 = x1 + x2;
    int32_t b3 = (((((int32_t)ac1) * 4 + x3) << BMP180_OSS) + 2) >> 2;
    x1 = (ac3 * b6) >> 13;
    x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    uint32_t b4 = (ac4 * (uint32_t)(x3 + 32768)) >> 15;
    uint32_t b7 = ((uint32_t)up - b3) * (50000 >> BMP180_OSS);
    int32_t p;
    if (b7 < 0x80000000)
        p = (b7 * 2) / b4;
    else
        p = (b7 / b4) * 2;
    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);

    *press_hpa = p / 100.0f;  // Pa → hPa
}

// ═══ Inicjalizacja ═══
static esp_err_t bmp180_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BMP180_ADDR,
        .scl_speed_hz    = 100000,  // Standard mode
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &bmp_dev));

    // Weryfikacja Chip ID
    uint8_t chip_id;
    i2c_read_reg(bmp_dev, BMP180_CHIP_ID_REG, &chip_id, 1);
    ESP_LOGI(TAG, "Chip ID = 0x%02X (oczekiwane: 0x55)", chip_id);
    if (chip_id != 0x55) {
        ESP_LOGE(TAG, "BMP180 nie wykryty!");
        return ESP_FAIL;
    }

    // Odczyt kalibracji
    bmp180_read_calibration();

    ESP_LOGI(TAG, "BMP180 zainicjalizowany (oss=%d)", BMP180_OSS);
    return ESP_OK;
}

// ═══ app_main ═══
void app_main(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = I2C_PORT_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    bmp180_init(bus);

    while (1) {
        int32_t raw_temp = bmp180_read_raw_temp();
        int32_t raw_pres = bmp180_read_raw_pressure();

        float temp_c, press_hpa;
        bmp180_calculate(raw_temp, raw_pres, &temp_c, &press_hpa);

        // Obliczenie wysokości barometrycznej (wzór hipsometryczny)
        // Ciśnienie na poziomie morza: 1013.25 hPa
        float altitude = 44330.0f * (1.0f - powf(press_hpa / 1013.25f, 0.1903f));

        ESP_LOGI(TAG, "Temp: %.1f °C | Ciśnienie: %.2f hPa | Wysokość: %.1f m",
                 temp_c, press_hpa, altitude);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

> **💡 Wzór hipsometryczny:** Pozwala oszacować wysokość n.p.m. na podstawie ciśnienia atmosferycznego. Wymaga znajomości ciśnienia referencyjnego na poziomie morza (domyślnie 1013.25 hPa). Dla dokładnych wyników należy zaktualizować ciśnienie referencyjne z aktualnych danych pogodowych.

---

## 9. Ćwiczenie 4: Detektor koloru TCS34725

### 9.1 Opis czujnika

**TCS34725** (ams/OSRAM) to cyfrowy czujnik koloru z filtrem IR. Mierzy natężenie światła w kanałach **Red, Green, Blue** oraz **Clear** (bez filtra). Popularny moduł breakout: **CJMCU-34725** lub **Adafruit TCS34725**.

| Parametr | Wartość |
|----------|---------|
| **Adres I2C** | 0x29 (stały) |
| **Napięcie** | 2.7V – 3.6V (moduł: 3.3V–5V) |
| **Kanały** | Clear, Red, Green, Blue (16-bit każdy) |
| **Czas integracji** | 2.4 ms – 614 ms (konfigurowalny, rejestr ATIME) |
| **Wzmocnienie** | 1×, 4×, 16×, 60× |
| **Filtr IR** | Wbudowany (blokuje podczerwień) |
| **LED** | Białe LED na module (pin kontrolny) |
| **Device ID** | Rejestr 0x12 → wartość 0x44 lub 0x4D |
| **Datasheet** | ams TCS34725 Color Light-to-Digital Converter |

### 9.2 Mapa rejestrów

> **⚠️ Uwaga:** Wszystkie adresy rejestrów TCS34725 muszą mieć ustawiony **bit COMMAND** (bit 7 = 1). Dlatego do operacji I2C zawsze dodawaj `0x80` do adresu rejestru: `reg_addr | 0x80`.

| Rejestr | Adres | Adres z CMD | Opis |
|---------|-------|-------------|------|
| `ENABLE` | 0x00 | 0x80 | Włączanie czujnika (PON, AEN) |
| `ATIME` | 0x01 | 0x81 | Czas integracji ADC |
| `CONTROL` | 0x0F | 0x8F | Wzmocnienie (gain) |
| `ID` | 0x12 | 0x92 | Identyfikator urządzenia |
| `STATUS` | 0x13 | 0x93 | Status (bit 0 = AVALID) |
| `CDATAL` | 0x14 | 0x94 | Clear channel (low byte) |
| `RDATAL` | 0x16 | 0x96 | Red channel (low byte) |
| `GDATAL` | 0x18 | 0x98 | Green channel (low byte) |
| `BDATAL` | 0x1A | 0x9A | Blue channel (low byte) |

### 9.3 Schemat podłączenia

```
ESP32 NodeMCU-32 — TCS34725
┌──────────────────┐       ┌─────────────────┐
│                  │       │   TCS34725       │
│  GPIO21 (SDA) ───┼───────┤ SDA              │
│  GPIO22 (SCL) ───┼───────┤ SCL              │
│  3.3V ───────────┼───────┤ VCC              │
│  GND ────────────┼───────┤ GND              │
│                  │       │ LED → (opcj. do GPIO)│
│                  │       └─────────────────┘
└──────────────────┘
  Pull-up: 4.7 kΩ na SDA i SCL
```

### 9.4 Kompletny kod — TCS34725

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "TCS34725";

#define I2C_SDA_PIN     GPIO_NUM_21
#define I2C_SCL_PIN     GPIO_NUM_22
#define TCS34725_ADDR   0x29

// Rejestry (z bitem COMMAND = 0x80)
#define TCS_CMD         0x80  // Command bit — zawsze ustawiaj!
#define TCS_ENABLE      (TCS_CMD | 0x00)
#define TCS_ATIME       (TCS_CMD | 0x01)
#define TCS_CONTROL     (TCS_CMD | 0x0F)
#define TCS_ID          (TCS_CMD | 0x12)
#define TCS_STATUS      (TCS_CMD | 0x13)
#define TCS_CDATAL      (TCS_CMD | 0x14)
#define TCS_RDATAL      (TCS_CMD | 0x16)
#define TCS_GDATAL      (TCS_CMD | 0x18)
#define TCS_BDATAL      (TCS_CMD | 0x1A)

// ENABLE bits
#define TCS_ENABLE_PON  0x01  // Power ON
#define TCS_ENABLE_AEN  0x02  // RGBC Enable

// Gain values
#define TCS_GAIN_1X     0x00
#define TCS_GAIN_4X     0x01
#define TCS_GAIN_16X    0x02
#define TCS_GAIN_60X    0x03

static i2c_master_dev_handle_t tcs_dev;

static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, -1);
}
static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev,
                               uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

// ═══ Inicjalizacja TCS34725 ═══
static esp_err_t tcs34725_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = TCS34725_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &tcs_dev));

    // Weryfikacja ID
    uint8_t id;
    i2c_read_reg(tcs_dev, TCS_ID, &id, 1);
    ESP_LOGI(TAG, "Device ID = 0x%02X (oczekiwane: 0x44 lub 0x4D)", id);
    if (id != 0x44 && id != 0x4D) {
        ESP_LOGE(TAG, "TCS34725 nie wykryty!");
        return ESP_FAIL;
    }

    // Czas integracji: 0xFF = 2.4ms, 0xF6 = 24ms, 0xD5 = 101ms, 0xC0 = 154ms
    i2c_write_reg(tcs_dev, TCS_ATIME, 0xD5);     // 101 ms
    i2c_write_reg(tcs_dev, TCS_CONTROL, TCS_GAIN_4X);  // Gain 4×

    // Włączenie czujnika: Power ON → czekaj 2.4ms → RGBC Enable
    i2c_write_reg(tcs_dev, TCS_ENABLE, TCS_ENABLE_PON);
    vTaskDelay(pdMS_TO_TICKS(3));
    i2c_write_reg(tcs_dev, TCS_ENABLE, TCS_ENABLE_PON | TCS_ENABLE_AEN);
    vTaskDelay(pdMS_TO_TICKS(105));  // Czekaj na pierwszy pomiar (ATIME + margines)

    ESP_LOGI(TAG, "TCS34725 zainicjalizowany (ATIME=101ms, Gain=4x)");
    return ESP_OK;
}

// ═══ Odczyt RGBC ═══
typedef struct {
    uint16_t clear, red, green, blue;
} tcs34725_rgbc_t;

static esp_err_t tcs34725_read(tcs34725_rgbc_t *out)
{
    // Sprawdź czy dane gotowe (AVALID)
    uint8_t status;
    i2c_read_reg(tcs_dev, TCS_STATUS, &status, 1);
    if (!(status & 0x01)) {
        ESP_LOGW(TAG, "Dane nie gotowe (AVALID=0)");
        return ESP_ERR_NOT_FINISHED;
    }

    uint8_t buf[8];
    // Odczyt 8 bajtów od CDATAL (Clear, Red, Green, Blue — po 2 bajty LE)
    i2c_read_reg(tcs_dev, TCS_CDATAL, buf, 8);

    out->clear = (uint16_t)(buf[1] << 8 | buf[0]);
    out->red   = (uint16_t)(buf[3] << 8 | buf[2]);
    out->green = (uint16_t)(buf[5] << 8 | buf[4]);
    out->blue  = (uint16_t)(buf[7] << 8 | buf[6]);

    return ESP_OK;
}

// ═══ Konwersja RGBC → RGB (0–255) ═══
static void tcs34725_to_rgb(tcs34725_rgbc_t *raw, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (raw->clear == 0) {
        *r = *g = *b = 0;
        return;
    }
    // Normalizacja kanałów do zakresu 0–255
    *r = (uint8_t)((uint32_t)raw->red   * 255 / raw->clear);
    *g = (uint8_t)((uint32_t)raw->green * 255 / raw->clear);
    *b = (uint8_t)((uint32_t)raw->blue  * 255 / raw->clear);
}

// ═══ app_main ═══
void app_main(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = I2C_PORT_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    tcs34725_init(bus);

    tcs34725_rgbc_t raw;
    uint8_t r, g, b;

    while (1) {
        if (tcs34725_read(&raw) == ESP_OK) {
            tcs34725_to_rgb(&raw, &r, &g, &b);

            ESP_LOGI(TAG, "Raw: C=%u R=%u G=%u B=%u",
                     raw.clear, raw.red, raw.green, raw.blue);
            ESP_LOGI(TAG, "RGB: R=%u G=%u B=%u", r, g, b);

            // Detekcja koloru dominującego
            const char *color = "nieznany";
            if (r > g && r > b)      color = "CZERWONY";
            else if (g > r && g > b)  color = "ZIELONY";
            else if (b > r && b > g)  color = "NIEBIESKI";
            else                      color = "BIALY/SZARY";
            ESP_LOGI(TAG, "Kolor dominujący: %s", color);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

> **💡 Tip:** Czas integracji (`ATIME`) kontroluje czułość. Formuła: `Integration time = (256 − ATIME) × 2.4 ms`. Dłuższy czas = wyższa czułość ale wolniejszy pomiar. Dla typowych zastosowań 101 ms (ATIME=0xD5) jest dobrym kompromisem.

---

## 10. Ćwiczenie 5: Czujnik odległości VL53L0X — Time-of-Flight

### 10.1 Opis czujnika

**VL53L0X** (STMicroelectronics) to laserowy czujnik odległości typu **Time-of-Flight (ToF)**. Emituje wiązkę lasera IR (VCSEL 940 nm) i mierzy czas powrotu odbitego światła. Popularny moduł breakout: **GY-VL53L0XV2**.

| Parametr | Wartość |
|----------|---------|
| **Adres I2C** | 0x29 (domyślny, programowalny) |
| **Napięcie** | 2.6V – 3.5V (moduł: 3.3V–5V) |
| **Zasięg** | 30 mm – 2000 mm (tryb long range) |
| **Dokładność** | ±3% (typowe warunki) |
| **Czas pomiaru** | 20 ms (High Speed) – 200 ms (High Accuracy) |
| **Źródło światła** | VCSEL 940 nm (Class 1 laser) |
| **FoV** | 25° (Field of View) |
| **Pin XSHUT** | GPIO shutdown — do resetu i zmiany adresu |
| **Datasheet** | STMicroelectronics VL53L0X DS11555 |

### 10.2 Zasada działania Time-of-Flight

```
Pomiar Time-of-Flight — zasada:

  ┌──────────┐            ┌──────────────┐
  │ VL53L0X  │ ─── laser ──→ │   Obiekt     │
  │          │ ←── odbicie ── │  (przeszkoda)│
  └──────────┘            └──────────────┘

  Dystans = (c × t) / 2

  Gdzie:
    c = prędkość światła (3 × 10⁸ m/s)
    t = czas podróży fotonu (tam i z powrotem)
    /2 = droga odpowiada podwójnej odległości

  VL53L0X mierzy t z dokładnością pikosekund (SPAD array)
  i przelicza na dystans w milimetrach.
```

### 10.3 Tryby pomiaru

| Tryb | Zasięg | Czas | Dokładność | Ustawienie |
|------|--------|------|-----------|-----------|
| **Default** | ~1.2 m | 30 ms | ±3% | Domyślny po inicjalizacji |
| **High Speed** | ~1.2 m | 20 ms | ±5% | `timing_budget = 20000` |
| **High Accuracy** | ~1.2 m | 200 ms | ±2% | `timing_budget = 200000` |
| **Long Range** | ~2.0 m | 33 ms | ±5% | Specjalna konfiguracja |

### 10.4 Schemat podłączenia

```
ESP32 NodeMCU-32 — VL53L0X
┌──────────────────┐       ┌─────────────────┐
│                  │       │   VL53L0X        │
│  GPIO21 (SDA) ───┼───────┤ SDA              │
│  GPIO22 (SCL) ───┼───────┤ SCL              │
│  3.3V ───────────┼───────┤ VCC              │
│  GND ────────────┼───────┤ GND              │
│  GPIO4 ──────────┼───────┤ XSHUT (opcj.)    │
│                  │       │ GPIO1 = INT (opc.)│
│                  │       └─────────────────┘
└──────────────────┘
  Pull-up: 4.7 kΩ na SDA i SCL
  XSHUT: do kontroli zasilania/zmiany adresu I2C
```

> **⚠️ Konflikt adresów:** VL53L0X i TCS34725 mają ten sam domyślny adres (0x29). Użyj pinu XSHUT do zmiany adresu VL53L0X lub multipleksera TCA9548A.

### 10.5 Kompletny kod — VL53L0X (uproszczony sterownik)

VL53L0X wymaga złożonej sekwencji inicjalizacji. Poniższy kod implementuje **uproszczony sterownik** z pomiarami w trybie single-shot:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "VL53L0X";

#define I2C_SDA_PIN     GPIO_NUM_21
#define I2C_SCL_PIN     GPIO_NUM_22
#define VL53L0X_ADDR    0x29

// Rejestry VL53L0X (wybrane)
#define VL53_REG_IDENTIFICATION_MODEL_ID    0xC0
#define VL53_REG_SYSRANGE_START             0x00
#define VL53_REG_RESULT_RANGE_STATUS        0x14
#define VL53_REG_RESULT_INTERRUPT_STATUS    0x13
#define VL53_REG_SYSTEM_INTERRUPT_CLEAR     0x0B
#define VL53_REG_I2C_SLAVE_DEVICE_ADDRESS   0x8A

static i2c_master_dev_handle_t vl_dev;

static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, -1);
}
static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev,
                               uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

// ═══ Inicjalizacja VL53L0X ═══
static esp_err_t vl53l0x_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = VL53L0X_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &vl_dev));

    // Weryfikacja Model ID (powinno być 0xEE)
    uint8_t model_id;
    i2c_read_reg(vl_dev, VL53_REG_IDENTIFICATION_MODEL_ID, &model_id, 1);
    ESP_LOGI(TAG, "Model ID = 0x%02X (oczekiwane: 0xEE)", model_id);
    if (model_id != 0xEE) {
        ESP_LOGE(TAG, "VL53L0X nie wykryty!");
        return ESP_FAIL;
    }

    // Konfiguracja standardowa (uproszczona)
    // Pełna inicjalizacja wymaga sekwencji ~40 zapisów rejestrów
    // (patrz: ST API VL53L0X_DataInit, VL53L0X_StaticInit, VL53L0X_PerformRefCalibration)

    // Krok 1: Data init
    i2c_write_reg(vl_dev, 0x88, 0x00);  // VHV config
    i2c_write_reg(vl_dev, 0x80, 0x01);
    i2c_write_reg(vl_dev, 0xFF, 0x01);
    i2c_write_reg(vl_dev, 0x00, 0x00);
    i2c_write_reg(vl_dev, 0x91, 0x3C);
    i2c_write_reg(vl_dev, 0x00, 0x01);
    i2c_write_reg(vl_dev, 0xFF, 0x00);
    i2c_write_reg(vl_dev, 0x80, 0x00);

    ESP_LOGI(TAG, "VL53L0X zainicjalizowany");
    return ESP_OK;
}

// ═══ Pojedynczy pomiar (single-shot) ═══
static esp_err_t vl53l0x_read_range_mm(uint16_t *range_mm)
{
    // Start pomiaru single-shot
    i2c_write_reg(vl_dev, VL53_REG_SYSRANGE_START, 0x01);

    // Czekaj na zakończenie pomiaru (polling)
    uint8_t status = 0;
    int timeout = 100;  // Max 100 × 10ms = 1s
    while (timeout-- > 0) {
        i2c_read_reg(vl_dev, VL53_REG_RESULT_RANGE_STATUS, &status, 1);
        if (status & 0x01) break;  // Device ready
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (timeout <= 0) {
        ESP_LOGW(TAG, "Timeout pomiaru!");
        return ESP_ERR_TIMEOUT;
    }

    // Odczyt wyniku (2 bajty: 0x1E–0x1F)
    uint8_t buf[12];
    i2c_read_reg(vl_dev, VL53_REG_RESULT_RANGE_STATUS, buf, 12);

    // Sprawdzenie statusu (DeviceRangeStatusInternal)
    uint8_t range_status = (buf[0] & 0x78) >> 3;
    *range_mm = (uint16_t)((buf[10] << 8) | buf[11]);

    // Wyczyść przerwanie
    i2c_write_reg(vl_dev, VL53_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    if (range_status != 0x0B) {
        // 0x0B = Range Valid (sigma/signal OK)
        ESP_LOGD(TAG, "Range status: 0x%02X (może być nieprecyzyjny)", range_status);
    }

    // Filtr: wartości > 8000 mm są zwykle błędne
    if (*range_mm > 8000) {
        ESP_LOGW(TAG, "Wartość poza zakresem: %u mm", *range_mm);
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

// ═══ app_main ═══
void app_main(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = I2C_PORT_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    vl53l0x_init(bus);

    uint16_t range;
    while (1) {
        if (vl53l0x_read_range_mm(&range) == ESP_OK) {
            ESP_LOGI(TAG, "Odległość: %u mm (%.1f cm)", range, range / 10.0f);

            // Wizualizacja odległości
            int bars = range / 50;  // 1 bar = 50 mm
            if (bars > 40) bars = 40;
            char bar_str[42] = {0};
            for (int i = 0; i < bars; i++) bar_str[i] = '#';
            ESP_LOGI(TAG, "[%s] %u mm", bar_str, range);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

> **💡 Produkcja:** Dla pełnej funkcjonalności VL53L0X (kalibracja SPAD, kalibracja temperaturowa, tryby ciągłe, długi zasięg) zalecamy użycie oficjalnej biblioteki **VL53L0X API** od STMicroelectronics. Powyższy kod to uproszczony sterownik do nauki i prototypowania.

> **⚠️ Zmiana adresu I2C VL53L0X:** Aby zmienić adres programowo, zapisz nowy adres (7-bit) do rejestru `0x8A`. Nowy adres obowiązuje do resetu zasilania. Aby zmienić adres na stałe, użyj pinu XSHUT: wyłącz czujnik (XSHUT=LOW), włącz (XSHUT=HIGH), natychmiast zapisz nowy adres — powtórz dla każdego VL53L0X z innym adresem.

---

## 11. Podsumowanie i dalsze kroki

### 11.1 Co poznaliśmy

| Temat | Kluczowe informacje |
|-------|-------------------|
| **Protokół I2C** | 2-przewodowy (SDA+SCL), synchroniczny, multi-slave, ACK/NACK |
| **ESP32 I2C** | 2 kontrolery, Standard (100 kHz) / Fast (400 kHz), GPIO Matrix |
| **API ESP-IDF v5.x** | Bus-Device model: `i2c_new_master_bus()` + `i2c_master_bus_add_device()` |
| **Skanowanie** | `i2c_master_probe()` — wykrywanie urządzeń na magistrali |
| **SSD1306** | OLED 128×64, bufor ramki 1024B, komendy + dane, czcionka 5×7 |
| **MPU6050** | 6-osiowy IMU, 16-bit ADC, kalibracja osi, temperatura |
| **BMP180** | Ciśnienie barometryczne, 11 współczynników kalibracji, wzór hipsometryczny |
| **TCS34725** | Detektor koloru RGBC, czas integracji, wzmocnienie, filtr IR |
| **VL53L0X** | Time-of-Flight, laser VCSEL 940nm, zasięg do 2m, zmiana adresu I2C |

### 11.2 Typowe problemy i rozwiązania

| Problem | Przyczyna | Rozwiązanie |
|---------|-----------|-------------|
| `ESP_ERR_TIMEOUT` | Brak pull-upów | Dodaj zewnętrzne 4.7 kΩ pull-upy na SDA i SCL |
| Skan nie znajduje urządzeń | Złe połączenie / adres | Sprawdź lutowania, długość kabli, napięcie |
| NACK zamiast ACK | Zły adres lub urządzenie w SLEEP | Sprawdź adres w datasheecie, wybudź urządzenie |
| Konflikt adresów (0x29) | TCS34725 + VL53L0X | Zmień adres VL53L0X lub użyj TCA9548A |
| Szum danych IMU | Brak filtrowania | Użyj DLPF w MPU6050, lub filtr Kalmana w software |
| OLED nie wyświetla | Zła sekwencja init | Sprawdź charge pump (0x8D, 0x14), kontrast |

### 11.3 Dalsze kroki

- **Moduł 3.3 — SPI:** Komunikacja z urządzeniami SPI (wyświetlacze TFT, czytniki SD)
- **Projekt integracyjny:** Stacja pogodowa (BMP180 + SSD1306) z logowaniem na kartę SD
- **Filtrowanie danych:** Implementacja filtru Kalmana dla MPU6050
- **Multi-sensor I2C:** Podłączenie wszystkich 5 czujników na jednej magistrali

---

## 12. Źródła i dokumentacja

### 12.1 Dokumentacja ESP-IDF

| Zasób | Link |
|-------|------|
| **I2C — ESP-IDF Programming Guide** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/i2c.html |
| **ESP-IDF I2C Examples** | https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2c |
| **ESP32 Technical Reference Manual** | https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf (Rozdział 11: I2C) |

### 12.2 Datasheety czujników

| Czujnik | Producent | Dokument |
|---------|-----------|----------|
| **SSD1306** | Solomon Systech | SSD1306 Advance Information Rev 1.1 — 128×64 Dot Matrix OLED/PLED Controller/Driver |
| **MPU6050** | InvenSense (TDK) | MPU-6000/MPU-6050 Register Map and Descriptions RM-MPU-6000A-00 Rev 4.2 |
| **BMP180** | Bosch Sensortec | BMP180 Digital Pressure Sensor Data Sheet BST-BMP180-DS000-12 |
| **TCS34725** | ams (OSRAM) | TCS34725 Color Light-to-Digital Converter Data Sheet |
| **VL53L0X** | STMicroelectronics | VL53L0X World's smallest Time-of-Flight ranging sensor DS11555 Rev 9 |

### 12.3 Specyfikacja I2C

| Dokument | Opis |
|----------|------|
| **UM10204** | I2C-bus specification and user manual — NXP Semiconductors Rev 7.0 |
| | https://www.nxp.com/docs/en/user-guide/UM10204.pdf |

### 12.4 Przydatne narzędzia

| Narzędzie | Opis |
|-----------|------|
| **Logic Analyzer** | Saleae Logic / PulseView — dekodowanie sygnałów I2C |
| **i2cdetect** | Skaner I2C (Linux) — `i2cdetect -y 1` |
| **ESP-IDF i2c_tools** | Przykład ESP-IDF: `examples/peripherals/i2c/i2c_tools` |
