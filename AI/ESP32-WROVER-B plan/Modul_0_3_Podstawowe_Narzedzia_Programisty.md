# Moduł 0.3 — Podstawowe narzędzia programisty

> **Poziom:** 🟢 Laik · **Czas:** Tydzień 1–2 (Faza 0)  
> **Cel:** Opanowanie kluczowych narzędzi ESP-IDF: menuconfig, UART monitor, system logowania oraz opcjonalnie JTAG debugging.

---

## Spis treści

1. [idf.py menuconfig — konfiguracja projektu](#1-idfpy-menuconfig--konfiguracja-projektu)
2. [idf.py monitor — UART monitor](#2-idfpy-monitor--uart-monitor)
3. [System logowania ESP-IDF](#3-system-logowania-esp-idf)
4. [JTAG Debugging](#4-jtag-debugging-opcjonalnie)
5. [Projekt przykładowy — wszystkie narzędzia w akcji](#5-projekt-przykładowy--wszystkie-narzędzia-w-akcji)
6. [Podsumowanie i dalsze kroki](#6-podsumowanie-i-dalsze-kroki)
7. [Źródła i dokumentacja](#7-źródła-i-dokumentacja)

---

## 1. idf.py menuconfig — konfiguracja projektu

### 1.1 Czym jest menuconfig?

`menuconfig` to **interaktywny konfigurator** projektu ESP-IDF oparty na systemie **Kconfig** (tym samym, którego używa jądro Linux). Pozwala na graficzne ustawianie setek opcji — od częstotliwości CPU, przez poziomy logowania, rozmiar stosu, aż po konfigurację WiFi, Bluetooth i peryferiów.

**Kluczowe fakty:**
- Konfiguracja zapisywana jest w pliku `sdkconfig` (klucz=wartość)
- **Nigdy nie edytuj `sdkconfig` ręcznie** — używaj `menuconfig`!
- Każdy komponent ESP-IDF definiuje swoje opcje w plikach `Kconfig`
- Opcje mają zależności — menuconfig automatycznie ukrywa/pokazuje opcje

### 1.2 Uruchomienie menuconfig

**Z terminala (klasyczna wersja TUI):**
```bash
idf.py menuconfig
```

**Z VS Code (graficzna wersja — zalecana):**
```
Ctrl+Shift+P → "ESP-IDF: SDK Configuration editor (Menuconfig)"
```
Lub kliknij ikonę ⚙️ na dolnym pasku VS Code.

> **💡 Wskazówka:** Wersja graficzna w VS Code jest znacznie wygodniejsza — oferuje wyszukiwanie, tooltips i kolorowe wyróżnienie zmian.

### 1.3 Nawigacja w menuconfig (terminal)

```
┌───────────────────── ESP-IDF Project Configuration ──────────────────────┐
│                                                                           │
│  SDK tool configuration  --->                                             │
│  Build type  --->                                                         │
│  Bootloader config  --->                                                  │
│  Security features  --->                                                  │
│  Serial flasher config  --->                                              │
│  Partition Table  --->                                                     │
│  Compiler options  --->                                                    │
│  Component config  --->                                                    │
│     ├── Bluetooth  --->                                                    │
│     ├── Driver Configurations  --->                                        │
│     ├── ESP System Settings  --->                                          │
│     ├── ESP PSRAM  --->                                                    │
│     ├── FreeRTOS  --->                                                     │
│     ├── Log output  --->                                                   │
│     ├── LWIP  --->                                                         │
│     └── Wi-Fi  --->                                                        │
│                                                                           │
├───────────────────────────────────────────────────────────────────────────┤
│ ↑↓: Nawigacja  │  Enter: Wejdź  │  Space/Y/N: Toggle  │  /: Szukaj     │
│ ?: Pomoc       │  S: Zapisz     │  Q: Wyjdź           │  D: Defaults   │
└───────────────────────────────────────────────────────────────────────────┘
```

| Klawisz | Akcja |
|---------|-------|
| `↑` `↓` | Nawigacja w menu |
| `Enter` | Wejście do podmenu / edycja wartości |
| `Space` lub `Y` | Włączenie opcji `[*]` |
| `N` | Wyłączenie opcji `[ ]` |
| `/` | **Wyszukiwanie** — kluczowa funkcja! |
| `?` | Pomoc dla wybranej opcji (opis, zależności) |
| `S` | Zapisanie konfiguracji |
| `Q` | Wyjście |
| `D` | Przywrócenie domyślnych wartości |

### 1.4 Wyszukiwanie opcji ( / )

Wyszukiwanie to **najważniejsza funkcja** menuconfig. Zamiast przekopywać się przez dziesiątki podmenu, naciśnij `/` i wpisz szukaną frazę:

```
Symbol: LOG_DEFAULT_LEVEL [=3]
Type  : int
Range : [0 5]
Prompt: Default log verbosity
  Location:
    -> Component config
      -> Log output (LOG_OUTPUT)
  Defined at: components/log/Kconfig:3
  Help text:
    Specify how much output to see in logs by default.
    Note: can be overridden at runtime using esp_log_level_set()
```

**Przykładowe wyszukiwania:**
| Szukaj | Znajdziesz |
|--------|-----------|
| `SPIRAM` | Wszystkie opcje PSRAM |
| `CPU_FREQ` | Częstotliwość procesora |
| `LOG_DEFAULT` | Domyślny poziom logów |
| `FREERTOS` | Opcje FreeRTOS (tick rate, stack size) |
| `FLASH_SIZE` | Rozmiar pamięci Flash |
| `WATCHDOG` | Konfiguracja watchdog timers |

### 1.5 Najważniejsze opcje menuconfig

#### Częstotliwość CPU
```
Component config → ESP System Settings → CPU frequency
├── ( ) 80 MHz
├── ( ) 160 MHz
└── (X) 240 MHz    ← Zalecane dla ESP32-WROVER-B
```

#### Konfiguracja PSRAM
```
Component config → ESP PSRAM
├── [*] Support for external, SPI-connected RAM
├── SPI RAM config  --->
│   ├── Type of SPI RAM chip: Auto detect
│   ├── Set RAM clock speed: 80 MHz
│   └── SPI RAM access method
│       ├── ( ) Integrate RAM into memory map     ← Zaawansowane
│       ├── (X) Make RAM allocatable using malloc  ← Zalecane
│       └── ( ) Allow .bss segment placed in PSRAM
```

#### Poziomy logowania
```
Component config → Log output
├── Default log verbosity: Info   ← Domyślny poziom
├── [*] Use ANSI terminal colors in log output
└── Maximum log verbosity: Verbose ← Maks. kompilowany poziom
```

#### Rozmiar Flash
```
Serial flasher config
├── Flash SPI mode: QIO (najszybszy)
├── Flash SPI speed: 80 MHz
└── Flash size: 4 MB  ← Dostosuj do modułu
```

#### Tablica partycji
```
Partition Table
├── Partition Table: Single factory app, no OTA
│   ├── ( ) Single factory app, no OTA
│   ├── ( ) Factory app, two OTA definitions
│   └── ( ) Custom partition table CSV
└── Offset of partition table: 0x8000
```

#### FreeRTOS
```
Component config → FreeRTOS → Kernel
├── configTICK_RATE_HZ: 1000  ← Ticki na sekundę
├── configMINIMAL_STACK_SIZE: 768  ← Min. stos (w słowach 32-bit)
└── configUSE_TICKLESS_IDLE: [ ]  ← Dla power saving
```

### 1.6 System Kconfig — jak to działa od środka

Każdy komponent ESP-IDF definiuje swoje opcje w pliku `Kconfig`:

```kconfig
# Przykład: components/log/Kconfig
menu "Log output"
    config LOG_DEFAULT_LEVEL
        int "Default log verbosity"
        default 3
        range 0 5
        help
            Specify how much output to see in logs by default.
            0 = None, 1 = Error, 2 = Warn, 3 = Info, 4 = Debug, 5 = Verbose

    config LOG_COLORS
        bool "Use ANSI terminal colors in log output"
        default y
        help
            Enable ANSI color codes in log output.
endmenu
```

**Możesz dodawać własne opcje menuconfig** do projektu! Utwórz plik `main/Kconfig.projbuild`:

```kconfig
# main/Kconfig.projbuild — opcje widoczne w menuconfig
menu "My Project Configuration"
    config MY_LED_GPIO
        int "LED GPIO number"
        default 2
        range 0 39
        help
            GPIO number for the onboard LED.

    config MY_LOG_ENABLED
        bool "Enable detailed logging"
        default y
        help
            Enable extra verbose logging for debugging.

    config MY_SENSOR_INTERVAL_MS
        int "Sensor read interval (ms)"
        default 1000
        range 100 60000
endmenu
```

Użycie w kodzie C:
```c
#include "sdkconfig.h"   // Automatycznie generowany header

void app_main(void)
{
    // Opcje z menuconfig dostępne jako #define CONFIG_xxx
    gpio_set_direction(CONFIG_MY_LED_GPIO, GPIO_MODE_OUTPUT);

    #if CONFIG_MY_LOG_ENABLED
    ESP_LOGI(TAG, "Szczegółowe logowanie włączone");
    ESP_LOGI(TAG, "Interwał czujnika: %d ms", CONFIG_MY_SENSOR_INTERVAL_MS);
    #endif

    while (1) {
        gpio_set_level(CONFIG_MY_LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_MY_SENSOR_INTERVAL_MS));
        gpio_set_level(CONFIG_MY_LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_MY_SENSOR_INTERVAL_MS));
    }
}
```

### 1.7 sdkconfig vs sdkconfig.defaults

| Plik | Opis | Git? |
|------|------|------|
| `sdkconfig` | Pełna konfiguracja (generowana) | ❌ NIE commituj |
| `sdkconfig.defaults` | Domyślne wartości (tworzona ręcznie) | ✅ TAK commituj |
| `sdkconfig.old` | Backup poprzedniej konfiguracji | ❌ NIE commituj |

**sdkconfig.defaults** — zdefiniuj tu opcje dla swojego projektu:
```ini
# sdkconfig.defaults — commituj do Git!
CONFIG_IDF_TARGET="esp32"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_FREERTOS_HZ=1000
```

Działanie: Gdy nie istnieje `sdkconfig`, ESP-IDF tworzy go z `sdkconfig.defaults`. Aby wymusić reset:
```bash
# Usuń sdkconfig i wygeneruj ponownie z defaults
del sdkconfig          # Windows
idf.py reconfigure     # Generuje nowy sdkconfig z sdkconfig.defaults
```

### 1.8 Przydatne komendy konfiguracyjne

```bash
# Otwórz menuconfig
idf.py menuconfig

# Zregeneruj sdkconfig bez otwierania menuconfig
idf.py reconfigure

# Zapisz bieżące różnice vs domyślne jako sdkconfig.defaults
idf.py save-defconfig

# Porównaj sdkconfig z defaults
diff sdkconfig sdkconfig.defaults
```

---

## 2. idf.py monitor — UART monitor

### 2.1 Czym jest IDF Monitor?

IDF Monitor to **zaawansowany terminal szeregowy** wbudowany w ESP-IDF. W porównaniu ze zwykłym terminalem (PuTTY, Tera Term) oferuje:

| Cecha | Zwykły terminal | IDF Monitor |
|-------|-----------------|-------------|
| Wyświetlanie logów | ✅ | ✅ |
| Kolorowanie logów | ❌ | ✅ Automatyczne |
| **Dekodowanie panic backtrace** | ❌ | ✅ Adresy → nazwy funkcji i linie |
| **Dekodowanie adresów** | ❌ | ✅ Automatycznie |
| Reset ESP32 | ❌ | ✅ `Ctrl+T → Ctrl+R` |
| Wejście w download mode | ❌ | ✅ `Ctrl+T → Ctrl+A` |
| Timestamp | ❌ | ✅ `Ctrl+T → Ctrl+Y` |
| Filtrowanie logów | ❌ | ✅ Po tagach i poziomach |
| Integracja z GDB | ❌ | ✅ GDB stub |

### 2.2 Uruchomienie monitora

```bash
# Podstawowe uruchomienie
idf.py -p COM3 monitor

# Build + Flash + Monitor (all-in-one) — najwygodniejsze
idf.py -p COM3 flash monitor

# Zmiana baud rate (domyślnie 115200)
idf.py -p COM3 -b 115200 monitor

# Alternatywna metoda w VS Code:
# Ctrl+E → M  (monitor)
# Ctrl+E → D  (build + flash + monitor)
```

### 2.3 Interfejs monitora

Typowy output po uruchomieniu:
```
--- idf_monitor on COM3 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T followed by Ctrl+H ---
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7176
entry 0x40080698

I (29) boot: ESP-IDF v5.4 2nd stage bootloader
I (29) boot: compile time Mar  1 2026 08:00:00
I (31) boot: Multiboot with 2 Processors running
I (35) boot: chip revision: v3.1
I (39) boot_comm: chip_revision: 301, min_revision: 0
I (44) boot.esp32: SPI Speed      : 80MHz
I (49) boot.esp32: SPI Mode       : QIO
I (53) boot.esp32: SPI Flash Size : 4MB
...
I (312) MAIN: ====================================
I (317) MAIN:   ESP32-WROVER-B — Module 0.3 Demo
I (322) MAIN: ====================================
I (327) MAIN: ESP-IDF version: v5.4
```

**Format logów ESP-IDF:**
```
I (327) MAIN: ESP-IDF version: v5.4
│  │     │     └── Treść komunikatu
│  │     └── TAG — identyfikator źródła
│  └── Czas od startu w milisekundach
└── Poziom: I=Info, W=Warning, E=Error, D=Debug, V=Verbose
```

### 2.4 Skróty klawiszowe monitora

| Skrót | Akcja | Opis |
|-------|-------|------|
| `Ctrl+]` | **Wyjście** | Zamknięcie monitora |
| `Ctrl+T` → `Ctrl+H` | **Pomoc** | Lista wszystkich skrótów |
| `Ctrl+T` → `Ctrl+R` | **Reset** | Hardware reset ESP32 (RTS pin) |
| `Ctrl+T` → `Ctrl+F` | **Download mode** | Wejście w tryb flashowania |
| `Ctrl+T` → `Ctrl+A` | **Reset + Download** | Reset i wejście w download |
| `Ctrl+T` → `Ctrl+Y` | **Timestamps** | Toggle wyświetlania timestampów |
| `Ctrl+T` → `Ctrl+P` | **Pause** | Wstrzymanie wyświetlania |
| `Ctrl+T` → `Ctrl+L` | **Log level** | Zmiana poziomu logów w runtime |
| `Ctrl+T` → `Ctrl+I` | **Print info** | Informacje o porcie i konfiguracji |

> **💡 Wskazówka:** `Ctrl+T` to prefix — naciśnij go, puść, a potem naciśnij kolejny klawisz. Nie przytrzymuj obu jednocześnie.

### 2.5 Dekodowanie Panic Backtrace

To **najważniejsza funkcja** monitora. Gdy ESP32 ulegnie krytycznemu błędowi (guru meditation error), monitor automatycznie dekoduje adresy pamięci na **nazwy funkcji i numery linii**.

**Przykład — crash bez monitora (surowe adresy):**
```
Guru Meditation Error: Core  1 panic'ed (LoadProhibited)
Core  1 register dump:
PC      : 0x400d1234  PS      : 0x00060030  A0      : 0x800d1567
A1      : 0x3ffb5e90  A2      : 0x00000000  A3      : 0x00000001

Backtrace:0x400d1234:0x3ffb5e90 0x400d1567:0x3ffb5eb0 0x400d2890:0x3ffb5ed0
```

**Ten sam crash z IDF Monitor (dekodowane):**
```
Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.
Core  1 register dump:
PC      : 0x400d1234  PS      : 0x00060030  A0      : 0x800d1567
...

Backtrace: 0x400d1234:0x3ffb5e90 0x400d1567:0x3ffb5eb0 0x400d2890:0x3ffb5ed0
  #0  0x400d1234 in read_sensor_data at /project/main/sensors.c:42
  #1  0x400d1567 in sensor_task at /project/main/sensors.c:87
  #2  0x400d2890 in vPortTaskWrapper at /esp-idf/components/freertos/port.c:131
```

Teraz od razu widzisz: **crash w `sensors.c`, linia 42, w funkcji `read_sensor_data()`** — prawdopodobnie dereferencja NULL pointera (`A2 = 0x00000000`).

### 2.6 Typowe przyczyny Guru Meditation Error

| Typ błędu | Przyczyna | Rozwiązanie |
|-----------|-----------|-------------|
| **LoadProhibited** | Odczyt z niedozwolonego adresu (NULL ptr) | Sprawdź wskaźniki, dodaj NULL check |
| **StoreProhibited** | Zapis do niedozwolonego adresu | Sprawdź wskaźniki i rozmiar buforów |
| **InstrFetchProhibited** | Skok do niepoprawnego adresu | Uszkodzony wskaźnik na funkcję |
| **IntegerDivideByZero** | Dzielenie przez zero | Dodaj walidację dzielnika |
| **Unhandled debug exception** | Breakpoint w kodzie | Usuń `__asm__("break")` |
| **Stack canary watchpoint** | Przepełnienie stosu taska | Zwiększ rozmiar stosu w `xTaskCreate()` |
| **Task watchdog timeout** | Task nie oddaje CPU | Dodaj `vTaskDelay()` lub `taskYIELD()` |
| **Interrupt watchdog timeout** | ISR trwa za długo | Skróć ISR, przenieś logikę do task |

### 2.7 Ręczne dekodowanie adresu (bez monitora)

Jeśli masz adresy z logów bez monitora (np. z innego terminala), możesz je ręcznie zdekodować:

```bash
# Dekodowanie pojedynczego adresu
xtensa-esp32-elf-addr2line -pfiaC -e build/my_project.elf 0x400d1234
# Output: read_sensor_data at /project/main/sensors.c:42

# Dekodowanie całego backtrace
xtensa-esp32-elf-addr2line -pfiaC -e build/my_project.elf \
    0x400d1234 0x400d1567 0x400d2890
```

### 2.8 Filtrowanie logów monitora

```bash
# Filtrowanie po tagu — pokaż tylko logi z tagów MAIN i SENSOR
idf.py -p COM3 monitor --filter="MAIN SENSOR"

# Filtrowanie po poziomie — pokaż tylko Warning i Error
idf.py -p COM3 monitor --filter="*:W"

# Kombinacja — MAIN: wszystko, SENSOR: tylko Error
idf.py -p COM3 monitor --filter="MAIN:V SENSOR:E"
```

Format filtra: `TAG:POZIOM` gdzie poziom to `N`=None, `E`=Error, `W`=Warn, `I`=Info, `D`=Debug, `V`=Verbose.

### 2.9 Przekierowanie logów do pliku

```bash
# Zapis logów do pliku (tee — terminal + plik)
idf.py -p COM3 monitor | tee session_log.txt

# Tylko do pliku (bez wyświetlania)
idf.py -p COM3 monitor > session_log.txt 2>&1

# Z timestampem w nazwie pliku
idf.py -p COM3 monitor | tee "log_$(date +%Y%m%d_%H%M%S).txt"
```

---

## 3. System logowania ESP-IDF

### 3.1 Makra logowania

ESP-IDF oferuje zaawansowany system logowania oparty na makrach `ESP_LOGx()`. Każde makro odpowiada innemu **poziomowi ważności**.

```c
#include "esp_log.h"

// TAG — identyfikator źródła logów (zwykle nazwa modułu/pliku)
static const char *TAG = "MY_MODULE";

void demonstrate_logging(void)
{
    // Poziomy logowania od najwyższego do najniższego priorytetu:
    ESP_LOGE(TAG, "Error:   Coś poszło bardzo źle!");           // Czerwony
    ESP_LOGW(TAG, "Warning: Potencjalny problem");               // Żółty
    ESP_LOGI(TAG, "Info:    Normalna informacja operacyjna");     // Zielony
    ESP_LOGD(TAG, "Debug:   Szczegóły do debugowania");          // Brak koloru
    ESP_LOGV(TAG, "Verbose: Bardzo szczegółowe informacje");     // Brak koloru

    // Logowanie z parametrami (format jak printf)
    int temp = 25;
    float voltage = 3.28f;
    ESP_LOGI(TAG, "Temperatura: %d°C, Napięcie: %.2fV", temp, voltage);

    // Logowanie hex dump (przydatne do debugowania protokołów)
    uint8_t data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    ESP_LOG_BUFFER_HEX(TAG, data, sizeof(data));
    // Output: 48 65 6c 6c 6f

    // Hex dump z ASCII
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, sizeof(data), ESP_LOG_INFO);
    // Output: 0x3ffb1234   48 65 6c 6c 6f                                 |Hello|

    // Logowanie bufora jako znaki
    ESP_LOG_BUFFER_CHAR(TAG, "Hello ESP32", 11);
}
```

### 3.2 Poziomy logowania — szczegóły

```
Priorytet    Poziom       Makro          Kolor     Zastosowanie
─────────────────────────────────────────────────────────────────
  (5)        VERBOSE      ESP_LOGV()     —         Śledzenie przepływu, dump danych
  (4)        DEBUG        ESP_LOGD()     —         Informacje diagnostyczne
  (3)        INFO         ESP_LOGI()     Zielony   Normalne zdarzenia operacyjne
  (2)        WARNING      ESP_LOGW()     Żółty     Sytuacje wykrywane automatycznie
  (1)        ERROR        ESP_LOGE()     Czerwony  Błędy wymagające uwagi
  (0)        NONE         —              —         Wyłączone logowanie
```

Każdy poziom **zawiera** wszystkie wyższe priorytety. Gdy ustawisz poziom na `INFO`:
- ✅ Widoczne: `ERROR`, `WARNING`, `INFO`
- ❌ Ukryte: `DEBUG`, `VERBOSE`

### 3.3 Kontrola poziomu logów — trzy mechanizmy

#### Mechanizm 1: menuconfig — poziom kompilacji (globalny)

```
Component config → Log output → Default log verbosity
├── ( ) No output          ← CONFIG_LOG_DEFAULT_LEVEL=0
├── ( ) Error              ← CONFIG_LOG_DEFAULT_LEVEL=1
├── ( ) Warning            ← CONFIG_LOG_DEFAULT_LEVEL=2
├── (X) Info               ← CONFIG_LOG_DEFAULT_LEVEL=3 (domyślne)
├── ( ) Debug              ← CONFIG_LOG_DEFAULT_LEVEL=4
└── ( ) Verbose            ← CONFIG_LOG_DEFAULT_LEVEL=5
```

Drugi ważny parametr:
```
Component config → Log output → Maximum log verbosity
```
Określa **maksymalny poziom wkompilowany** w firmware. Logi powyżej tego poziomu są **usuwane przez preprocesor** — nie zajmują Flash ani nie kosztują czasu CPU. Ustawiając na `Info` w produkcji oszczędzasz pamięć i zwiększasz wydajność.

#### Mechanizm 2: esp_log_level_set() — zmiana w runtime (per tag)

```c
#include "esp_log.h"

void configure_log_levels(void)
{
    // Ustaw globalny poziom na WARNING
    esp_log_level_set("*", ESP_LOG_WARN);

    // Ale dla modułu SENSOR — pełny verbose (do debugowania)
    esp_log_level_set("SENSOR", ESP_LOG_VERBOSE);

    // Dla modułu WIFI — tylko błędy
    esp_log_level_set("wifi", ESP_LOG_ERROR);

    // Wyłącz logi z konkretnego modułu
    esp_log_level_set("NOISY_MODULE", ESP_LOG_NONE);
}
```

> **⚠️ Ważne:** `esp_log_level_set()` może **tylko obniżyć** poziom poniżej wartości ustawionej w menuconfig jako "Maximum log verbosity". Jeśli maksimum to `INFO`, nie możesz włączyć `DEBUG` przez runtime API — te logi nie istnieją w firmware.

#### Mechanizm 3: Kompilacyjne per plik (zaawansowane)

```c
// Na początku pliku, PRZED #include "esp_log.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE   // Ten plik: verbose
#include "esp_log.h"
```

### 3.4 Kolorowanie logów

ESP-IDF domyślnie koloruje logi w terminalu ANSI:

```c
// Output w terminalu (z kolorami ANSI):
// \033[0;31m E (1234) TAG: Error message \033[0m     ← Czerwony
// \033[0;33m W (1234) TAG: Warning message \033[0m   ← Żółty
// \033[0;32m I (1234) TAG: Info message \033[0m       ← Zielony
//           D (1234) TAG: Debug message               ← Domyślny
//           V (1234) TAG: Verbose message              ← Domyślny
```

Wyłączenie kolorów (np. dla zapisu do pliku):
```
Component config → Log output → Use ANSI terminal colors: [ ]
```

### 3.5 Wydajność systemu logowania

```
Operacja                          Czas (µs)    Cyklów CPU
──────────────────────────────────────────────────────────
ESP_LOGI() — krótki tekst         ~20-50       ~5000-12000
ESP_LOGI() — długi tekst+format   ~50-150      ~12000-36000
ESP_LOG_BUFFER_HEX() — 64 bajty   ~100-200     ~24000-48000
Wyłączony log (powyżej poziomu)   ~0           0 (preprocesor!)
```

> **💡 Wskazówka:** W ISR (Interrupt Service Routine) **NIGDY nie używaj ESP_LOGx()**! Funkcje logujące używają mutex i UART — niedozwolone w ISR. Użyj `ESP_DRAM_LOGE()` w sytuacjach awaryjnych lub sygnalizuj task przez kolejkę.

### 3.6 Logowanie bezpieczne dla ISR

```c
#include "esp_log.h"

// W ISR — NIE RÓB TEGO:
void IRAM_ATTR bad_isr_handler(void *arg)
{
    // ❌ ZABRONIONE W ISR!
    // ESP_LOGI(TAG, "Przerwanie!");  // → crash lub deadlock
}

// Poprawne podejście — sygnalizacja do task
static QueueHandle_t isr_queue;

void IRAM_ATTR good_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(isr_queue, &gpio_num, NULL);
}

void isr_log_task(void *pvParam)
{
    uint32_t gpio_num;
    while (1) {
        if (xQueueReceive(isr_queue, &gpio_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Przerwanie na GPIO %lu", (unsigned long)gpio_num);
        }
    }
}

// W ekstremalnych sytuacjach awaryjnych (np. w panic handler):
void IRAM_ATTR emergency_log(void)
{
    // ESP_DRAM_LOGE — działa z IRAM, pomija mutex, ale TYLKO do UART
    ESP_DRAM_LOGE("ISR", "Krytyczny błąd w ISR!");
}
```

### 3.7 Własny handler logów (zaawansowane)

Domyślnie logi idą na UART0. Możesz je przekierować:

```c
#include "esp_log.h"

// Własna funkcja obsługi logów
static int my_log_vprintf(const char *fmt, va_list args)
{
    // 1. Wyślij na UART (domyślne zachowanie)
    int ret = vprintf(fmt, args);

    // 2. Dodatkowo: zapisz do bufora w PSRAM
    // ... lub wyślij przez WiFi, zapisz na SD card, etc.

    return ret;
}

void setup_custom_logging(void)
{
    // Podmiana handlera logów
    esp_log_set_vprintf(my_log_vprintf);
}
```

### 3.8 Dobre praktyki logowania

```c
// ✅ DOBRZE: opisowe TAG-i
static const char *TAG = "SENSOR_BME280";
static const char *TAG = "WIFI_MANAGER";
static const char *TAG = "DISPLAY_SPI";

// ❌ ŹLE: generyczne TAG-i
static const char *TAG = "main";
static const char *TAG = "app";
static const char *TAG = "test";

// ✅ DOBRZE: loguj kontekst
ESP_LOGE(TAG, "I2C read failed: addr=0x%02X, reg=0x%02X, err=%s",
         dev_addr, reg_addr, esp_err_to_name(ret));

// ❌ ŹLE: bezużyteczne logi
ESP_LOGE(TAG, "Error!");

// ✅ DOBRZE: warunkowe logowanie kosztownych operacji
if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG) {
    // Zbierz dane diagnostyczne tylko gdy DEBUG jest aktywne
    char *diag = gather_diagnostics();
    ESP_LOGD(TAG, "Diagnostics: %s", diag);
    free(diag);
}

// ✅ DOBRZE: esp_err_to_name() do kodów błędów
esp_err_t ret = some_function();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Funkcja zwróciła błąd: %s (0x%X)",
             esp_err_to_name(ret), ret);
}
```

---

## 4. JTAG Debugging (opcjonalnie)

### 4.1 Czym jest JTAG?

**JTAG** (Joint Test Action Group) to interfejs sprzętowego debugowania, który pozwala na:
- **Breakpoints** — zatrzymanie CPU na dowolnej linii kodu
- **Step-by-step** — wykonywanie kodu linia po linii
- **Inspekcja pamięci** — podgląd zmiennych, rejestrów, stosu w czasie rzeczywistym
- **Watchpointy** — zatrzymanie gdy zmienna zmieni wartość
- **Flash programming** — programowanie bez użycia UART

### 4.2 Adaptery JTAG dla ESP32

| Adapter | Cena | Prędkość | Zalety |
|---------|------|----------|--------|
| **ESP-PROG** | ~$10 | Dobra | Oficjalny Espressif, plug-and-play |
| **FT2232H** | ~$15 | Dobra | Popularny, dobrze wspierany |
| **J-Link EDU** | ~$20 | Bardzo dobra | Profesjonalny, świetne oprogramowanie |
| **ESP32-S3 USB** | Wbudowany | Dobra | USB JTAG wbudowany (tylko S3!) |

> **⚠️ Uwaga:** ESP32 (bez S3) **nie ma wbudowanego USB JTAG**. Potrzebujesz zewnętrznego adaptera. Jeśli go nie masz, możesz użyć **GDB Stub** przez UART (wolniejsze, ale nie wymaga adaptera).

### 4.3 Pinout JTAG na ESP32

| Pin JTAG | GPIO ESP32 | Opis |
|----------|-----------|------|
| **TDI** | GPIO 12 | Test Data In |
| **TDO** | GPIO 15 | Test Data Out |
| **TCK** | GPIO 13 | Test Clock |
| **TMS** | GPIO 14 | Test Mode Select |

> **⚠️ Ważne:** GPIO 12 to **strapping pin** (VDD_SDIO)! Podłączenie JTAG może wpłynąć na boot. Rozwiązanie: użyj `espefuse.py` do ustawienia odpowiedniego napięcia na stałe, lub skonfiguruj `menuconfig` → `Component config → ESP32-specific → GPIO 12 default behavior`.

### 4.4 Konfiguracja OpenOCD + GDB (z adapterem)

**Instalacja OpenOCD** (wbudowane w ESP-IDF tools):
```bash
# OpenOCD jest instalowany automatycznie z ESP-IDF
# Sprawdź wersję:
openocd --version
```

**Uruchomienie sesji debugowania:**
```bash
# Terminal 1: Uruchom OpenOCD (serwer JTAG)
openocd -f board/esp32-wrover-kit-3.3v.cfg

# Terminal 2: Uruchom GDB
xtensa-esp32-elf-gdb -x gdbinit build/my_project.elf

# Lub z VS Code — automatyczna konfiguracja:
# Ctrl+Shift+P → "ESP-IDF: Launch OpenOCD"
# Potem F5 (Start Debugging)
```

**launch.json (VS Code):**
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "type": "espidf",
            "name": "ESP-IDF Debug",
            "request": "launch",
            "debugPort": 3333,
            "logLevel": 2,
            "mode": "auto",
            "verifyAppBinBeforeDebug": false,
            "tmoScaleFactor": 5
        }
    ]
}
```

### 4.5 GDB Stub — debugowanie przez UART (bez adaptera JTAG)

Jeśli **nie masz adaptera JTAG**, ESP-IDF oferuje **GDB Stub** — debugger działający przez UART. Jest wolniejszy niż JTAG, ale nie wymaga dodatkowego sprzętu.

**Konfiguracja w menuconfig:**
```
Component config → ESP System Settings → Panic handler behaviour
├── ( ) Print registers and reboot
├── (X) Invoke GDB Stub                ← Wybierz tę opcję
└── ( ) Print registers and halt
```

**Działanie:** Gdy ESP32 ulegnie crash, zamiast resetować się, uruchomi GDB Stub i poczeka na połączenie GDB przez UART. Możesz wtedy zbadać stan pamięci, zmiennych i stosu.

```bash
# Gdy ESP32 wejdzie w GDB Stub, IDF Monitor zapyta:
# --- GDB Stub detected. Connect with:
# xtensa-esp32-elf-gdb -x gdbinit build/project.elf

# W VS Code: automatyczna integracja — wystarczy kliknąć
```

### 4.6 Inne metody debugowania (bez JTAG)

| Metoda | Opis | Wady |
|--------|------|------|
| **ESP_LOGx()** | Logowanie na UART | Wpływa na timing, brak breakpoints |
| **GDB Stub** | GDB przez UART po crash | Tylko po crash, wolny |
| **Core Dump** | Zapis stanu CPU do Flash/UART | Post-mortem, nie real-time |
| **SystemView** | SEGGER SystemView przez JTAG/UART | Wymaga JTAG dla pełnych funkcji |
| **App Trace** | Szybki logging przez JTAG | Wymaga adaptera JTAG |

**Core Dump — konfiguracja:**
```
Component config → ESP System Settings → Panic handler behaviour
→ Core dump
├── Core dump destination: Flash    ← Zapis do partycji Flash
├── Core dump data format: ELF
└── Maximum number of tasks: 64
```

Odczyt core dump po restart:
```bash
# Odczyt core dump z Flash
idf.py coredump-info
idf.py coredump-debug    # Otwiera interaktywny GDB z core dump
```

---

## 5. Projekt przykładowy — wszystkie narzędzia w akcji

### 5.1 Scenariusz

Projekt demonstrujący:
1. Własne opcje `menuconfig` (Kconfig)
2. Wielopoziomowe logowanie
3. Symulowany crash z dekodowaniem backtrace
4. Runtime zmiana poziomu logów

### 5.2 Struktura projektu

```
faza_00_modul_03_narzedzia/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   └── main.c
```

### 5.3 Pliki projektu

**CMakeLists.txt** (główny):
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(narzedzia_demo)
```

**main/CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
)
```

**main/Kconfig.projbuild:**
```kconfig
menu "Narzędzia Demo Configuration"
    config DEMO_LED_GPIO
        int "LED GPIO number"
        default 2
        range 0 39
        help
            GPIO number for the demo LED.

    config DEMO_ENABLE_CRASH_TEST
        bool "Enable crash test (after 10 seconds)"
        default n
        help
            If enabled, the app will intentionally crash after 10 seconds
            to demonstrate panic backtrace decoding.

    config DEMO_LOG_INTERVAL_MS
        int "Log interval (ms)"
        default 2000
        range 500 30000
        help
            Interval between log messages in milliseconds.
endmenu
```

**sdkconfig.defaults:**
```ini
CONFIG_IDF_TARGET="esp32"
CONFIG_SPIRAM=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_MAXIMUM_LEVEL_VERBOSE=y
```

**main/main.c:**
```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

// === TAG-i dla różnych modułów ===
static const char *TAG_MAIN   = "MAIN";
static const char *TAG_SENSOR = "SENSOR";
static const char *TAG_SYSTEM = "SYSTEM";

// === Symulacja odczytu czujnika ===
typedef struct {
    float temperature;
    float humidity;
    int   reading_count;
} sensor_data_t;

static sensor_data_t read_sensor(void)
{
    static int count = 0;
    sensor_data_t data = {
        .temperature = 22.5f + (float)(count % 10) * 0.3f,
        .humidity    = 45.0f + (float)(count % 20) * 0.5f,
        .reading_count = ++count
    };

    ESP_LOGD(TAG_SENSOR, "Raw sensor read #%d: temp=%.1f, hum=%.1f",
             data.reading_count, data.temperature, data.humidity);

    return data;
}

// === Wyświetlanie informacji systemowych ===
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG_SYSTEM, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG_SYSTEM, "║    ESP32 — Narzędzia Programisty    ║");
    ESP_LOGI(TAG_SYSTEM, "╚══════════════════════════════════════╝");
    ESP_LOGI(TAG_SYSTEM, "ESP-IDF: %s", esp_get_idf_version());
    ESP_LOGI(TAG_SYSTEM, "Rdzenie: %d, Rewizja: %d", chip_info.cores, chip_info.revision);
    ESP_LOGI(TAG_SYSTEM, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG_SYSTEM, "PSRAM free: %lu bytes",
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG_SYSTEM, "LED GPIO: %d (z menuconfig)", CONFIG_DEMO_LED_GPIO);
    ESP_LOGI(TAG_SYSTEM, "Log interval: %d ms", CONFIG_DEMO_LOG_INTERVAL_MS);

    #if CONFIG_DEMO_ENABLE_CRASH_TEST
    ESP_LOGW(TAG_SYSTEM, "⚠️  Crash test WŁĄCZONY — crash za ~10 sekund");
    #endif
}

// === Demonstracja poziomów logowania ===
static void demonstrate_log_levels(void)
{
    ESP_LOGE(TAG_MAIN, "To jest ERROR   — krytyczny błąd (zawsze widoczny)");
    ESP_LOGW(TAG_MAIN, "To jest WARNING — ostrzeżenie");
    ESP_LOGI(TAG_MAIN, "To jest INFO    — normalna informacja");
    ESP_LOGD(TAG_MAIN, "To jest DEBUG   — szczegóły diagnostyczne");
    ESP_LOGV(TAG_MAIN, "To jest VERBOSE — najdrobniejsze detale");

    // Demonstracja hex dump
    uint8_t packet[] = {0x02, 0x10, 0x00, 0x05, 'H', 'e', 'l', 'l', 'o', 0x03};
    ESP_LOGI(TAG_MAIN, "Przykładowy pakiet danych:");
    ESP_LOG_BUFFER_HEXDUMP(TAG_MAIN, packet, sizeof(packet), ESP_LOG_INFO);
}

// === Demonstracja runtime zmiany logów ===
static void configure_runtime_log_levels(void)
{
    ESP_LOGI(TAG_MAIN, "--- Zmiana poziomów logów w runtime ---");

    // Włącz DEBUG dla SENSOR
    esp_log_level_set(TAG_SENSOR, ESP_LOG_DEBUG);
    ESP_LOGI(TAG_MAIN, "SENSOR log level → DEBUG");

    // Wycisz logi WiFi (gdyby był aktywny)
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    ESP_LOGI(TAG_MAIN, "wifi log level → ERROR only");
}

#if CONFIG_DEMO_ENABLE_CRASH_TEST
// === Celowy crash (demonstracja backtrace) ===
static void inner_function(int *ptr)
{
    // Celowa dereferencja NULL — wywoła LoadProhibited
    ESP_LOGW(TAG_MAIN, "Próba odczytu z NULL pointera...");
    int value = *ptr;   // ← CRASH TUTAJ — linia ~108
    ESP_LOGI(TAG_MAIN, "Wartość: %d", value);  // Nigdy nie zostanie wykonane
}

static void outer_function(void)
{
    ESP_LOGW(TAG_MAIN, "Wywołanie inner_function(NULL)...");
    inner_function(NULL);   // Przekaż NULL pointer
}

static void crash_test_task(void *pvParam)
{
    ESP_LOGW(TAG_MAIN, "Crash test task uruchomiony — crash za 10 sekund");
    vTaskDelay(pdMS_TO_TICKS(10000));

    ESP_LOGE(TAG_MAIN, "=== ROZPOCZYNAM CRASH TEST ===");
    ESP_LOGE(TAG_MAIN, "Poniżej zobaczysz Guru Meditation Error");
    ESP_LOGE(TAG_MAIN, "IDF Monitor zdekoduje backtrace automatycznie!");
    outer_function();  // → inner_function(NULL) → CRASH
}
#endif

// === Główna pętla aplikacji ===
void app_main(void)
{
    // 1. Informacje systemowe
    print_system_info();

    // 2. Demonstracja logów
    demonstrate_log_levels();

    // 3. Konfiguracja runtime logów
    configure_runtime_log_levels();

    // 4. Konfiguracja LED
    gpio_reset_pin(CONFIG_DEMO_LED_GPIO);
    gpio_set_direction(CONFIG_DEMO_LED_GPIO, GPIO_MODE_OUTPUT);

    // 5. Opcjonalny crash test
    #if CONFIG_DEMO_ENABLE_CRASH_TEST
    xTaskCreate(crash_test_task, "crash_test", 4096, NULL, 5, NULL);
    #endif

    // 6. Główna pętla — odczyt czujnika + LED blink
    int iteration = 0;
    while (1) {
        // Odczyt symulowanego czujnika
        sensor_data_t data = read_sensor();

        // Log co CONFIG_DEMO_LOG_INTERVAL_MS
        ESP_LOGI(TAG_SENSOR, "[%d] Temp: %.1f°C, Wilgotność: %.1f%%",
                 data.reading_count, data.temperature, data.humidity);

        // Ostrzeżenie przy "wysokiej temperaturze"
        if (data.temperature > 25.0f) {
            ESP_LOGW(TAG_SENSOR, "Temperatura powyżej progu: %.1f°C > 25°C",
                     data.temperature);
        }

        // Blink LED
        gpio_set_level(CONFIG_DEMO_LED_GPIO, iteration % 2);

        // Periodycznie loguj stan pamięci
        if (iteration % 10 == 0 && iteration > 0) {
            ESP_LOGI(TAG_SYSTEM, "Heap free: %lu, min ever: %lu",
                     (unsigned long)esp_get_free_heap_size(),
                     (unsigned long)esp_get_minimum_free_heap_size());
        }

        iteration++;
        vTaskDelay(pdMS_TO_TICKS(CONFIG_DEMO_LOG_INTERVAL_MS));
    }
}
```

### 5.4 Build, Flash, Monitor

```bash
# 1. Ustaw target
idf.py set-target esp32

# 2. Skonfiguruj menuconfig (włącz crash test itp.)
idf.py menuconfig

# 3. Build + Flash + Monitor
idf.py -p COM3 flash monitor
```

### 5.5 Oczekiwany output (bez crash test)

```
I (312) SYSTEM: ╔══════════════════════════════════════╗
I (317) SYSTEM: ║    ESP32 — Narzędzia Programisty    ║
I (322) SYSTEM: ╚══════════════════════════════════════╝
I (327) SYSTEM: ESP-IDF: v5.4
I (332) SYSTEM: Rdzenie: 2, Rewizja: 301
I (337) SYSTEM: Free heap: 4388576 bytes
I (342) SYSTEM: PSRAM free: 8388352 bytes
I (347) SYSTEM: LED GPIO: 2 (z menuconfig)
I (352) SYSTEM: Log interval: 2000 ms
E (357) MAIN: To jest ERROR   — krytyczny błąd (zawsze widoczny)
W (362) MAIN: To jest WARNING — ostrzeżenie
I (367) MAIN: To jest INFO    — normalna informacja
I (372) MAIN: Przykładowy pakiet danych:
I (377) MAIN: 0x3ffb1230   02 10 00 05 48 65 6c 6c  6f 03       |....Hello.|
I (382) MAIN: --- Zmiana poziomów logów w runtime ---
I (387) MAIN: SENSOR log level → DEBUG
I (392) MAIN: wifi log level → ERROR only
D (397) SENSOR: Raw sensor read #1: temp=22.5, hum=45.0
I (402) SENSOR: [1] Temp: 22.5°C, Wilgotność: 45.0%
D (2402) SENSOR: Raw sensor read #2: temp=22.8, hum=45.5
I (2407) SENSOR: [2] Temp: 22.8°C, Wilgotność: 45.5%
```

### 5.6 Output z crash test

```
W (10357) MAIN: Crash test task uruchomiony — crash za 10 sekund
E (20357) MAIN: === ROZPOCZYNAM CRASH TEST ===
E (20362) MAIN: Poniżej zobaczysz Guru Meditation Error
E (20367) MAIN: IDF Monitor zdekoduje backtrace automatycznie!
W (20372) MAIN: Wywołanie inner_function(NULL)...
W (20377) MAIN: Próba odczytu z NULL pointera...

Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x400d1a8c  PS      : 0x00060030  A0      : 0x800d1ac4
A1      : 0x3ffb5e70  A2      : 0x00000000  ...

Backtrace: 0x400d1a8c:0x3ffb5e70 0x400d1ac4:0x3ffb5e90 0x400d1b10:0x3ffb5eb0
  #0  0x400d1a8c in inner_function at main/main.c:108
  #1  0x400d1ac4 in outer_function at main/main.c:114
  #2  0x400d1b10 in crash_test_task at main/main.c:121
```

---

## 6. Podsumowanie i dalsze kroki

### 6.1 Podsumowanie narzędzi

| Narzędzie | Zastosowanie | Kiedy używać |
|-----------|-------------|--------------|
| `idf.py menuconfig` | Konfiguracja projektu | Na początku projektu, przy zmianach konfiguracji |
| `idf.py monitor` | UART monitoring + dekodowanie | **Zawsze** podczas developmentu |
| `ESP_LOGx()` | Logowanie w kodzie | Wszędzie — główna metoda debugowania |
| JTAG / GDB Stub | Debugowanie z breakpoints | Trudne do zlokalizowania bugi |
| Core Dump | Analiza post-mortem | Crashe w produkcji |

### 6.2 Dalsze kroki

Po opanowaniu narzędzi z tego modułu przejdź do:
- **Faza 1, Moduł 1.1:** GPIO & RTC GPIO — pierwsze sterowanie sprzętem
- Podczas pracy z modułami 1.x, ćwicz używanie `ESP_LOGx()` do debugowania i `menuconfig` do konfiguracji peryferiów

---

## 7. Źródła i dokumentacja

| Zasób | Link |
|-------|------|
| **ESP-IDF Build System** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/build-system.html |
| **Project Configuration (Kconfig)** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/kconfig.html |
| **IDF Monitor** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/tools/idf-monitor.html |
| **Logging Library** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/log.html |
| **JTAG Debugging** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/jtag-debugging/index.html |
| **Fatal Errors (Panic)** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/fatal-errors.html |
| **Core Dump** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/core_dump.html |
| **GDB Stub** | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/tools/idf-monitor.html#gdb-stub |
| **ESP32 Technical Reference** | `esp32_technical_reference_manual_en.pdf` (w workspace) |
| **ESP32-WROVER-B Datasheet** | `esp32-wrover-b_datasheet_en.pdf` (w workspace) |

---

> *Moduł 0.3 — Podstawowe narzędzia programisty. Ostatnia aktualizacja: marzec 2026.*
