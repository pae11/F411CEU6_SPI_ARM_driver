```markdown
# F411CEU6 SPI ARM Driver — WeAct Black Pill + e-Paper 2.13"

Проект для **STM32F411CEU6** (WeAct Black Pill V2.0): драйвер трёхцветного e-Paper дисплея (чёрный / белый / красный) с передачей кадрового буфера через **DMA**, управляющими GPIO через **CMSIS-Driver**, сборка через **CMake + Ninja**.

---

## Содержание

1. [Аппаратная часть](#аппаратная-часть)
2. [Структура проекта](#структура-проекта)
3. [Архитектура ПО](#архитектура-по)
4. [Тактирование](#тактирование)
5. [SPI + DMA](#spi--dma)
6. [CMSIS-Driver GPIO](#cmsis-driver-gpio)
7. [Драйвер e-Paper SSD1680](#драйвер-e-paper-ssd1680)
8. [Сборка](#сборка)
9. [Прошивка](#прошивка)
10. [Зависимости](#зависимости)

---

## Аппаратная часть

### Микроконтроллер

| Параметр  | Значение                     |
|-----------|------------------------------|
| МК        | STM32F411CEU6                |
| Ядро      | ARM Cortex-M4F               |
| SYSCLK    | 100 МГц (PLL от HSE 25 МГц) |
| Flash     | 512 КБ                       |
| SRAM      | 128 КБ                       |
| Плата     | WeAct Black Pill V2.0        |

### Дисплей

| Параметр        | Значение                      |
|-----------------|-------------------------------|
| Модуль          | WeAct Studio 2.13" EPD        |
| Контроллер      | SSD1680                       |
| Артикул матрицы | GDEY0213Z98                   |
| Разрешение      | 122 × 250 пикселей            |
| Цвета           | Чёрный / Белый / Красный      |
| Интерфейс       | 4-wire SPI, Mode 0, MSB first |

### Распиновка

| Сигнал EPD | Пин STM32 | Функция                        |
|------------|-----------|--------------------------------|
| DIN (MOSI) | PA7       | SPI1_MOSI (AF5)                |
| CLK (SCK)  | PA5       | SPI1_SCK  (AF5)                |
| CS         | PA4       | GPIO out, LOW = выбран         |
| DC         | PB0       | GPIO out, 0=команда / 1=данные |
| RST        | PB1       | GPIO out, LOW = сброс          |
| BUSY       | PB10      | GPIO in, HIGH = занят          |
| VCC        | 3.3V      | —                              |
| GND        | GND       | —                              |

Встроенный светодиод: **PC13** (активный LOW).

---

## Структура проекта

```
F411CEU6_SPI_ARM_driver/
├── CMakeLists.txt                # Конфигурация сборки
├── toolchain.cmake               # Кросс-компилятор arm-none-eabi-gcc
├── STM32F411CEUX_FLASH.ld        # Linker script
├── Sources/
│   ├── main.c                    # Точка входа, тактирование, LED, EPD_Demo
│   ├── main.h                    # Общие определения и прототипы
│   ├── syscalls.c                # Системные вызовы (newlib)
│   ├── sysmem.c                  # _sbrk для heap
│   └── EPD/
│       ├── epd213.c              # Драйвер SSD1680 (SPI, DMA, init, draw)
│       ├── epd213.h              # Публичный API + константы + распиновка
│       └── font5x7.h             # Bitmap-шрифт 5×7 (ASCII 32–127)
├── Startup/
│   └── startup_stm32f411ceux.s   # Таблица векторов + Reset_Handler
└── CMSIS/
    ├── Core/Include/             # Заголовки CMSIS-Core (fallback)
    ├── Driver/
    │   ├── Include/              # Driver_GPIO.h, Driver_SPI.h, ...
    │   └── ST/STM32F4xx/
    │       └── Source/
    │           └── GPIO_STM32F4xx.c  # Реализация ARM_DRIVER_GPIO
    └── Config/
        └── RTE_Device.h          # Конфигурация RTE
```

---

## Архитектура ПО

```
main()
 ├─ SystemClock_Config()    // PLL: 100 МГц от HSE 25 МГц
 ├─ SysTick_Config()        // 1 мс тик -> delay_ms()
 ├─ LED_Initialize()        // CMSIS GPIO -> PC13
 └─ EPD_Demo()
     ├─ EPD_Init()          // GPIO + SPI1 + DMA2 + SSD1680
     ├─ EPD_Clear()         // Очистка (DMA bulk fill 0xFF)
     ├─ EPD_DrawString(...) // Рендер текста в frame буферы
     ├─ EPD_Display()       // DMA TX -> SSD1680 RAM -> refresh
     └─ EPD_Sleep()         // Deep sleep (~0 мкА)
```

### Путь данных

```
EPD_DrawString()
    |
    v  frame buffers в RAM
epd_bw[4000]  +  epd_red[4000]
    |
    v  EPD_Display()
SPI команды (polling, 1 байт)
SPI данные   (DMA2 Stream3 Ch3, 4000 байт)
    |
    v
SSD1680 RAM -> обновление дисплея
```

---

## Тактирование

| Домен    | Частота   | Конфигурация                          |
|----------|-----------|---------------------------------------|
| HSE      | 25 МГц    | Внешний кварц на плате                |
| PLL VCO  | 200 МГц   | PLLM=25, PLLN=200                     |
| SYSCLK   | 100 МГц   | PLLP=2                                |
| AHB      | 100 МГц   | HPRE=1                                |
| APB1     | 50 МГц    | PPRE1=DIV2                            |
| APB2     | 100 МГц   | PPRE2=1                               |
| SPI1 SCK | 12.5 МГц  | PCLK2 / 8  (BR[2:0] = 010)           |
| Flash    | 3 WS      | Prefetch + I-cache + D-cache включены |
| SysTick  | 1 мс      | SYSCLK / 100 000                      |

---

## SPI + DMA

### SPI1

- Режим: **Master**, Mode 0 (CPOL=0, CPHA=0), 8 бит, MSB first
- Software NSS: SSM=1, SSI=1
- CS управляется вручную через CMSIS GPIO (PA4)
- `SPI_CR2_TXDMAEN=1` — DMA-запрос на TX постоянно активен

### DMA2 Stream3 Channel3 (SPI1_TX)

| Параметр        | Значение               |
|-----------------|------------------------|
| Поток           | DMA2 Stream3           |
| Канал           | 3 (SPI1_TX)            |
| Направление     | Memory -> Peripheral   |
| Размер данных   | 8 бит                  |
| Адрес периферии | &SPI1->DR (фиксирован) |
| Приоритет       | High (PL=10)           |
| FIFO            | Отключён (Direct mode) |
| Circular        | Нет                    |

Два режима отправки:

```c
SPI1_DMA_Send(buf, 0, len);     // передать буфер (MINC=1)
SPI1_DMA_Send(NULL, 0xFF, len); // залить байтом len раз (MINC=0)
```

Одиночные байты команд отправляются в polling-режиме — `TXDMAEN` временно снимается.

---

## CMSIS-Driver GPIO

Используется `ARM_DRIVER_GPIO` вместо прямой работы с регистрами:

```c
extern ARM_DRIVER_GPIO Driver_GPIO_A;  // PA4 — CS
extern ARM_DRIVER_GPIO Driver_GPIO_B;  // PB0 DC, PB1 RST, PB10 BUSY
extern ARM_DRIVER_GPIO Driver_GPIO_C;  // PC13 — LED

Driver_GPIO_A.Setup(4U, NULL);
Driver_GPIO_A.SetDirection(4U, ARM_GPIO_OUTPUT);
Driver_GPIO_A.SetOutputMode(4U, ARM_GPIO_PUSH_PULL);
Driver_GPIO_A.SetOutput(4U, 1U);  // CS HIGH
```

Кодирование пина: `port_index × 16 + bit`  
Примеры: PA4=4, PB10=26, PC13=45

---

## Драйвер e-Paper SSD1680

### Публичный API

```c
// Инициализация (один раз после SystemClock_Config)
void EPD_Init(void);

// Очистка экрана (белый фон + full refresh)
void EPD_Clear(void);

// Вывод кадра
// bw_buf  — BW-слой:  1=белый, 0=чёрный  (EPD_BUFFER_SIZE байт)
// red_buf — Red-слой: 1=красный, 0=нет   (EPD_BUFFER_SIZE байт)
void EPD_Display(const uint8_t *bw_buf, const uint8_t *red_buf);

// Глубокий сон (~0 мкА). Для пробуждения — EPD_Init()
void EPD_Sleep(void);

// Рендер символа в буферы (не отправляет на дисплей)
void EPD_DrawChar(uint8_t *bw_buf, uint8_t *red_buf,
                  int16_t x, int16_t y, char c, uint8_t color);

// Рендер строки
void EPD_DrawString(uint8_t *bw_buf, uint8_t *red_buf,
                    int16_t x, int16_t y, const char *str, uint8_t color);
```

### Цвета пикселей

| Константа         | Значение | Экран   |
|-------------------|----------|---------|
| `EPD_COLOR_BLACK` | 0        | Чёрный  |
| `EPD_COLOR_WHITE` | 1        | Белый   |
| `EPD_COLOR_RED`   | 2        | Красный |

Если `red_buf[bit]=1`, пиксель **красный** независимо от `bw_buf`.

### Геометрия буфера

```
EPD_WIDTH=122, EPD_HEIGHT=250, EPD_BYTES_PER_ROW=16, EPD_BUFFER_SIZE=4000
Байт: buf[row * 16 + col/8]
Бит:  0x80 >> (col % 8)  — MSB = левый пиксель строки
```

### Последовательность обновления

```
EPD_Init:
  HardReset -> SW Reset(0x12) -> Driver Output Control(0x01)
  -> Border Waveform(0x3C) -> Display Update Control(0x21)
  -> Temperature sensor(0x18) -> SetRamWindow -> WaitBusy

EPD_Display:
  SetRamWindow -> Cmd 0x24 -> DMA bw_buf
  -> SetRamWindow -> Cmd 0x26 -> DMA red_buf
  -> Cmd 0x22(0xF7) + Cmd 0x20 (Master Activation) -> WaitBusy
```

---

## Сборка

### Требования

- **arm-none-eabi-gcc 14.2** — 14.2.rel1
- **CMake** >= 3.20
- **Ninja**
- **Git** — для автозагрузки CMSIS через FetchContent

### Команды

```bash
# Конфигурация (Debug)
cmake -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
      -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Сборка
ninja -C build

# Без авто-загрузки CMSIS
cmake -DAUTO_FETCH=OFF -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
      -S . -B build -G Ninja
```

Артефакты: F411CEU6_SPI_ARM_driver.elf / `.hex` / `.bin` / `.map`

---

## Прошивка

```bash
ninja -C build flash
```

Запускает OpenOCD + ST-Link:

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "init" -c "reset halt" \
        -c "flash write_image erase F411CEU6_SPI_ARM_driver.hex" \
        -c "reset" -c "exit"
```

---

## Зависимости

| Пакет           | Версия | Репозиторий                        | Что даёт                        |
|-----------------|--------|------------------------------------|---------------------------------|
| CMSIS_5         | 5.9.0  | ARM-software/CMSIS_5               | core_cm4.h, cmsis_gcc.h         |
| cmsis_device_f4 | master | STMicroelectronics/cmsis_device_f4 | stm32f4xx.h, system_stm32f4xx.c |
| CMSIS-Driver    | 2.9.0  | ARM-software/CMSIS-Driver          | Driver_GPIO.h                   |

Загружаются автоматически при первой конфигурации CMake.  
Локальные копии в CMSIS используются как fallback при `AUTO_FETCH=OFF`.

```sh

```