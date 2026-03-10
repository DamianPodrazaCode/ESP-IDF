# Plan Nauki ESP32-WROVER-B (ESP-IDF + VS Code)

> **Platforma:** ESP32-WROVER-B (NodeMCU-32) · **Framework:** ESP-IDF (CMake) · **IDE:** Visual Studio Code + ESP-IDF Extension  
> **Łączny szacowany czas:** ~52 tygodnie (≈ 12 miesięcy) przy nauce ~8-12 h/tydzień  
> **Data rozpoczęcia:** marzec 2026

---

## Poziomy zaawansowania

| Symbol | Poziom | Opis |
|--------|--------|------|
| 🟢 | **Laik** | Pierwsze kroki, podstawy elektroniki i toolchaina |
| 🟡 | **Początkujący** | Samodzielne projekty z pojedynczymi peryferiami |
| 🟠 | **Średniozaawansowany** | Integracja wielu peryferiów, RTOS, komunikacja bezprzewodowa |
| 🔴 | **Zaawansowany** | Optymalizacja, LVGL, przetwarzanie obrazu, systemy real-time |
| ⚫ | **Profesjonalista** | Produkcyjne wzorce, OTA, bezpieczeństwo, architektura |

---

## Faza 0 — Przygotowanie środowiska 🟢

**Czas: tydzień 1–2 (2 tygodnie)**

### Moduł 0.1 — Instalacja i konfiguracja
- Instalacja ESP-IDF (stabilna wersja v5.x) i rozszerzenia ESP-IDF do VS Code.
- Konfiguracja toolchaina Xtensa, CMake, Ninja.
- Struktura projektu ESP-IDF: `main/`, `CMakeLists.txt`, `sdkconfig`.
- Pierwsze uruchomienie: `idf.py build`, `idf.py flash`, `idf.py monitor`.
- Konfiguracja `menuconfig` — wybór targetu ESP32, ustawienia PSRAM.

### Moduł 0.2 — Architektura ESP32-WROVER-B
- Rdzenie Xtensa LX6 (dual-core), pamięć: 520 KB SRAM + 4/8 MB PSRAM + 4/16 MB Flash.
- Mapa pamięci: IRAM, DRAM, RTC SLOW/FAST, PSRAM.
- Zasilanie, stany uśpienia (light sleep, deep sleep).
- Pinout NodeMCU-32: GPIO, strapping pins, bootstrap.

### Moduł 0.3 — Podstawowe narzędzia programisty
- `idf.py menuconfig` — konfiguracja projektu.
- `idf.py monitor` — UART monitor z dekodowaniem panic backtrace.
- Logi: `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, poziomy logowania.
- JTAG debugging (opcjonalnie, jeśli dostępny adapter).

**Sprzęt:** NodeMCU-32, kabel USB, płytka stykowa.

---

## Faza 1 — GPIO i podstawowe peryferia cyfrowe 🟢

**Czas: tydzień 3–6 (4 tygodnie)**

### Moduł 1.1 — GPIO & RTC GPIO
- `gpio_config()`, tryby: `INPUT`, `OUTPUT`, `OPEN_DRAIN`.
- Pull-up / pull-down wewnętrzne.
- Przerwania GPIO: `gpio_install_isr_service()`, `gpio_isr_handler_add()`.
- RTC GPIO — piny dostępne w deep sleep, `rtc_gpio_init()`.
- **Ćwiczenie:** Miganie LED, debouncing przycisku ze sprzętowym i programowym filtrem.

**Sprzęt:** LED, rezystory, przyciski, płytka stykowa.

### Moduł 1.2 — General Purpose Timer (GPTimer)
- Timery sprzętowe ESP32 (2 grupy × 2 timery = 4).
- API `gptimer_new_timer()`, prescaler, alarm, auto-reload.
- Callback timera w kontekście ISR.
- **Ćwiczenie:** Precyzyjne odmierzanie czasu, generowanie sygnału prostokątnego.

**Sprzęt:** LED, oscyloskop (opcjonalny).

### Moduł 1.3 — Pulse Counter (PCNT)
- Zliczanie impulsów sprzętowe (8 kanałów).
- Tryby: zliczanie w górę/dół, dekoder kwadraturowy.
- Filtrowanie glitchy: `pcnt_unit_set_glitch_filter()`.
- **Ćwiczenie 1:** Odczyt taniego enkodera obrotowego (pokrętło).
- **Ćwiczenie 2:** Odczyt precyzyjnego enkodera 500 kroków z pozycją i kierunkiem.

**Sprzęt:** Enkoder obrotowy tani (36), precyzyjny enkoder 500 kroków (35).

### Moduł 1.4 — LED Control (LEDC)
- PWM sprzętowy: 8 kanałów szybkich, 8 wolnych.
- Konfiguracja timer + channel: rozdzielczość, częstotliwość, duty cycle.
- Hardware fade: `ledc_set_fade_with_time()`, `ledc_fade_start()`.
- **Ćwiczenie:** Płynne rozjaśnianie/ściemnianie LED (breathing effect).

**Sprzęt:** LED, rezystor.

---

## Faza 2 — Peryferia analogowe 🟡

**Czas: tydzień 7–9 (3 tygodnie)**

### Moduł 2.1 — Analog to Digital Converter (ADC)
- ADC1 (8 kanałów, GPIO 32–39) i ADC2 (10 kanałów — uwaga na konflikty z WiFi!).
- Konfiguracja: rozdzielczość (9–12 bit), attenuation (0/2.5/6/11 dB).
- Kalibracja ADC: `adc_cali_create_scheme_curve_fitting()`.
- Continuous mode vs One-shot mode.
- **Ćwiczenie 1:** Odczyt czujnika światła LDR (31) — pomiar natężenia oświetlenia.
- **Ćwiczenie 2:** Odczyt dżojstika (34) — dwie osie + przycisk.
- **Ćwiczenie 3:** Odczyt czujnika odległości Sharp GP2Y0A41SK0F (22) — konwersja napięcie → cm.

**Sprzęt:** LDR + rezystor (31), dżojstik (34), Sharp GP2Y0A41SK0F (22).

### Moduł 2.2 — Digital to Analog Converter (DAC)
- Dwa kanały: DAC1 (GPIO25), DAC2 (GPIO26).
- Generowanie stałego napięcia: `dac_output_voltage()`.
- DMA + DAC: generowanie przebiegów (sinus, piła, trójkąt).
- **Ćwiczenie:** Generator sygnału — wyjście sinusoidalne na DAC, weryfikacja czujnikiem dźwięku (33).

**Sprzęt:** Czujnik dźwięku LM386 (33), głośnik/buzzer.

### Moduł 2.3 — Sigma-Delta Modulation (SDM)
- Modulacja sigma-delta — alternatywa dla DAC/PWM.
- Konfiguracja: `sdm_new_channel()`, prescale, duty.
- **Ćwiczenie:** Sterowanie jasnością LED z SDM, porównanie z LEDC PWM.

**Sprzęt:** LED, rezystor, filtr RC (opcjonalny).

---

## Faza 3 — Magistrale komunikacyjne 🟡

**Czas: tydzień 10–15 (6 tygodni)**

### Moduł 3.1 — UART (Universal Asynchronous Receiver/Transmitter)
- Konfiguracja: baud rate, bity danych, parzystość, stop bity.
- `uart_driver_install()`, bufory TX/RX, ring buffer.
- Przerwania UART vs polling.
- Wzorzec zdarzeń: `uart_pattern_queue_reset()`.
- **Ćwiczenie:** Komunikacja z PC — wysyłanie danych z czujników, odbieranie komend.

**Sprzęt:** NodeMCU-32 (wbudowany USB-UART).

### Moduł 3.2 — I2C (Inter-Integrated Circuit)
- Master: `i2c_master_bus_add_device()`, start, ACK/NACK, stop.
- Skanowanie magistrali I2C (wykrywanie adresów urządzeń).
- Prędkości: Standard (100 kHz), Fast (400 kHz).
- **Ćwiczenie 1:** Odczyt OLED SSD1306 0.96″ (4) — wyświetlanie tekstu (inicjalizacja, komendy, bufor ramki).
- **Ćwiczenie 2:** Odczyt IMU MPU6050 (15) — akcelerometr + żyroskop, wyświetlanie na SSD1306.
- **Ćwiczenie 3:** Odczyt czujnika ciśnienia BMP180 (28) — temperatura i ciśnienie atmosferyczne.
- **Ćwiczenie 4:** Detektor koloru TCS34725 (27) — odczyt wartości RGB.
- **Ćwiczenie 5:** Czujnik odległości VL53L0X (24) — Time-of-Flight.

**Sprzęt:** OLED SSD1306 (4), MPU6050 (15), BMP180 (28), TCS34725 (27), VL53L0X (24).

### Moduł 3.3 — SPI Master Driver
- Konfiguracja: `spi_bus_initialize()`, `spi_bus_add_device()`.
- Tryby SPI (CPOL, CPHA), prędkość zegara, CS.
- Transakcje: `spi_device_transmit()`, polling vs DMA.
- Half-duplex vs full-duplex.
- **Ćwiczenie 1:** Wyświetlacz ILI9341 SPI 2.2″ (7) — inicjalizacja, wypełnianie kolorami.
- **Ćwiczenie 2:** Wyświetlacz ST7789 1.3″ MSP1308 (5) — wyświetlanie grafiki.

**Sprzęt:** ILI9341 SPI 2.2″ (7), ST7789 1.3″ (5).

### Moduł 3.4 — SPI Slave Driver
- Konfiguracja slave: `spi_slave_initialize()`.
- Transakcje slave z DMA.
- **Ćwiczenie:** Komunikacja dwóch ESP32 (jeśli dostępny drugi moduł) lub loopback test.

### Moduł 3.5 — SPI Flash API
- Partycje Flash: tablica partycji, NVS, SPIFFS, FAT.
- `esp_partition_find()`, odczyt/zapis surowych danych.
- NVS (Non-Volatile Storage): `nvs_set_*()`, `nvs_get_*()`.
- SPIFFS / LittleFS — system plików na Flash.
- **Ćwiczenie:** Zapis ustawień (kalibracja czujnika) do NVS, odczyt po restarcie.

### Moduł 3.6 — I2S (Inter-IC Sound)
- Tryby: Standard, PDM, TDM.
- Konfiguracja: sample rate, rozdzielczość, kanały.
- DMA bufory, callback funkcje.
- **Ćwiczenie:** Odczyt mikrofonu ze wzmacniaczem LM386 (33) przez ADC + DMA, analiza poziomu dźwięku.

**Sprzęt:** Czujnik dźwięku LM386 (33).

---

## Faza 4 — Peryferia specjalne i zaawansowane GPIO 🟡

**Czas: tydzień 16–19 (4 tygodnie)**

### Moduł 4.1 — Remote Control Transceiver (RMT)
- Nadawanie i odbiór sygnałów IR, WS2812, DHT.
- Konfiguracja kanałów RMT: `rmt_new_tx_channel()`, `rmt_new_rx_channel()`.
- Enkodery i dekodery: NEC, WS2812 (pixel LED).
- **Ćwiczenie 1:** Czujnik DHT11 (30) — odczyt temperatury i wilgotności przez RMT.
- **Ćwiczenie 2:** Czujnik ultradźwiękowy HC-SR04 (20) — pomiar odległości przez RMT (trigger + echo).

**Sprzęt:** DHT11 (30), HC-SR04 (20).

### Moduł 4.2 — Motor Control PWM (MCPWM)
- Architektura: operator, comparator, generator, dead-time.
- Sterowanie silnikami DC, servo, mostek H.
- Synchronizacja PWM, fault handling.
- Capture module — pomiar częstotliwości/szerokości impulsu.
- **Ćwiczenie 1:** Sterowanie serwomechanizmem (jeśli dostępny).
- **Ćwiczenie 2:** MCPWM Capture — pomiar HC-SR04 (20) / SRF05 (21).

**Sprzęt:** HC-SR04 (20), SRF05 (21).

### Moduł 4.3 — Capacitive Touch Sensor
- Touch piny ESP32 (Touch0–Touch9).
- Konfiguracja: `touch_pad_config()`, próg detekcji.
- Filtrowanie, kalibracja, przerwania touch.
- **Ćwiczenie:** Przycisk dotykowy — sterowanie LED przez touch pad.

**Sprzęt:** Kabel/folia jako elektroda dotykowa.

### Moduł 4.4 — TWAI (Two-Wire Automotive Interface / CAN)
- Protokół CAN 2.0: ramki standard i extended.
- Konfiguracja: `twai_driver_install()`, bit timing, filtry akceptacji.
- Transmisja i odbiór ramek, obsługa błędów.
- **Ćwiczenie:** Loopback test lub komunikacja dwóch ESP32 przez TWAI (self-test mode).

### Moduł 4.5 — SDMMC / SD SPI Host
- SD Pull-up Requirements (obowiązkowe pull-upy na liniach CMD, DAT).
- Tryb SDMMC (4-bit, 1-bit) vs SPI.
- `sdmmc_host_init()`, `esp_vfs_fat_sdmmc_mount()`.
- SDIO Card Slave Driver (teoria).
- **Ćwiczenie:** Zapis/odczyt plików na karcie micro SD.

**Sprzęt:** Moduł karty SD (opcjonalny).

---

## Faza 5 — PSRAM 🟡

**Czas: tydzień 20 (1 tydzień)**

### Moduł 5.1 — PSRAM (Pseudo-Static RAM)
- Konfiguracja PSRAM w `menuconfig`: `CONFIG_SPIRAM_SUPPORT`.
- Tryby użycia:
  - `malloc()` / `calloc()` → automatyczna alokacja w PSRAM (z `CONFIG_SPIRAM_USE_MALLOC`).
  - `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` — alokacja jawna.
  - Memory-mapped access.
- Ograniczenia: DMA nie może operować bezpośrednio na PSRAM, konieczność buforów w DRAM.
- Strategia alokacji: co umieszczać w SRAM vs PSRAM.
- **Ćwiczenie 1:** Alokacja dużego frame-buffera (320×240×2 = 150 KB) w PSRAM.
- **Ćwiczenie 2:** Profiling pamięci: `heap_caps_get_info()`, `heap_caps_print_heap_info()`.

**Sprzęt:** NodeMCU-32 (PSRAM wbudowany w ESP32-WROVER-B).

---

## Faza 6 — FreeRTOS od podstaw do systemu czasu rzeczywistego 🟠

**Czas: tydzień 21–28 (8 tygodni)**

### Moduł 6.1 — Podstawy FreeRTOS na ESP32
- Jądro FreeRTOS: scheduler, tick, idle task.
- ESP-IDF: dual-core SMP — core affinity (`xTaskCreatePinnedToCore()`).
- Tworzenie tasków: `xTaskCreate()`, priorytety (0–configMAX_PRIORITIES-1).
- `vTaskDelay()` vs `vTaskDelayUntil()`.
- Stack overflow detection.
- **Ćwiczenie:** Dwa taski na dwóch rdzeniach — blink LED + odczyt ADC (LDR).

**Sprzęt:** LED, LDR (31).

### Moduł 6.2 — Synchronizacja: Semafory i Mutexy
- Binary semaphore: `xSemaphoreCreateBinary()` — sygnalizacja ISR → task.
- Counting semaphore: zarządzanie zasobami.
- Mutex: `xSemaphoreCreateMutex()` — ochrona zasobów współdzielonych.
- Recursive mutex, priority inversion, priority inheritance.
- **Ćwiczenie:** ISR GPIO (przycisk) → binary semaphore → task (toggle LED).

### Moduł 6.3 — Kolejki (Queues)
- `xQueueCreate()`, `xQueueSend()`, `xQueueReceive()`.
- Queue set — nasłuchiwanie na wielu kolejkach.
- Wzorzec producent-konsument.
- **Ćwiczenie:** Task producent (odczyt MPU6050 przez I2C) → queue → task konsument (wyświetlanie na OLED SSD1306).

**Sprzęt:** MPU6050 (15), OLED SSD1306 (4).

### Moduł 6.4 — Notyfikacje tasków (Task Notifications)
- Lekka alternatywa dla semaforów: `xTaskNotifyGive()`, `ulTaskNotifyTake()`.
- Bezpośrednie powiadamianie tasków z ISR.
- **Ćwiczenie:** Timer sprzętowy → task notification → odczyt czujnika.

### Moduł 6.5 — Event Groups i Event Loops
- `xEventGroupCreate()`, `xEventGroupSetBits()`, `xEventGroupWaitBits()`.
- ESP Event Loop: `esp_event_loop_create()`, custom events.
- Synchronizacja wielu tasków na kombinacjach zdarzeń.
- **Ćwiczenie:** System monitoringu: event group z flagami (ruch PIR, próg dźwięku, próg odległości).

**Sprzęt:** PIR HC-SR501 (26), czujnik dźwięku LM393 (32), HC-SR04 (20).

### Moduł 6.6 — Timery programowe (Software Timers)
- `xTimerCreate()`, one-shot vs auto-reload.
- Timer daemon task, przetwarzanie w kontekście timer service.
- **Ćwiczenie:** Periodyczny odczyt DHT11 co 2 sekundy + timeout alarm.

**Sprzęt:** DHT11 (30).

### Moduł 6.7 — Zaawansowany RTOS: Watchdog, Idle Hooks, Stack
- Task Watchdog Timer (TWDT): `esp_task_wdt_add()`.
- Idle hook: `esp_register_freertos_idle_hook()`.
- High-water mark: `uxTaskGetStackHighWaterMark()`.
- `vTaskList()`, `vTaskGetRunTimeStats()` — diagnostyka.
- **Ćwiczenie:** System z watchdog, monitorowanie stack usage, runtime stats.

### Moduł 6.8 — Wzorce projektowe RTOS
- State Machine w tasku (FSM).
- Pipeline: łańcuch tasków z kolejkami.
- Publish-Subscribe: event loop + custom events.
- Resource manager: jeden task zarządzający magistralą I2C/SPI.
- **Ćwiczenie:** System sensoryczny: task odczytu IMU → queue → task filtracji (EMA) → queue → task wyświetlania (OLED).

**Sprzęt:** MPU9250 (17) lub MPU6050 (15), OLED SSD1306 (4).

---

## Faza 7 — WiFi 🟠

**Czas: tydzień 29–32 (4 tygodnie)**

### Moduł 7.1 — WiFi Station (STA)
- Inicjalizacja: `esp_wifi_init()`, `esp_wifi_set_mode(WIFI_MODE_STA)`.
- Łączenie z AP: SSID, hasło, event handler (`WIFI_EVENT`, `IP_EVENT`).
- Reconnect strategy, error handling.
- Skanowanie sieci: `esp_wifi_scan_start()`.
- **Ćwiczenie:** Połączenie z WiFi, wyświetlanie IP na OLED SSD1306.

**Sprzęt:** OLED SSD1306 (4), router WiFi.

### Moduł 7.2 — WiFi Access Point (AP) i AP+STA
- Konfiguracja SoftAP: SSID, hasło, kanał, max połączeń.
- Tryb AP+STA: jednoczesna praca.
- DHCP server konfiguracja.
- **Ćwiczenie:** ESP32 jako AP — klient łączy się i widzi stronę konfiguracyjną.

### Moduł 7.3 — Protokoły sieciowe: HTTP, WebSocket, mDNS
- HTTP Server: `httpd_start()`, rejestracja URI handlerów.
- HTTP Client: `esp_http_client_perform()`.
- WebSocket: real-time dwukierunkowa komunikacja.
- mDNS: `mdns_init()` — odkrywanie usług.
- **Ćwiczenie:** Web dashboard — strona WWW z live danymi z czujników (BMP180 temp, VL53L0X odległość) aktualizowanymi przez WebSocket.

**Sprzęt:** BMP180 (28), VL53L0X (24), OLED SSD1306 (4).

### Moduł 7.4 — MQTT, NTP, OTA
- MQTT Client: `esp_mqtt_client_init()`, publish, subscribe, QoS.
- SNTP: synchronizacja czasu, `esp_sntp_init()`.
- OTA (Over-The-Air): `esp_ota_begin()`, update z HTTP serwera.
- **Ćwiczenie:** Publikowanie danych z czujników do brokera MQTT (np. Mosquitto), OTA update firmware.

---

## Faza 8 — Bluetooth 🟠

**Czas: tydzień 33–36 (4 tygodnie)**

### Moduł 8.1 — Bluetooth Classic (BR/EDR)
- Stack: Bluedroid, profile'e (SPP, A2DP).
- SPP (Serial Port Profile): komunikacja szeregowa przez BT.
- `esp_bt_controller_init()`, `esp_spp_init()`.
- **Ćwiczenie:** SPP — przesyłanie danych z IMU MPU6050 do telefonu (aplikacja Serial Bluetooth Terminal).

**Sprzęt:** MPU6050 (15), telefon z BT.

### Moduł 8.2 — BLE (Bluetooth Low Energy) — Podstawy
- Architektura BLE: GAP, GATT, profile, services, characteristics.
- Advertising, scanning, connection.
- Server GATT: `esp_ble_gatts_register_callback()`.
- **Ćwiczenie:** BLE environmental sensing — udostępnianie temperatury (BMP180) i wilgotności (DHT11) jako GATT characteristics.

**Sprzęt:** BMP180 (28), DHT11 (30).

### Moduł 8.3 — BLE — Zaawansowane
- Client GATT: skanowanie i łączenie z innymi urządzeniami BLE.
- Notifications i Indications.
- BLE bonding, security, pairing.
- BLE Mesh (wstęp).
- **Ćwiczenie:** BLE Client — odczyt danych z drugiego ESP32 działającego jako sensor.

---

## Faza 9 — Wyświetlacze: od tekstowego po zaawansowane TFT 🟠🔴

**Czas: tydzień 37–42 (6 tygodni)**

### Moduł 9.1 — LCD tekstowy 2004A, komunikacja równoległa
- Kontroler HD44780.
- Interfejs 4-bitowy: RS, EN, D4–D7.
- Inicjalizacja, wysyłanie komend i danych.
- Kursor, przewijanie, własne znaki (CGRAM).
- **Ćwiczenie:** Wyświetlanie danych z czujników (temperatura DHT11, odległość HC-SR04) na 4 liniach LCD.

**Sprzęt:** LCD 2004A równoległy (2), DHT11 (30), HC-SR04 (20).

### Moduł 9.2 — LCD tekstowy 2004A, komunikacja I2C (PCF8574)
- Ekspander I/O PCF8574 na magistrali I2C.
- Sterowanie HD44780 przez PCF8574: bit-banging przez I2C.
- Oszczędność pinów GPIO.
- **Ćwiczenie:** Ten sam program co w 9.1, ale przez I2C — porównanie zużycia pinów.

**Sprzęt:** LCD 2004A I2C (3).

### Moduł 9.3 — OLED SSD1306 128×64 I2C
- Sterownik SSD1306: komendy inicjalizacji, tryb adresowania.
- Bufor ramki (128×64 / 8 = 1024 bajtów).
- Renderowanie fontów bitmapowych, rysowanie pikseli.
- Podwójne buforowanie.
- **Ćwiczenie:** Dashboard na OLED — wykresy real-time z akcelerometru MPU6050.

**Sprzęt:** OLED SSD1306 (4), MPU6050 (15).

### Moduł 9.4 — IPS ST7789 1.3″ 240×240 SPI (MSP1308)
- Inicjalizacja ST7789: software reset, display inversion, color format (RGB565).
- SPI z DMA — szybki transfer pikseli.
- Orientacja ekranu, okno rysowania (`CASET`, `RASET`, `RAMWR`).
- **Ćwiczenie:** Kolorowy dashboard — wyświetlanie danych z wielu czujników I2C.

**Sprzęt:** ST7789 MSP1308 (5), BMP180 (28), TCS34725 (27).

### Moduł 9.5 — TFT S6D0164 2.2″ 176×220, równoległy 16-bit
- Interfejs równoległy 16-bitowy: DB0–DB15, WR, RD, RS, CS.
- Obsługa dużej liczby GPIO — mapowanie pinów.
- Inicjalizacja sterownika S6D0164.
- **Ćwiczenie:** Pełnoekranowe wyświetlanie gradientów i wzorów testowych.

**Sprzęt:** TFT S6D0164 (6).

### Moduł 9.6 — TFT ILI9341 2.2″ 240×320 SPI
- Inicjalizacja ILI9341 przez SPI.
- Buforowanie ramki w PSRAM (240×320×2 = 150 KB).
- Porównanie wydajności: bez DMA vs z DMA.
- **Ćwiczenie:** Animowany interfejs — odbijająca się piłka, FPS counter.

**Sprzęt:** ILI9341 SPI 2.2″ (7).

### Moduł 9.7 — TFT ILI9341 3.2″ 320×240, 16-bit + Touch XPT2046 (MRB3205)
- Interfejs równoległy 16-bitowy — konfiguracja dla 3.2″.
- ESP-IDF LCD driver: `esp_lcd_panel_io_i80`.
- Touch XPT2046: odczyt pozycji przez SPI, kalibracja.
- **Ćwiczenie:** Ekran dotykowy — przyciski na wyświetlaczu, rysowanie palcem.

**Sprzęt:** TFT ILI9341 3.2″ MRB3205 (8).

### Moduł 9.8 — TFT SSD1963 4.3″ 480×272 + Touch XPT2046
- Kontroler SSD1963 — sterownik z wbudowanym frame bufferem.
- Interfejs 16-bitowy, konfiguracja PLL, panel timing.
- Duża rozdzielczość — konieczność PSRAM na bufory.
- Touch: kalibracja 3-punktowa.
- **Ćwiczenie:** GUI z wieloma panelami informacyjnymi i obsługą dotyku.

**Sprzęt:** TFT SSD1963 4.3″ (9).

### Moduł 9.9 — Graficzny mono T6963C 5.1″ 240×128
- Kontroler T6963C — tryb tekstowy i graficzny.
- Interfejs równoległy 8-bitowy.
- Mapa graficzna, fonty, XOR grafika+tekst.
- **Ćwiczenie:** Wykresy danych historycznych z czujników na monochronicznym ekranie.

**Sprzęt:** T6963C 5.1″ ABG240128 (10).

---

## Faza 10 — Biblioteka graficzna 🔴

**Czas: tydzień 43–44 (2 tygodnie)**

### Moduł 10.1 — Własna biblioteka graficzna (HAL)
- Warstwa abstrakcji: `gfx_init()`, `gfx_draw_pixel()`, `gfx_flush()`.
- Prymitywy:
  - `gfx_clear(color)` — czyszczenie ekranu.
  - `gfx_draw_line(x0, y0, x1, y1, color)` — algorytm Bresenhama.
  - `gfx_draw_rect(x, y, w, h, color)` i `gfx_fill_rect()`.
  - `gfx_draw_circle(cx, cy, r, color)` — algorytm midpoint.
  - `gfx_draw_triangle(x0, y0, x1, y1, x2, y2, color)`.
  - `gfx_fill_triangle()` — rasteryzacja trójkąta (scanline).
  - `gfx_draw_string(x, y, text, font, color)` — fonty bitmapowe.
- Podwójne buforowanie w PSRAM.
- **Ćwiczenie:** Implementacja biblioteki i demonstracja na ILI9341 SPI 2.2″ oraz ST7789 1.3″.

**Sprzęt:** ILI9341 SPI (7), ST7789 (5).

### Moduł 10.2 — Zaawansowane elementy graficzne
- Rysowanie łuków, zaokrąglonych prostokątów.
- Skalowanie i obracanie bitmap.
- Dekodowanie obrazów: BMP, JPEG (biblioteka TJPGD wbudowana w ESP-IDF).
- Wykresy: liniowe, słupkowe, gauges.
- **Ćwiczenie:** Wyświetlanie obrazu JPEG z Flash na TFT ILI9341.

---

## Faza 11 — LVGL — Wstęp 🔴

**Czas: tydzień 45–47 (3 tygodnie)**

### Moduł 11.1 — Integracja LVGL z ESP-IDF
- Dodanie LVGL jako komponentu IDF (esp_lvgl_port lub ręcznie).
- Konfiguracja `lv_conf.h`: rozmiar bufora, color depth, DPI.
- Display driver: rejestracja `lv_disp_draw_buf_init()`, `lv_disp_drv_init()`.
- Input driver: rejestracja `lv_indev_drv_init()` dla touch XPT2046.
- Tick LVGL: `lv_tick_inc()` z timera, `lv_timer_handler()` w pętli.
- **Ćwiczenie:** Minimalny projekt LVGL — label "Hello World" na ILI9341 3.2″ z touchem.

**Sprzęt:** ILI9341 3.2″ MRB3205 (8) z touch XPT2046.

### Moduł 11.2 — Widgety podstawowe LVGL
- `lv_label`, `lv_btn`, `lv_slider`, `lv_switch`, `lv_bar`.
- `lv_arc`, `lv_spinner`, `lv_checkbox`, `lv_dropdown`.
- Stylizacja: `lv_style_set_*()`, kolory, gradienty, zaokrąglenia, cienie.
- Events: `lv_obj_add_event_cb()`, `LV_EVENT_CLICKED`, `LV_EVENT_VALUE_CHANGED`.
- **Ćwiczenie:** Panel sterujący — suwaki PWM LEDC, przełączniki, wyświetlanie wartości ADC.

**Sprzęt:** ILI9341 3.2″ MRB3205 (8), LED, LDR (31).

### Moduł 11.3 — Layouty i nawigacja LVGL
- Flex layout: `lv_obj_set_flex_flow()`, `lv_obj_set_flex_align()`.
- Grid layout: `lv_obj_set_grid_dsc_array()`.
- Tabview: `lv_tabview_create()` — wiele zakładek.
- Screen management: `lv_scr_load_anim()` — animowane przejścia.
- **Ćwiczenie:** Wieloekranowa aplikacja z zakładkami: Czujniki, Ustawienia, Wykresy.

---

## Faza 12 — LVGL — Zaawansowane 🔴⚫

**Czas: tydzień 48–52 (5 tygodni)**

### Moduł 12.1 — Wykresy i wizualizacja danych
- `lv_chart` — typy: line, bar, scatter.
- Serie danych, auto-zakres, kursor, zoom.
- `lv_meter` — tarcze, wskaźniki (gauge).
- Animacje: `lv_anim_init()`, interpolacja wartości.
- **Ćwiczenie:** Real-time dashboard: wykres akcelerometru (MPU9250), barometr (BMP180), tarcze kompasu.

**Sprzęt:** MPU9250 (17), BMP180 (28), ILI9341 3.2″ (8) lub TFT 4.3″ (9).

### Moduł 12.2 — Obrazy, fonty, motywy
- Konwerter obrazów LVGL (online/offline) → C array.
- Fonty custom: konwerter fontów LVGL, rozmiary, antialising.
- Motywy: `lv_theme_default_init()`, customizacja.
- Dekodowanie obrazów z systemu plików (SPIFFS/SD).
- **Ćwiczenie:** Profesjonalny UI z custom fontami, ikonami i tłem.

### Moduł 12.3 — Animacje i efekty
- `lv_anim_t` — custom animacje właściwości.
- Animacje przejść ekranów (fade, slide, zoom).
- Opa (opacity) animacje, color cycling.
- Gesture recognition: swipe, long press.
- **Ćwiczenie:** Animated splash screen + płynne przejścia między ekranami.

### Moduł 12.4 — LVGL + RTOS: Architektura aplikacji
- Dedykowany task LVGL (`lv_timer_handler()` w pętli).
- Mutex na LVGL (`lv_port_sem_take/give`) — LVGL nie jest thread-safe!
- Wzorzec: taski sensorowe → queue → task LVGL update.
- Optymalizacja: partial refresh, dirty areas, double-buffering.
- Wydajność: profilowanie FPS, optymalizacja rysowania.
- **Ćwiczenie:** Kompletna aplikacja: dashboard z live danymi z 5+ czujników, touch UI, WiFi config, wieloekranowa, animowana.

**Sprzęt:** TFT 4.3″ SSD1963 (9) z touch, MPU9250 (17), BMP180 (28), VL53L0X (24), DHT11 (30), TCS34725 (27).

### Moduł 12.5 — LVGL: SquareLine Studio i prototypowanie
- SquareLine Studio — wizualne projektowanie UI.
- Eksport do kodu C — integracja z ESP-IDF.
- Optymalizacja wygenerowanego kodu.
- **Ćwiczenie:** Zaprojektowanie UI w SquareLine i uruchomienie na ESP32.

---

## Faza 13 — Kamera i przetwarzanie obrazu (bonus) ⚫

**Czas: dodatkowe 2–4 tygodnie (poza głównym planem)**

### Moduł 13.1 — Kamerka OV7670
- Interfejs kamery: SCCB (I2C-like) + równoległy port danych (8-bit).
- Brak sprzętowego DCMI na ESP32 — implementacja przez I2S w trybie camera.
- Konfiguracja rejestrów OV7670: rozdzielczość, format (RGB565, YUV).
- Bufor ramki w PSRAM.
- **Ćwiczenie:** Przechwytywanie obrazu i wyświetlanie na TFT ILI9341.

**Sprzęt:** OV7670 (11), ILI9341 3.2″ (8).

---

## Faza 14 — Zaawansowane czujniki i integracja (bonus) ⚫

**Czas: dodatkowe 2–4 tygodnie (poza głównym planem)**

### Moduł 14.1 — Porównanie IMU
- MPU6050 (15): 6-DOF (akc + żyro), I2C.
- MPU9150 (16): 9-DOF (akc + żyro + mag AK8975), I2C.
- MPU9250 (17): 9-DOF (akc + żyro + mag AK8963), I2C/SPI — **najlepsza jakość**.
- MPU9255 (18): następca MPU9250, niższy pobór prądu.
- ICM20602 (19): 6-DOF, wysoka odporność na wibracje, SPI/I2C.
- L3G4200D (14): 3-axis żyroskop, I2C/SPI.
- MXR9500 (12): akcelerometr analogowy ±4g.
- ADXL330 (13): akcelerometr analogowy 3-osiowy.
- **Ćwiczenie:** Fuzja sensorów — Complementary Filter lub Madgwick na MPU9250, wizualizacja orientacji 3D na TFT.

### Moduł 14.2 — Porównanie czujników odległości
- HC-SR04 / SRF05 (20, 21): ultradźwiękowe, 2–400 cm, dokładność ±3 mm.
- Sharp GP2Y0A41SK0F (22): IR analogowy, 4–30 cm.
- VL6180X (23): ToF I2C, 0–10 cm (wysoka precyzja).
- VL53L0X (24): ToF I2C, 0–200 cm.
- VL53L1X (25): ToF I2C, 0–400 cm, programowalny ROI.
- **Ćwiczenie:** Radar odległości — skanowanie z enkoderem obrotowym, wizualizacja na TFT.

### Moduł 14.3 — Czujnik przeszkody IR FC-51
- Detekcja przeszkody na podczerwień (29).
- Wyjście cyfrowe: GPIO interrupt.
- Regulacja progu detekcji potencjometrem.
- **Ćwiczenie:** Licznik obiektów na linii produkcyjnej (symulacja).

---

## Podsumowanie harmonogramu

```
Tydzień    Faza                                         Poziom
────────── ──────────────────────────────────────────── ──────
 1 –  2    Faza 0: Środowisko, architektura ESP32       🟢
 3 –  6    Faza 1: GPIO, timery, PCNT, LEDC             🟢
 7 –  9    Faza 2: ADC, DAC, SDM                        🟡
10 – 15    Faza 3: UART, I2C, SPI, Flash, I2S           🟡
16 – 19    Faza 4: RMT, MCPWM, Touch, TWAI, SD         🟡
20         Faza 5: PSRAM                                🟡
21 – 28    Faza 6: FreeRTOS (od podstaw → zaawansowany) 🟠
29 – 32    Faza 7: WiFi (STA, AP, HTTP, MQTT, OTA)      🟠
33 – 36    Faza 8: Bluetooth (Classic + BLE)            🟠
37 – 42    Faza 9: Wyświetlacze (od LCD po TFT)        🟠🔴
43 – 44    Faza 10: Własna biblioteka graficzna          🔴
45 – 47    Faza 11: LVGL — wstęp                        🔴
48 – 52    Faza 12: LVGL — zaawansowane                 🔴⚫
+2–4       Faza 13: Kamera OV7670 (bonus)                ⚫
+2–4       Faza 14: Zaawansowane czujniki (bonus)        ⚫
```

---

## Mapa sprzętowa — wykorzystanie modułów

| # | Moduł / Element | Wykorzystanie w fazach |
|---|-----------------|----------------------|
| 1 | ESP32-WROVER-B (NodeMCU-32) | Wszystkie |
| 2 | LCD 2004A równoległy | 9.1 |
| 3 | LCD 2004A I2C (PCF8574) | 9.2 |
| 4 | OLED SSD1306 0.96″ I2C | 3.2, 6.3, 6.8, 7.1, 7.3, 9.3 |
| 5 | IPS ST7789 1.3″ SPI (MSP1308) | 3.3, 9.4, 10.1 |
| 6 | TFT S6D0164 2.2″ 16-bit | 9.5 |
| 7 | TFT ILI9341 2.2″ SPI | 3.3, 9.6, 10.1, 10.2 |
| 8 | TFT ILI9341 3.2″ 16-bit + Touch (MRB3205) | 9.7, 11.1, 11.2, 11.3, 12.1, 13.1 |
| 9 | TFT SSD1963 4.3″ + Touch | 9.8, 12.4 |
| 10 | Graficzny mono T6963C 5.1″ | 9.9 |
| 11 | Kamerka OV7670 | 13.1 |
| 12 | Akcelerometr MXR9500 | 14.1 |
| 13 | Akcelerometr ADXL330 | 14.1 |
| 14 | Żyroskop L3G4200D | 14.1 |
| 15 | IMU MPU6050 | 3.2, 6.3, 6.8, 8.1, 9.3 |
| 16 | IMU MPU9150 | 14.1 |
| 17 | IMU MPU9250 | 6.8, 12.1, 14.1 |
| 18 | IMU MPU9255 | 14.1 |
| 19 | IMU ICM20602 | 14.1 |
| 20 | HC-SR04 | 4.1, 4.2, 6.5, 9.1 |
| 21 | SRF05 | 4.2 |
| 22 | Sharp GP2Y0A41SK0F | 2.1 |
| 23 | VL6180X | 14.2 |
| 24 | VL53L0X | 3.2, 7.3, 12.4, 14.2 |
| 25 | VL53L1X | 14.2 |
| 26 | PIR HC-SR501 | 6.5 |
| 27 | Detektor koloru TCS34725 | 3.2, 9.4 |
| 28 | Czujnik ciśnienia BMP180 | 3.2, 7.3, 8.2, 12.1, 12.4 |
| 29 | Czujnik przeszkody FC-51 | 14.3 |
| 30 | DHT11 | 4.1, 6.6, 8.2, 9.1, 12.4 |
| 31 | LDR | 2.1, 6.1, 11.2 |
| 32 | Czujnik dźwięku LM393 | 6.5 |
| 33 | Czujnik dźwięku LM386 | 2.2, 3.6 |
| 34 | Dżojstik | 2.1 |
| 35 | Enkoder precyzyjny 500 kroków | 1.3 |
| 36 | Enkoder tani | 1.3 |
| 37 | Płytki stykowe | Wszystkie |
| 38 | Kable, rezystory, kondensatory, tranzystory | Wszystkie |

---

## Clock Tree — Podsumowanie

> **Uwaga:** Clock Tree ESP32 jest omawiany kontekstowo przy każdym peryferium, ale poniżej zebrano kluczowe informacje.

- **Źródła zegara:**
  - Crystal oscillator 40 MHz (XTAL) — główny zegar.
  - Internal 8 MHz RC oscillator — backup, deep sleep.
  - Internal 150 kHz RC oscillator — RTC, watchdog.
  - External 32.768 kHz crystal (opcjonalny) — RTC precyzyjny.

- **PLL:**
  - CPU PLL: 160 MHz lub 240 MHz (konfigurowalny w menuconfig).
  - APLL (Audio PLL): programowalny, dla I2S, LCD.
  - SDIO/SDMMC clock divider.

- **Konfiguracja:**
  - `menuconfig` → Component config → ESP32-specific → CPU frequency.
  - Dynamic Frequency Scaling (DFS): `esp_pm_configure()` — automatyczne skalowanie taktowania CPU.
  - Wpływ na peryferia: timer prescalery, SPI clock divider, I2C clock — wszystkie derywowane z APB bus clock (80 MHz przy CPU 160/240 MHz).

---

## Zalecenia dotyczące nauki

1. **Każdy moduł = osobny projekt ESP-IDF.** Nazewnictwo: `faza_XX_modul_YY_nazwa/`.
2. **Dokumentuj postępy** — krótkie notatki po każdym module, co działało, co nie.
3. **Git** — każdy projekt commituj, branch per eksperymenty.
4. **Datasheet'y** — przy każdym nowym czipie/module zaglądaj do datasheeta (masz je w katalogu projektu!).
5. **Oficjalna dokumentacja ESP-IDF** — `docs.espressif.com` — jest doskonała, czytaj API reference i examples.
6. **Nie pomijaj RTOS** — to fundament każdej poważnej aplikacji na ESP32.
7. **PSRAM** — wykorzystuj od początku przy TFT, LVGL zmniejsza to barierę entry.

---

> *Plan wygenerowany 28.02.2026. Dostosuj ramy czasowe do własnego tempa — podane wartości zakładają systematyczną naukę 8-12 godzin tygodniowo.*
