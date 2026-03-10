# Moduł 0.1 — Instalacja i konfiguracja ESP-IDF

> **Poziom:** 🟢 Laik · **Czas:** Tydzień 1 (pierwsza połowa Fazy 0)  
> **Cel:** Przygotowanie kompletnego środowiska deweloperskiego, zrozumienie struktury projektu ESP-IDF, pierwsze kompilacje i flashowanie firmware na ESP32-WROVER-B.

---

## Spis treści

1. [Wymagania wstępne](#1-wymagania-wstępne)
2. [Instalacja ESP-IDF v5.x](#2-instalacja-esp-idf-v5x)
3. [Instalacja rozszerzenia ESP-IDF do VS Code](#3-instalacja-rozszerzenia-esp-idf-do-vs-code)
4. [Toolchain Xtensa, CMake, Ninja — co to jest i jak działa](#4-toolchain-xtensa-cmake-ninja--co-to-jest-i-jak-działa)
5. [Struktura projektu ESP-IDF](#5-struktura-projektu-esp-idf)
6. [Pierwsze uruchomienie — build, flash, monitor](#6-pierwsze-uruchomienie--build-flash-monitor)
7. [Konfiguracja menuconfig — target ESP32 i PSRAM](#7-konfiguracja-menuconfig--target-esp32-i-psram)
8. [Najczęstsze problemy i rozwiązania](#8-najczęstsze-problemy-i-rozwiązania)
9. [Podsumowanie i dalsze kroki](#9-podsumowanie-i-dalsze-kroki)
10. [Źródła i dokumentacja](#10-źródła-i-dokumentacja)

---

## 1. Wymagania wstępne

### Sprzęt

| Element | Opis |
|---------|------|
| **ESP32-WROVER-B (NodeMCU-32)** | Płytka deweloperska z wbudowanym USB-UART (CP2102 lub CH340) |
| **Kabel USB Micro-B** | Do połączenia płytki z komputerem |
| **Komputer** | Windows 10/11, Linux (Ubuntu 20.04+) lub macOS |

### Oprogramowanie

| Narzędzie | Wersja | Opis |
|-----------|--------|------|
| **Visual Studio Code** | Najnowsza | Edytor kodu z obsługą rozszerzeń |
| **Python** | 3.8 – 3.12 | Wymagany przez ESP-IDF (skrypty budowania) |
| **Git** | 2.x+ | Klonowanie repozytorium ESP-IDF |
| **Sterownik USB** | CP2102 lub CH340 | Zależnie od konwertera USB-UART na płytce |

> **⚠️ Ważne:** Przed instalacją ESP-IDF upewnij się, że masz zainstalowany Python i Git. Na Windows obie aplikacje powinny być dodane do zmiennej `PATH`.

### Sterowniki USB-UART

NodeMCU-32 z ESP32-WROVER-B najczęściej używa konwertera **CP2102** (Silicon Labs) lub **CH340** (WCH).

- **CP2102:** Sterownik pobierz ze strony Silicon Labs: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- **CH340:** Sterownik pobierz ze strony WCH: http://www.wch-ic.com/downloads/CH341SER_ZIP.html

Po instalacji sterownika i podłączeniu płytki, w **Menedżerze urządzeń** (Windows) powinien pojawić się port COM, np.: `COM3 (Silicon Labs CP210x USB to UART Bridge)`.

---

## 2. Instalacja ESP-IDF v5.x

ESP-IDF (Espressif IoT Development Framework) to oficjalny framework od Espressif do programowania mikrokontrolerów ESP32. Zawiera:
- Kompilator (toolchain Xtensa)
- System budowania (CMake + Ninja)
- Biblioteki sterowników i middleware
- FreeRTOS
- Narzędzia: `idf.py`, `menuconfig`, `esptool.py`

### 2.1 Metoda 1: Instalator Windows (zalecana dla początkujących)

1. Pobierz instalator z **oficjalnej strony:**
   - https://dl.espressif.com/dl/esp-idf/
   - Wybierz najnowszą stabilną wersję **v5.x** (np. `ESP-IDF v5.4`)

2. Uruchom instalator i przejdź przez kreator:
   ```
   [x] ESP-IDF v5.4
   [x] ESP-IDF Tools (Python, Git, CMake, Ninja, toolchain)
   
   Ścieżka instalacji: C:\Espressif
   ```

3. Po instalacji na pulpicie pojawią się skróty:
   - **ESP-IDF 5.4 CMD** — terminal z poprawnie skonfigurowanym środowiskiem
   - **ESP-IDF 5.4 PowerShell** — alternatywna wersja

4. Zweryfikuj instalację — otwórz **ESP-IDF CMD** i wpisz:
   ```bash
   idf.py --version
   # Oczekiwany wynik: ESP-IDF v5.4-xxxxx
   ```

### 2.2 Metoda 2: Instalacja ręczna (zaawansowana)

> **Uwaga:** Ta metoda daje pełną kontrolę nad konfiguracją, ale wymaga więcej kroków.

1. **Klonuj repozytorium ESP-IDF:**
   ```bash
   mkdir -p ~/esp
   cd ~/esp
   git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git
   ```

2. **Zainstaluj narzędzia:**
   ```bash
   cd esp-idf
   
   # Windows (PowerShell):
   .\install.ps1 esp32
   
   # Linux/macOS:
   ./install.sh esp32
   ```

3. **Eksportuj zmienne środowiskowe:**
   ```bash
   # Windows (PowerShell):
   .\export.ps1
   
   # Linux/macOS:
   . ./export.sh
   ```
   > Ten krok musisz powtarzać w **każdym nowym terminalu**, chyba że dodasz go do profilu powłoki.

4. **Weryfikacja:**
   ```bash
   idf.py --version
   python -c "import idf_component_manager; print('OK')"
   ```

### 2.3 Struktura instalacji ESP-IDF

Po instalacji na dysku znajdziesz:

```
C:\Espressif\                        (lub ~/esp/)
├── frameworks/
│   └── esp-idf-v5.4/               ← Kod źródłowy ESP-IDF
│       ├── components/              ← Sterowniki, biblioteki (FreeRTOS, lwIP, mbedTLS...)
│       ├── examples/                ← Setki przykładowych projektów!
│       ├── tools/                   ← Skrypty budowania, idf.py
│       └── Kconfig                  ← Definicje menuconfig
├── tools/
│   ├── xtensa-esp32-elf/            ← Toolchain (kompilator GCC Xtensa)
│   ├── cmake/                       ← CMake
│   ├── ninja/                       ← Ninja build system
│   └── python_env/                  ← Wirtualne środowisko Pythona
└── python_env/                      ← Virtualenv z pakietami ESP-IDF
```

> **💡 Wskazówka:** Katalog `examples/` to kopalnia wiedzy! Każdy peryferium ma tam działające przykłady. Warto je przeglądać przy nauce każdego modułu.

---

## 3. Instalacja rozszerzenia ESP-IDF do VS Code

Rozszerzenie **ESP-IDF** dla VS Code zapewnia wygodną integrację: budowanie, flashowanie, monitor, menuconfig — wszystko z poziomu IDE.

### 3.1 Instalacja rozszerzenia

1. Otwórz VS Code
2. Przejdź do zakładki **Extensions** (`Ctrl+Shift+X`)
3. Wyszukaj: `ESP-IDF`
4. Zainstaluj rozszerzenie **"Espressif IDF"** (autor: **Espressif Systems**)

### 3.2 Konfiguracja rozszerzenia

Po instalacji rozszerzenie uruchomi kreator konfiguracji:

1. **Naciśnij `Ctrl+Shift+P`** → wpisz: `ESP-IDF: Configure ESP-IDF Extension`
2. Wybierz tryb konfiguracji:
   - **EXPRESS** — automatyczna konfiguracja (zalecana)
   - **ADVANCED** — ręczny wybór ścieżek
3. W trybie EXPRESS:
   - **ESP-IDF version:** wybierz `v5.4` (lub najnowszą stabilną)
   - **Python path:** rozszerzenie wykryje automatycznie
   - **ESP-IDF path:** wskaż na `C:\Espressif\frameworks\esp-idf-v5.4` (lub ścieżkę z ręcznej instalacji)
   - **Tools path:** `C:\Espressif\tools`
4. Kliknij **Install** i poczekaj na pobranie narzędzi (może to zająć kilka minut)

### 3.3 Interfejs rozszerzenia w VS Code

Po konfiguracji w dolnym pasku VS Code pojawią się ikony:

```
┌──────────────────────────────────────────────────────────────────────┐
│ 🔧 ESP32  │  COM3  │  ⚡ Build  │  📤 Flash  │  🖥️ Monitor  │  🔥  │
│  (target)  │ (port) │           │            │              │(full)│
└──────────────────────────────────────────────────────────────────────┘
```

| Ikona | Funkcja | Skrót klawiszowy |
|-------|---------|-----------------|
| 🔧 Set Target | Wybór chipa (ESP32, ESP32-S2, S3...) | `Ctrl+Shift+P` → `Set Target` |
| COM port | Wybór portu szeregowego | kliknij na COM |
| ⚡ Build | Kompilacja projektu (`idf.py build`) | `Ctrl+E` → `B` |
| 📤 Flash | Wgrywanie firmware | `Ctrl+E` → `F` |
| 🖥️ Monitor | UART monitor | `Ctrl+E` → `M` |
| 🔥 Build+Flash+Monitor | Wszystko naraz | `Ctrl+E` → `D` |
| ⚙️ Menuconfig | Konfiguracja graficzna | `Ctrl+E` → `G` |

### 3.4 Tworzenie nowego projektu z rozszerzenia

1. `Ctrl+Shift+P` → `ESP-IDF: New Project`
2. Uzupełnij:
   - **Project Name:** np. `faza_00_modul_01_hello`
   - **Project Directory:** wybierz katalog roboczy
   - **Board:** `ESP32 chip (via ESP-WROVER-KIT)` lub `Custom Board`
   - **Template:** `template-app` (minimalny projekt) lub `get-started/hello_world`
3. Kliknij **Create Project**

Alternatywnie możesz użyć przykładu:
- `Ctrl+Shift+P` → `ESP-IDF: Show Examples Projects`
- Wybierz np. `get-started/hello_world`
- Kliknij **Create project using example hello_world**

---

## 4. Toolchain Xtensa, CMake, Ninja — co to jest i jak działa

### 4.1 Toolchain Xtensa

**Toolchain** to zestaw narzędzi do kompilacji kodu C/C++ na kod maszynowy procesora docelowego.

ESP32 używa procesora **Xtensa LX6** — jest to architektura inna niż x86 (PC) czy ARM. Dlatego potrzebujemy **cross-compilera** — kompilatora działającego na PC, ale generującego kod dla Xtensa.

```
Twój kod C/C++
      │
      ▼
┌─────────────────────┐
│ xtensa-esp32-elf-gcc │  ← Cross-compiler (GCC dla Xtensa)
└─────────────────────┘
      │
      ▼
Plik binarny .elf / .bin
      │
      ▼
┌─────────────────────┐
│   esptool.py flash   │  ← Narzędzie do flashowania
└─────────────────────┘
      │
      ▼
   ESP32 Flash
```

Skład toolchaina Xtensa:

| Narzędzie | Opis |
|-----------|------|
| `xtensa-esp32-elf-gcc` | Kompilator C |
| `xtensa-esp32-elf-g++` | Kompilator C++ |
| `xtensa-esp32-elf-ld` | Linker — łączy pliki obiektowe |
| `xtensa-esp32-elf-objdump` | Analiza plików binarnych |
| `xtensa-esp32-elf-size` | Rozmiar sekcji (text, data, bss) |
| `xtensa-esp32-elf-gdb` | Debugger (GDB) |

### 4.2 CMake — system budowania

**CMake** to meta-system budowania — nie kompiluje kodu sam, ale generuje pliki konfiguracyjne dla innego narzędzia (Ninja, Make itp.).

W ESP-IDF CMake odpowiada za:
- Rekurencyjne zbieranie plików źródłowych z komponentów
- Zarządzanie zależnościami między komponentami
- Konfigurację opcji kompilacji (flagi, definy)
- Generowanie skryptów linkera

**Przepływ budowania:**

```
CMakeLists.txt (projekt)
       │
       ▼
┌──────────┐      ┌──────────────┐
│  CMake   │ ───► │ build.ninja  │  ← Wygenerowane reguły budowania
└──────────┘      └──────────────┘
                         │
                         ▼
                  ┌──────────┐
                  │  Ninja   │  ← Wykonanie kompilacji
                  └──────────┘
                         │
                         ▼
                   projekt.bin
```

### 4.3 Ninja — szybki build system

**Ninja** to ultra-szybki system budowania zaprojektowany jako backend dla CMake. W porównaniu z tradycyjnym `make`:

| Cecha | Make | Ninja |
|-------|------|-------|
| Prędkość | Wolniejszy (parsowanie Makefile) | ~10x szybszy |
| Inkrementalny build | Tak | Tak (bardziej precyzyjny) |
| Paralelizm | `-j N` | Automatyczny (`-j` domyślnie) |
| Plik konfiguracji | `Makefile` | `build.ninja` |

> **Dlaczego to ważne?** ESP-IDF to ogromny framework — pełna kompilacja to setki plików. Ninja znacząco przyspiesza rebuildy — po zmianie jednego pliku rekompiluje tylko to, co konieczne (kilka sekund zamiast minut).

### 4.4 Podsumowanie przepływu

```
                    idf.py build
                         │
        ┌────────────────┼────────────────┐
        ▼                ▼                ▼
   1. CMake          2. Ninja         3. esptool
   (konfiguracja)    (kompilacja)     (tworzenie .bin)
        │                │                │
   Generuje          Wywołuje          Scala sekcje
   build.ninja       xtensa-gcc       bootloader +
                     na każdym        partition table +
                     pliku .c/.cpp    app → firmware.bin
```

---

## 5. Struktura projektu ESP-IDF

### 5.1 Minimalny projekt

Każdy projekt ESP-IDF wymaga minimum **3 elementów**:

```
my_project/
├── CMakeLists.txt          ← Główny plik CMake (definicja projektu)
├── main/
│   ├── CMakeLists.txt      ← CMake dla komponentu main
│   └── main.c              ← Punkt wejścia programu (app_main)
└── sdkconfig               ← Konfiguracja projektu (generowana przez menuconfig)
```

### 5.2 Główny CMakeLists.txt (katalog główny projektu)

```cmake
# Minimalny CMakeLists.txt projektu ESP-IDF
cmake_minimum_required(VERSION 3.16)

# Dołączenie systemu budowania ESP-IDF
# Zmienna IDF_PATH musi wskazywać na katalog ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nazwa projektu
project(hello_world)
```

**Wyjaśnienie:**
- `cmake_minimum_required(VERSION 3.16)` — ESP-IDF v5.x wymaga CMake 3.16+
- `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` — ładuje cały system budowania ESP-IDF (komponenty, toolchain, flagi)
- `project(hello_world)` — nazwa projektu (pojawi się w logach kompilacji)

### 5.3 CMakeLists.txt w katalogu main/

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
)
```

**Wyjaśnienie:**
- `SRCS` — lista plików źródłowych do skompilowania
- `INCLUDE_DIRS` — katalogi z plikami nagłówkowymi (`.` = bieżący katalog)

Dla większych projektów:
```cmake
idf_component_register(
    SRCS "main.c" "sensors.c" "display.c" "wifi_manager.c"
    INCLUDE_DIRS "." "include"
    REQUIRES driver esp_wifi nvs_flash        # zależności od komponentów ESP-IDF
    PRIV_REQUIRES esp_timer                    # zależności prywatne
)
```

- `REQUIRES` — publiczne zależności (widoczne dla innych komponentów)
- `PRIV_REQUIRES` — prywatne zależności (tylko wewnętrznie)

### 5.4 Plik main.c — punkt wejścia

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Tag do logowania — identyfikuje źródło komunikatu
static const char *TAG = "HELLO";

// Główna funkcja aplikacji — wywoływana po inicjalizacji systemu
// UWAGA: to NIE jest main() jak na PC!
// System uruchamia FreeRTOS, a app_main() jest taskiem o priorytecie 1.
void app_main(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "  ESP32-WROVER-B — Hello World!");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Licznik: %d", counter++);
        vTaskDelay(pdMS_TO_TICKS(1000));  // Czekaj 1 sekundę
    }
}
```

**Kluczowe koncepty:**

| Element | Opis |
|---------|------|
| `app_main()` | Punkt wejścia — **nie** `main()`. Wywoływany jako task FreeRTOS po inicjalizacji bootloadera i systemu |
| `ESP_LOGI(TAG, ...)` | System logowania ESP-IDF. `I` = Info, `W` = Warning, `E` = Error, `D` = Debug, `V` = Verbose |
| `vTaskDelay()` | Funkcja FreeRTOS — oddaje czas CPU innym taskom na podany okres |
| `pdMS_TO_TICKS()` | Makro konwertujące milisekundy na ticki FreeRTOS |
| `esp_get_idf_version()` | Zwraca wersję ESP-IDF jako string |
| `esp_get_free_heap_size()` | Zwraca ilość wolnej pamięci RAM |

### 5.5 Plik sdkconfig

`sdkconfig` to plik z konfiguracją projektu generowany przez `menuconfig`. **Nie edytuj go ręcznie** — używaj `menuconfig`!

Przykład fragmentu sdkconfig:
```ini
# Automatycznie generowany — NIE EDYTUJ RĘCZNIE!
CONFIG_IDF_TARGET="esp32"
CONFIG_IDF_TARGET_ESP32=y

# CPU
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240

# PSRAM
CONFIG_SPIRAM=y
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_MODE_QUAD=y

# Flash
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# Log level
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

> **💡 Wskazówka:** Plik `sdkconfig.defaults` pozwala zdefiniować domyślne wartości konfiguracji, które zostaną zastosowane przy pierwszym `idf.py build`. Jest to plik, który **powinieneś** commitować do Gita (w przeciwieństwie do `sdkconfig`).

Przykład `sdkconfig.defaults`:
```ini
# sdkconfig.defaults — commituj do Git!
CONFIG_IDF_TARGET="esp32"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

### 5.6 Pełna struktura projektu (rozbudowany projekt)

```
faza_00_modul_01_hello/
├── CMakeLists.txt                ← Główny plik CMake
├── sdkconfig                     ← Konfiguracja (NIE commituj do Git)
├── sdkconfig.defaults            ← Domyślna konfiguracja (commituj!)
├── partitions.csv                ← Opcjonalna tablica partycji
├── main/
│   ├── CMakeLists.txt            ← CMake komponentu main
│   ├── main.c                    ← app_main()
│   ├── Kconfig.projbuild         ← Opcjonalne opcje w menuconfig
│   └── idf_component.yml         ← Opcjonalne zależności od komponentów
├── components/                   ← Własne komponenty (biblioteki)
│   └── my_driver/
│       ├── CMakeLists.txt
│       ├── my_driver.c
│       └── include/
│           └── my_driver.h
├── build/                        ← Katalog budowania (generowany, NIE commituj)
│   ├── bootloader/
│   ├── partition_table/
│   ├── hello_world.bin           ← Skompilowany firmware
│   ├── hello_world.elf           ← Plik ELF (do debugowania)
│   └── hello_world.map           ← Mapa pamięci
└── .gitignore                    ← Powinien ignorować: build/, sdkconfig
```

### 5.7 Plik .gitignore (zalecany)

```gitignore
# Katalog budowania
build/

# Konfiguracja (generowana przez menuconfig)
sdkconfig
sdkconfig.old

# Pliki VS Code (opcjonalnie)
.vscode/

# Pliki systemowe
*.pyc
__pycache__/
```

---

## 6. Pierwsze uruchomienie — build, flash, monitor

### 6.1 Krok 1: Przygotowanie projektu

```bash
# Utwórz nowy projekt z przykładu hello_world
cd ~/esp/projects    # lub dowolny katalog roboczy
idf.py create-project faza_00_modul_01_hello
cd faza_00_modul_01_hello
```

Lub skopiuj przykład:
```bash
cp -r $IDF_PATH/examples/get-started/hello_world ./faza_00_modul_01_hello
cd faza_00_modul_01_hello
```

### 6.2 Krok 2: Ustawienie targetu — `idf.py set-target`

```bash
idf.py set-target esp32
```

**Co robi ta komenda:**
- Tworzy katalog `build/` (jeśli nie istnieje)
- Ustawia `CONFIG_IDF_TARGET="esp32"` w `sdkconfig`
- Inicjalizuje toolchain Xtensa LX6 (a nie np. RISC-V dla ESP32-C3)
- Czyści poprzednią konfigurację (!)

> **⚠️ Ważne:** Zmiana targetu **kasuje** katalog `build/` i resetuje `sdkconfig`. Rób to na początku projektu!

Dostępne targety:

| Target | Procesor | Przykłady płytek |
|--------|----------|-------------------|
| `esp32` | Xtensa LX6 dual-core | ESP32-WROVER-B, ESP32-DevKit |
| `esp32s2` | Xtensa LX7 single-core | ESP32-S2-Saola |
| `esp32s3` | Xtensa LX7 dual-core | ESP32-S3-DevKitC |
| `esp32c3` | RISC-V single-core | ESP32-C3-DevKitM |
| `esp32c6` | RISC-V single-core | ESP32-C6-DevKitC |
| `esp32h2` | RISC-V single-core | ESP32-H2-DevKitM |

### 6.3 Krok 3: Kompilacja — `idf.py build`

```bash
idf.py build
```

**Co się dzieje podczas build:**

```
idf.py build
│
├─ 1. CMake configuration (jeśli pierwszy raz)
│     └─ Analizuje CMakeLists.txt, sdkconfig, generuje build.ninja
│
├─ 2. Kompilacja komponentów
│     ├─ bootloader (second-stage bootloader)
│     ├─ partition_table
│     ├─ freertos
│     ├─ driver
│     ├─ esp_system
│     ├─ ... (setki komponentów)
│     └─ main (Twój kod!)
│
├─ 3. Linkowanie
│     └─ Łączenie wszystkiego w jeden plik ELF
│
└─ 4. Generowanie plików binarnych
      ├─ bootloader.bin     (bootloader)
      ├─ partition-table.bin (tablica partycji)
      └─ hello_world.bin    (aplikacja)
```

Przykładowy output:

```
$ idf.py build
Executing action: all (aliases: build)
Running cmake in directory /home/user/faza_00_modul_01_hello/build
...
[939/939] Generating binary image from built executable
esptool.py v4.7.0
Creating esp32 image...
Merged 2 ELF sections
Successfully created esp32 image.
Generated /home/user/faza_00_modul_01_hello/build/hello_world.bin

Project build complete. To flash, run:
 idf.py flash
or
 idf.py -p PORT flash
```

**Informacje o rozmiarze firmware:**

Po udanym buildzie ESP-IDF wyświetla rozmiar:
```
Total sizes:
Used static DRAM:   31452 bytes (149284 remain)
Used static IRAM:   56789 bytes (  74283 remain)
Used Flash size :  142856 bytes
   .text       :  98543 bytes
   .rodata     :  44313 bytes
Total image size:  231097 bytes (.bin may be padded larger)
```

| Sekcja | Opis |
|--------|------|
| **DRAM** | Zmienne globalne, stosy, heap (520 KB max) |
| **IRAM** | Kod krytyczny czasowo (cache, ISR handlers) |
| **Flash .text** | Kod programu (wykonywany z cache Flash-XIP) |
| **Flash .rodata** | Stałe, stringi, tablice lookup |

### 6.4 Krok 4: Flashowanie — `idf.py flash`

```bash
# Automatyczne wykrycie portu COM
idf.py flash

# Lub z ręcznym podaniem portu
idf.py -p COM3 flash            # Windows
idf.py -p /dev/ttyUSB0 flash    # Linux
idf.py -p /dev/cu.SLAB_USBtoUART flash  # macOS
```

**Co się dzieje podczas flash:**

```
idf.py flash
│
├─ 1. Łączenie z ESP32 przez UART (esptool.py)
│     └─ Reset + GPIO0 LOW → tryb bootloadera (download mode)
│        (na NodeMCU-32 dzieje się automatycznie przez DTR/RTS)
│
├─ 2. Wgrywanie partycji:
│     ├─ Bootloader     → adres 0x1000
│     ├─ Partition table → adres 0x8000
│     └─ Application    → adres 0x10000
│
└─ 3. Reset ESP32 → uruchomienie nowego firmware
```

Przykładowy output:
```
$ idf.py -p COM3 flash
Executing action: flash
Serial port COM3
Connecting....
Chip is ESP32-D0WD-V3 (revision v3.1)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, ...
Crystal is 40MHz

Changing baud rate to 460800
...
Wrote 231472 bytes at 0x00010000 in 5.3 seconds (349.5 kbit/s)...

Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

**Prędkość flashowania:**
Domyślnie `esptool.py` używa baudu 460800. Można zwiększyć:
```bash
idf.py -p COM3 -b 921600 flash
```

### 6.5 Krok 5: Monitor — `idf.py monitor`

```bash
idf.py -p COM3 monitor
```

**Co robi monitor:**
- Otwiera port szeregowy (domyślnie 115200 baud)
- Wyświetla logi z `ESP_LOGx()` i `printf()`
- **Automatycznie dekoduje panic backtraces** — jeśli ESP32 się zcrashuje, pokaże nazwy funkcji i numery linii zamiast surowych adresów

Przykładowy output:
```
--- idf_monitor on COM3 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T followed by Ctrl+H | Help: Ctrl+T followed by Ctrl+H ---
I (234) cpu_start: Pro cpu start user code
I (234) cpu_start: cpu freq: 240000000 Hz
I (235) heap_init: Initializing. RAM available for dynamic allocation:
I (242) heap_init:  AT 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (248) heap_init:  AT 3FFB2EF8 len 0002D108 (180 KiB): DRAM
I (254) heap_init:  AT 3FFE0440 len 0001FBC0 (126 KiB): D/IRAM
I (261) spi_psram: Found 8MBit SPI RAM device
I (265) spi_psram: SPI RAM mode: qio
I (270) spi_psram: PSRAM initialized, cache is on
I (275) cpu_start: Pro cpu up
I (279) cpu_start: Application information:
I (284) cpu_start:   Project name:     hello_world
I (289) cpu_start:   App version:      1.0
I (294) cpu_start:   Compile time:     Feb 28 2026 23:15:00
I (300) HELLO: ====================================
I (305) HELLO:   ESP32-WROVER-B — Hello World!
I (310) HELLO: ====================================
I (315) HELLO: ESP-IDF version: v5.4
I (320) HELLO: Free heap: 4388576 bytes
I (325) HELLO: Licznik: 0
I (1325) HELLO: Licznik: 1
I (2325) HELLO: Licznik: 2
```

**Skróty klawiszowe monitora:**

| Skrót | Akcja |
|-------|-------|
| `Ctrl+]` | **Wyjście z monitora** |
| `Ctrl+T` → `Ctrl+H` | Pomoc (lista skrótów) |
| `Ctrl+T` → `Ctrl+R` | Reset ESP32 |
| `Ctrl+T` → `Ctrl+F` | Wejście w tryb download (boot mode) |
| `Ctrl+T` → `Ctrl+A` | Reset i wejście w download mode |
| `Ctrl+T` → `Ctrl+Y` | Toggle timestamps |
| `Ctrl+T` → `Ctrl+P` | Wstrzymanie (pause) |

### 6.6 Komenda all-in-one

Najwygodniejsza komenda — buduje, flashuje i uruchamia monitor:

```bash
idf.py -p COM3 flash monitor
```

Lub krótko z VS Code: kliknij ikonę 🔥 (Build, Flash and Monitor) na dolnym pasku.

### 6.7 Inne przydatne komendy idf.py

```bash
# Pełne czyszczenie (kasuje build/)
idf.py fullclean

# Tylko wyczyszczenie outputu (bez usuwania configu)
idf.py clean

# Uruchomienie menuconfig
idf.py menuconfig

# Rozmiar firmware — szczegółowa analiza
idf.py size
idf.py size-components    # rozmiar każdego komponentu
idf.py size-files         # rozmiar każdego pliku

# Kasowanie całego flash (przydatne przy problemach)
idf.py -p COM3 erase-flash

# Tylko wygenerowanie sdkconfig (bez kompilacji)
idf.py reconfigure

# Otwarcie dokumentacji
idf.py docs
```

---

## 7. Konfiguracja menuconfig — target ESP32 i PSRAM

`menuconfig` to interaktywny konfigurator projektu ESP-IDF. Pozwala na ustawienie setek opcji bez ręcznej edycji plików. Konfiguracja zapisywana jest w pliku `sdkconfig`.

### 7.1 Uruchomienie menuconfig

```bash
# Z terminala:
idf.py menuconfig

# Z VS Code:
# Ctrl+Shift+P → "ESP-IDF: SDK Configuration editor (Menuconfig)"
# lub kliknij ⚙️ na dolnym pasku
```

> **💡 Wskazówka:** Rozszerzenie ESP-IDF w VS Code oferuje **graficzną wersję menuconfig** — jest wygodniejsza niż terminalowa. Uruchom ją przez `Ctrl+Shift+P` → `SDK Configuration editor`.

### 7.2 Nawigacja w menuconfig (terminal)

```
┌──────────────────── ESP-IDF Configuration ─────────────────────┐
│                                                                  │
│  Bootloader config  --->                                         │
│  Serial flasher config  --->                                     │
│  Partition Table  --->                                           │
│  Compiler options  --->                                          │
│  Component config  --->                                          │
│     ├── Bluetooth  --->                                          │
│     ├── Driver configurations  --->                              │
│     ├── ESP System Settings  --->                                │
│     ├── ESP PSRAM  --->                                          │
│     ├── ESP-TLS  --->                                            │
│     ├── FreeRTOS  --->                                           │
│     ├── HTTP Server  --->                                        │
│     ├── Log output  --->                                         │
│     ├── LWIP  --->                                               │
│     └── Wi-Fi  --->                                              │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│ ↑↓ Navigate  │  Enter: Select  │  Y/N: Toggle  │  /: Search    │
│ ?: Help      │  S: Save        │  Q: Quit                       │
└──────────────────────────────────────────────────────────────────┘
```

| Klawisz | Akcja |
|---------|-------|
| `↑` / `↓` | Nawigacja |
| `Enter` | Wejście do podmenu |
| `Y` / `N` | Włączenie / wyłączenie opcji |
| `Space` | Toggle |
| `/` | **Wyszukiwanie** (bardzo przydatne!) |
| `?` | Pomoc dla zaznaczonej opcji |
| `S` | Zapisz |
| `Q` | Wyjdź |

### 7.3 Kluczowe ustawienia dla ESP32-WROVER-B

#### 7.3.1 Target — ESP32

```
Top level
  → Component config
    → ESP System Settings
      → Chip revision (minimum supported: v0)
```

Ale zazwyczaj target ustawiasz komendą:
```bash
idf.py set-target esp32
```

#### 7.3.2 Częstotliwość CPU

```
Component config
  → ESP System Settings
    → CPU frequency
      (X) 240 MHz     ← Zalecane dla WROVER-B (maks. wydajność)
      ( ) 160 MHz
      ( )  80 MHz
```

> **Uwaga:** 240 MHz to maksimum dla ESP32. WROVER-B obsługuje tę częstotliwość stabilnie. Dla oszczędności energii (np. na baterii) można zmniejszyć do 160 lub 80 MHz.

#### 7.3.3 PSRAM — konfiguracja (KRYTYCZNE dla WROVER-B!)

ESP32-WROVER-B posiada **wbudowany 8 MB PSRAM** (w modelu z 8MB; lub 4 MB w starszych wariantach). Musi być aktywowany w menuconfig!

```
Component config
  → ESP PSRAM
    [*] Support for external, SPI-connected RAM    ← WŁĄCZ!

    → SPI RAM config
      → Mode (QUAD/OCT) of SPI RAM chip in use
        (X) Quad Mode                              ← Dla WROVER-B
      
      → Set RAM clock speed
        (X) 80 MHz                                 ← Optymalna prędkość
        ( ) 40 MHz
      
      → Type of SPI RAM chip in use
        (X) Auto-detect                            ← Automatyczny wybór

      → SPI RAM access method
        (X) Make RAM allocatable using heap_caps_malloc(... MALLOC_CAP_SPIRAM)
            ← Jawna alokacja
        ( ) Integrate RAM into the ESP32 memory map
            ← Memory-mapped access
        ( ) Make RAM allocatable using malloc() as well
            ← Automatyczna alokacja (alokator wybiera SRAM/PSRAM)
```

**Metody dostępu do PSRAM — wyjaśnienie:**

| Metoda | `CONFIG_*` | Opis | Kiedy używać |
|--------|-----------|------|-------------|
| **Jawna alokacja** | `SPIRAM_USE_CAPS_ALLOC` | `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` | Pełna kontrola — **zalecane na początek** |
| **Integracja z mapą pamięci** | `SPIRAM_USE_MEMMAP` | Dostęp przez wskaźnik do stałego adresu | Zaawansowane, statyczne bufory |
| **Automatyczna alokacja** | `SPIRAM_USE_MALLOC` | `malloc()` automatycznie używa PSRAM gdy SRAM się wyczerpie | Wygodne, ale mniej kontroli |

> **⚠️ Ważne:** DMA (np. SPI, I2S) **nie może** operować bezpośrednio na danych w PSRAM! Bufory DMA muszą być w wewnętrznym SRAM. Używaj `heap_caps_malloc(size, MALLOC_CAP_DMA)` dla buforów DMA.

**Przykład weryfikacji PSRAM w kodzie:**

```c
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "PSRAM_TEST";

void app_main(void)
{
    // Sprawdzenie czy PSRAM jest dostępne
    size_t psram_size = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM size: %u bytes (%.1f MB)", 
             psram_size, psram_size / (1024.0 * 1024.0));

    // Jawna alokacja w PSRAM
    uint8_t *buffer = heap_caps_malloc(1024 * 1024, MALLOC_CAP_SPIRAM);
    if (buffer) {
        ESP_LOGI(TAG, "Zaalokowano 1 MB w PSRAM — adres: %p", buffer);
        
        // Test zapisu/odczytu
        buffer[0] = 0xAA;
        buffer[1024*1024 - 1] = 0x55;
        ESP_LOGI(TAG, "Zapis/odczyt OK: first=0x%02X, last=0x%02X", 
                 buffer[0], buffer[1024*1024 - 1]);
        
        heap_caps_free(buffer);
    } else {
        ESP_LOGE(TAG, "BŁĄD: Nie udało się zaalokować PSRAM!");
    }

    // Informacje o pamięci
    ESP_LOGI(TAG, "Wolna DRAM:  %u bytes", 
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Wolna PSRAM: %u bytes", 
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Wolna DMA:   %u bytes", 
             heap_caps_get_free_size(MALLOC_CAP_DMA));
}
```

#### 7.3.4 Flash — konfiguracja

```
Serial flasher config
  → Flash SPI speed
    (X) 80 MHz         ← Szybki dostęp do Flash
  
  → Flash size
    (X) 4 MB           ← Typowe dla NodeMCU-32
    ( ) 8 MB
    ( ) 16 MB
  
  → Flash SPI mode
    (X) QIO            ← Quad I/O — najszybszy tryb (4 linie danych)
    ( ) QOUT           ← Quad Output (4 linie tylko odczyt)
    ( ) DIO            ← Dual I/O (2 linie danych)
    ( ) DOUT           ← Dual Output (2 linie tylko odczyt)
```

> **💡 Wskazówka:** QIO jest najszybszy, ale nie wszystkie moduły Flash go obsługują. Jeśli ESP32 nie startuje po ustawieniu QIO, spróbuj DIO.

#### 7.3.5 Tablica partycji

```
Partition Table
  → Partition Table
    (X) Single factory app, no OTA     ← Na początek
    ( ) Factory app, two OTA definitions ← Gdy będziesz robić OTA
    ( ) Custom partition table CSV       ← Własna tablica
```

Domyślna tablica partycji:

| Nazwa | Typ | Podtyp | Offset | Rozmiar | Opis |
|-------|-----|--------|--------|---------|------|
| nvs | data | nvs | 0x9000 | 24 KB | Non-Volatile Storage |
| phy_init | data | phy | 0xf000 | 4 KB | Dane kalibracji PHY (WiFi/BT) |
| factory | app | factory | 0x10000 | ~1 MB | Aplikacja |

#### 7.3.6 Poziom logowania

```
Component config
  → Log output
    → Default log verbosity
      ( ) No output        ← Brak logów
      ( ) Error            ← Tylko błędy
      ( ) Warning          ← Błędy + ostrzeżenia
      (X) Info             ← Zalecane na co dzień
      ( ) Debug            ← Szczegółowe informacje
      ( ) Verbose          ← Wszystko (spowalnia!)
```

#### 7.3.7 FreeRTOS

```
Component config
  → FreeRTOS
    → Kernel
      → configTICK_RATE_HZ
        (1000)              ← Tick co 1 ms (domyślnie, OK)
      → configMAX_PRIORITIES
        (25)                ← Domyślnie, OK
```

### 7.4 Zapisywanie konfiguracji

Po dokonaniu zmian:
1. W terminalu: naciśnij `S` (Save), potem `Q` (Quit)
2. W VS Code GUI: kliknij **Save** na górze editora
3. Zmiany zapisują się do pliku `sdkconfig`

Po zmianie konfiguracji **musisz ponownie skompilować:**
```bash
idf.py build
```

### 7.5 Przeszukiwanie opcji

Nie wiesz gdzie jest opcja? Użyj wyszukiwania!

```bash
# W menuconfig terminal: naciśnij /
# Wpisz np.: PSRAM
# Wyświetli wszystkie opcje zawierające "PSRAM" z ich lokalizacją

# Lub z terminala:
grep -r "PSRAM" $IDF_PATH/Kconfig*
```

---

## 8. Najczęstsze problemy i rozwiązania

### Problem 1: Port COM nie jest wykrywany

```
Symptom: "No serial ports detected" lub brak portu w Menedżerze urządzeń
```

**Rozwiązania:**
1. Zainstaluj sterownik USB-UART (CP2102 lub CH340 — patrz sekcja 1)
2. Spróbuj inny kabel USB — niektóre kable to "charge-only" (bez linii danych!)
3. Spróbuj inny port USB
4. Sprawdź w Menedżerze urządzeń czy sterownik jest poprawnie załadowany

### Problem 2: Błąd "Failed to connect to ESP32"

```
Symptom: "A fatal error occurred: Failed to connect to ESP32: No serial data received."
```

**Rozwiązania:**
1. Przytrzymaj przycisk **BOOT** (GPIO0) podczas flashowania
2. Naciśnij **EN** (reset) trzymając **BOOT** — to ręczne wejście w download mode
3. Zmniejsz prędkość flashowania: `idf.py -p COM3 -b 115200 flash`

### Problem 3: Bootloop (ciągłe restarty)

```
Symptom: ESP32 restartuje się co kilka sekund, w monitorze widać "Guru Meditation Error"
```

**Rozwiązania:**
1. Uruchom `idf.py monitor` — odczytaj backtrace z nazwami funkcji
2. Sprawdź linię, w której następuje crash
3. Upewnij się, że nie masz przepełnienia stosu (stack overflow) — zwiększ stos taska
4. Skasuj Flash i spróbuj ponownie: `idf.py -p COM3 erase-flash`

### Problem 4: PSRAM nie jest wykrywany

```
Symptom: "E (336) spi_psram: PSRAM ID read error" lub brak PSRAM w logach
```

**Rozwiązania:**
1. Sprawdź w `menuconfig` czy PSRAM jest włączony (sekcja 7.3.3)
2. Spróbuj prędkość 40 MHz zamiast 80 MHz
3. Upewnij się, że masz moduł **ESP32-WROVER** (nie ESP32-WROOM, który nie ma PSRAM!)
4. Sprawdź tryb SPI Flash — przy QIO some PSRAM may conflict; try DIO

### Problem 5: Build trwa bardzo długo

**Rozwiązania:**
1. Pierwszy build zawsze jest długi (kompilacja całego frameworka) — kolejne będą szybkie
2. Użyj `ccache`: `idf.py set-target esp32` → automatycznie konfiguruje ccache
3. Upewnij się, że projekt nie jest na dysku sieciowym lub w folderze synchronizowanym (OneDrive, Dropbox)
4. Zamknij antywirusa tymczasowo — skanowanie plików spowalnia kompilację

### Problem 6: Python / pip errors

```
Symptom: "ModuleNotFoundError" lub problemy z Pythonem
```

**Rozwiązania:**
1. Użyj terminalu ESP-IDF (nie systemowego) — ma poprawne virtualenv
2. Ponownie zainstaluj narzędzia: `python $IDF_PATH/tools/idf_tools.py install-python-env`
3. Na Windows: upewnij się, że Python nie jest z Microsoft Store (preferuj python.org)

---

## 9. Podsumowanie i dalsze kroki

### Co nauczyłeś się w tym module:

| Temat | Kluczowe pojęcia |
|-------|-------------------|
| **ESP-IDF** | Framework od Espressif, zawiera kompilator, RTOS, sterowniki |
| **Toolchain** | Cross-compiler Xtensa, CMake, Ninja |
| **Struktura projektu** | `CMakeLists.txt`, `main/`, `sdkconfig`, `build/` |
| **Workflow** | `idf.py set-target` → `build` → `flash` → `monitor` |
| **menuconfig** | Konfiguracja PSRAM, CPU, Flash, logów, partycji |
| **PSRAM** | 8 MB w WROVER-B, jawna alokacja lub automatyczna |

### Checklist — potwierdź, że wszystko działa:

- [ ] ESP-IDF zainstalowane, `idf.py --version` zwraca numer wersji
- [ ] Rozszerzenie ESP-IDF w VS Code skonfigurowane
- [ ] Port COM wykrywany w systemie
- [ ] `idf.py set-target esp32` — działa bez błędów
- [ ] `idf.py build` — kompilacja zakończona sukcesem
- [ ] `idf.py flash` — firmware wgrany na płytkę
- [ ] `idf.py monitor` — widoczne logi z ESP32
- [ ] PSRAM aktywowany i widoczny w logach startowych
- [ ] `menuconfig` — potrafisz nawigować i zmieniać opcje

### Dalsze kroki — Moduł 0.2:

W kolejnym module poznasz **architekturę ESP32-WROVER-B**:
- Rdzenie Xtensa LX6 (dual-core)
- Mapa pamięci: IRAM, DRAM, RTC SLOW/FAST, PSRAM
- Zasilanie, stany uśpienia
- Pinout NodeMCU-32, strapping pins

---

## 10. Źródła i dokumentacja

### Oficjalna dokumentacja ESP-IDF

| Zasób | Link |
|-------|------|
| **ESP-IDF Programming Guide** | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/ |
| **Get Started** | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/ |
| **API Reference** | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/ |
| **Build System (CMake)** | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html |
| **Menuconfig** | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html |
| **PSRAM** | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html |
| **Partition Tables** | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/partition-tables.html |
| **ESP-IDF Examples (GitHub)** | https://github.com/espressif/esp-idf/tree/master/examples |

### VS Code Extension

| Zasób | Link |
|-------|------|
| **ESP-IDF VS Code Extension** | https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension |
| **Extension Tutorial** | https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/toc.md |

### Datasheet'y (w katalogu projektu)

| Plik | Opis |
|------|------|
| `NODEMCU32-V1.3.PDF` | Schemat płytki NodeMCU-32 |

### Przydatne narzędzia

| Narzędzie | Opis |
|-----------|------|
| `esptool.py` | Narzędzie do flashowania i odczytu informacji o chipie |
| `idf.py` | Główne narzędzie CLI ESP-IDF |
| `xtensa-esp32-elf-gdb` | Debugger GDB dla Xtensa |
| `esp-idf-monitor` | Monitor UART z dekodowaniem backtrace |

---

> *Moduł 0.1 — wersja 1.0 · Data: 28.02.2026*  
> *Następny: [Moduł 0.2 — Architektura ESP32-WROVER-B]()*
