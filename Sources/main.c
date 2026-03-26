#include "main.h"
#include <stdio.h>

/* PC13: порт C (индекс 2) * 16 + 13 = 45 */
#define PC13_PIN    45U

/* Получаем доступ к драйверу GPIO порта C */
extern ARM_DRIVER_GPIO Driver_GPIO_C;
static ARM_DRIVER_GPIO *gpioc;

/* SysTick счётчик для delay_ms */
static volatile uint32_t sys_tick_count = 0U;

/* ------------------------------------------------------------------ */
/* SysTick Handler                                                     */
/* ------------------------------------------------------------------ */
void SysTick_Handler(void)
{
    sys_tick_count++;
}


/**
  * @brief  Основная функция программы
  * @retval int
  */
int main(void)
{
    /* Настраиваем систему тактирования */
    SystemClock_Config();

    /* Настраиваем SysTick на 1 мс при 100 МГц */
    SysTick_Config(SYSTEM_CORE_CLOCK / 1000U);

    /* Инициализируем светодиод */
    LED_Initialize();

    /* Инициализируем и демонстрируем e-Paper дисплей */
    EPD_Demo();

    /* Инициализируем DS1620 */
    DS1620_Init();

    /* Основной цикл программы */
    while (1)
    {
        /* Включаем светодиод (активный LOW на PC13) */
        gpioc->SetOutput(PC13_PIN, 0U);
        delay_ms(LED_BLINK_INTERVAL);

        /* Выключаем светодиод */
        gpioc->SetOutput(PC13_PIN, 1U);
        delay_ms(LED_BLINK_INTERVAL);

        /* Считываем температуру каждые 2 с */
        Temp_Demo();
    }
}

/**
  * @brief  Считать температуру DS1620 и отобразить на e-Paper (only temperature, big font)
  * @retval None
  */
void Temp_Demo(void)
{
    static uint8_t bw[EPD_BUFFER_SIZE];
    static uint8_t red[EPD_BUFFER_SIZE];

    /* ── Считываем температуру ───────────────────────────── */
    float temperature = 0.0f;
    DS1620_Status status = DS1620_ReadTemp(&temperature);

    /* ── Формируем строку температуры ──────────────────── */
    /*
     * Scale 4x: each glyph = 20 px wide (5*4) + 4 px gap = 24 px step.
     * Display width = 122 px. Max chars that fit: 122/24 = 5 → "-99.5" fits.
     * Vertical: 7*4 = 28 px tall. Centered vertically: (250-28)/2 = 111.
     */
    char temp_str[10];
    if (status == DS1620_OK)
    {
        int16_t raw_x2;
        if (temperature >= 0.0f)
            raw_x2 = (int16_t)(temperature * 2.0f + 0.25f);
        else
            raw_x2 = (int16_t)(temperature * 2.0f - 0.25f);

        uint8_t  neg  = (raw_x2 < 0) ? 1U : 0U;
        uint16_t abs2 = (uint16_t)(neg ? -raw_x2 : raw_x2);
        uint16_t deg  = abs2 / 2U;
        uint8_t  frac = (uint8_t)((abs2 % 2U) * 5U);

        snprintf(temp_str, sizeof(temp_str), "%s%u.%u",
                 neg ? "-" : "+", (unsigned)deg, (unsigned)frac);
    }
    else
    {
        snprintf(temp_str, sizeof(temp_str), "Err");
    }

    /* ── Белый фон ────────────────────────────────────── */
    for (uint32_t i = 0U; i < EPD_BUFFER_SIZE; i++)
    {
        bw[i]  = 0xFFU;
        red[i] = 0x00U;
    }

    /*
     * Layout (display is 122 wide × 250 tall, portrait):
     *
     *   y=  2   "DS1620"  small black label
     *   y= 14   thin separator line
     *   y= 20   big temperature value (scale=4, 28 px tall) in RED
     *   y= 52   thin separator line
     *   y= 58   "°C"  small label
     */

    /* Label */
    EPD_DrawString(bw, red, 2, 2, "DS1620", EPD_COLOR_BLACK);

    /* Separator */
    for (uint32_t col = 0; col < EPD_BYTES_PER_ROW; col++)
        bw[13U * EPD_BYTES_PER_ROW + col] = 0x00U;

    /* Big temperature value, scale=4 */
    EPD_DrawString_Big(bw, red, 2, 20, temp_str, EPD_COLOR_RED, 4U);

    /* Separator */
    for (uint32_t col = 0; col < EPD_BYTES_PER_ROW; col++)
        bw[51U * EPD_BYTES_PER_ROW + col] = 0x00U;

    /* Unit label */
    EPD_DrawString(bw, red, 2, 55, "deg C", EPD_COLOR_BLACK);

    /* ── Выводим на e-Paper ───────────────────────────── */
    EPD_Init();
    EPD_Display(bw, red);
    EPD_Sleep();
}

/**
  * @brief  Инициализация светодиода на PC13
  * @retval None
  */
void LED_Initialize(void)
{
    /* Получаем указатель на драйвер GPIOC */
    gpioc = &Driver_GPIO_C;

    /* Инициализируем пин (тактирование порта включается внутри Setup) */
    gpioc->Setup(PC13_PIN, NULL);

    /* Настраиваем PC13 как выход */
    gpioc->SetDirection(PC13_PIN, ARM_GPIO_OUTPUT);

    /* Тип выхода: push-pull */
    gpioc->SetOutputMode(PC13_PIN, ARM_GPIO_PUSH_PULL);

    /* Без подтяжки */
    gpioc->SetPullResistor(PC13_PIN, ARM_GPIO_PULL_NONE);

    /* Начальное состояние: светодиод выключен (HIGH) */
    gpioc->SetOutput(PC13_PIN, 1U);
}

/**
  * @brief  Настройка системного тактирования 100 МГц от HSE 25 МГц
  *         HSE=25, PLLM=25, PLLN=200, PLLP=2 → VCO=200 МГц, SYSCLK=100 МГц
  * @retval None
  */
void SystemClock_Config(void)
{
    /* Включаем HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* Настраиваем Flash Prefetch + задержку 3 WS для 100 МГц */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_3WS;

    /* APB1 = SYSCLK/2 (макс 50 МГц), APB2 = SYSCLK (макс 100 МГц) */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

    /* Настраиваем PLL: PLLM=25, PLLN=200, PLLP=2, источник HSE */
    RCC->PLLCFGR = (25U  << RCC_PLLCFGR_PLLM_Pos) |   /* PLLM = 25  */
                   (200U << RCC_PLLCFGR_PLLN_Pos) |    /* PLLN = 200 */
                   (0U   << RCC_PLLCFGR_PLLP_Pos) |    /* PLLP = 2   */
                   RCC_PLLCFGR_PLLSRC_HSE;

    /* Включаем PLL */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* Переключаем SYSCLK на PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    /* Обновляем переменную SystemCoreClock */
    SystemCoreClockUpdate();
}

/**
  * @brief  Задержка в миллисекундах (на основе SysTick)
  * @param  ms: количество миллисекунд
  * @retval None
  */
void delay_ms(uint32_t ms)
{
    uint32_t start = sys_tick_count;
    while ((sys_tick_count - start) < ms);
}

/**
  * @brief  Демонстрация e-Paper дисплея:
  *         1. Очистка (белый фон)
  *         2. Горизонтальные полосы (10 чёрных / белых групп по 25 строк)
  *         3. Перевод в глубокий сон
  * @retval None
  */
void EPD_Demo(void)
{
    static uint8_t epd_bw[EPD_BUFFER_SIZE];   /* BW layer:  1=white, 0=black */
    static uint8_t epd_red[EPD_BUFFER_SIZE];  /* Red layer: 1=red,   0=none  */

    /* Инициализация контроллера SSD1680 */
    EPD_Init();

    /* Очистка — обязательна перед первым показом */
    EPD_Clear();

    /* Белый фон: BW=0xFF (white), Red=0x00 (no red) */
    for (uint32_t i = 0U; i < EPD_BUFFER_SIZE; i++)
    {
        epd_bw[i]  = 0xFFU;
        epd_red[i] = 0x00U;
    }

    /* ── Заголовок: КРАСНЫЙ текст ──────────────────────────────── */
    EPD_DrawString(epd_bw, epd_red,  2,  2, "WeAct Studio",       EPD_COLOR_RED);
    EPD_DrawString(epd_bw, epd_red,  2, 12, "2.13\" EPD B/W/RED",  EPD_COLOR_RED);

    /* ── Разделитель: чёрная линия ─────────────────────────────── */
    for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
    {
        epd_bw[22U * EPD_BYTES_PER_ROW + col] = 0x00U;
    }

    /* ── Основной текст: чёрный ────────────────────────────────── */
    EPD_DrawString(epd_bw, epd_red,  2, 26, "STM32F411CEU6",      EPD_COLOR_BLACK);
    EPD_DrawString(epd_bw, epd_red,  2, 36, "100MHz  512K Flash", EPD_COLOR_BLACK);
    EPD_DrawString(epd_bw, epd_red,  2, 48, "SPI1 @ 12.5 MHz",   EPD_COLOR_BLACK);

    /* ── Разделитель ────────────────────────────────────────────── */
    for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
    {
        epd_bw[60U * EPD_BYTES_PER_ROW + col] = 0x00U;
    }

    /* ── Цифры и символы: красный ───────────────────────────────── */
    EPD_DrawString(epd_bw, epd_red,  2, 64, "0123456789",         EPD_COLOR_RED);
    EPD_DrawString(epd_bw, epd_red,  2, 74, "Hello, World!",      EPD_COLOR_BLACK);
    EPD_DrawString(epd_bw, epd_red,  2, 84, "Red + Black + White",EPD_COLOR_RED);

    /* ── Инвертированный блок: белый текст на чёрном ───────────── */
    for (uint32_t row = 96U; row < 120U; row++)
    {
        for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
        {
            epd_bw[row * EPD_BYTES_PER_ROW + col] = 0x00U;
        }
    }
    EPD_DrawString(epd_bw, epd_red,  2, 100, "White on Black",    EPD_COLOR_WHITE);
    EPD_DrawString(epd_bw, epd_red,  2, 110, "STM32 CMSIS GPIO",  EPD_COLOR_WHITE);

    /* Вывод изображения */
    EPD_Display(epd_bw, epd_red);

    /* Переводим дисплей в спящий режим */
    EPD_Sleep();
}