#include "main.h"
#include <stdio.h>

/* PC13: порт C (индекс 2) * 16 + 13 = 45 */
#define PC13_PIN    45U

/* GPIO порт C (LED) */
extern ARM_DRIVER_GPIO Driver_GPIO_C;
static ARM_DRIVER_GPIO *gpioc;

/* GPIO порт A (relay PA6) */
extern ARM_DRIVER_GPIO Driver_GPIO_A;
static ARM_DRIVER_GPIO *gpioa_main;

/* Состояние реле: 0=выкл, 1=вкл */
static uint8_t relay_on = 0U;

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

    /* Инициализируем выход реле (PA6) */
    Relay_Initialize();

    /* Инициализируем DS1620 */
    DS1620_Init();

    /* Инициализируем e-Paper один раз при старте */
    EPD_Init();

    /* Первое отображение температуры сразу при старте */
    Temp_Demo();
    uint32_t last_update = sys_tick_count;

    /* Основной цикл программы */
    while (1)
    {
        /* Мигаем светодиодом (heartbeat) */
        gpioc->SetOutput(PC13_PIN, 0U);
        delay_ms(LED_BLINK_INTERVAL);
        gpioc->SetOutput(PC13_PIN, 1U);
        delay_ms(LED_BLINK_INTERVAL);

        /* Обновляем каждые 5 секунд */
        if ((sys_tick_count - last_update) >= 5000U)
        {
            /* ── Термостатная логика с гистерезисом ── */
            float cur_temp = 0.0f;
            if (DS1620_ReadTemp(&cur_temp) == DS1620_OK)
            {
                if (!relay_on && cur_temp < (TEMP_SETPOINT - TEMP_HYST * 0.5f))
                    relay_on = 1U;
                else if (relay_on && cur_temp > (TEMP_SETPOINT + TEMP_HYST * 0.5f))
                    relay_on = 0U;
            }
            gpioa_main->SetOutput(RELAY_PIN, relay_on);

            Temp_Demo();
            last_update = sys_tick_count;
        }
    }
}

/**
  * @brief  Термостатный дисплей: температура + уставка + статус реле (HEAT ON/OFF)
  *
  * Раскладка экрана (122×250 px, портрет):
  *   y=  2  "DS1620"           RED  (статично)
  *   y= 13  ─────────────────  разделитель
  *   y= 20  +23.50             BLACK scale=3  ← динамично (зона А: строки 14-43)
  *   y= 44  ─────────────────  разделитель
  *   y= 48  "deg C"            RED  (статично)
  *   y= 65  ─────────────────  разделитель
  *   y= 70  "SET +22.0C"       RED  (статично)
  *   y= 83  ─────────────────  разделитель
  *   y= 88  "HEAT:"            RED  (статично)
  *   y= 99  ─────────────────  разделитель
  *   y=104  ON  / OFF          BLACK scale=3  ← динамично (зона Б: строки 100-128)
  *   y=129  ─────────────────  разделитель
  *
  * @retval None
  */
void Temp_Demo(void)
{
    static uint8_t bw_prev[EPD_BUFFER_SIZE];
    static uint8_t bw_new[EPD_BUFFER_SIZE];
    static uint8_t red[EPD_BUFFER_SIZE];
    static uint8_t initialized = 0U;

    /* ── Текущая температура ────────────────────────────────── */
    float temperature = 0.0f;
    DS1620_Status status = DS1620_ReadTemp(&temperature);

    char temp_str[10];
    if (status == DS1620_OK)
    {
        int16_t raw_x2 = (temperature >= 0.0f)
            ? (int16_t)(temperature * 2.0f + 0.25f)
            : (int16_t)(temperature * 2.0f - 0.25f);
        uint8_t  neg  = (raw_x2 < 0) ? 1U : 0U;
        uint16_t abs2 = (uint16_t)(neg ? -raw_x2 : raw_x2);
        snprintf(temp_str, sizeof(temp_str), "%s%u.%02u",
                 neg ? "-" : "+", (unsigned)(abs2 / 2U), (unsigned)((abs2 % 2U) * 50U));
    }
    else
    {
        snprintf(temp_str, sizeof(temp_str), "Err");
    }

    /* ── Строка уставки (из compile-time константы) ─────────── */
    char set_str[20];
    {
        int16_t sp_x2 = (TEMP_SETPOINT >= 0.0f)
            ? (int16_t)(TEMP_SETPOINT * 2.0f + 0.25f)
            : (int16_t)(TEMP_SETPOINT * 2.0f - 0.25f);
        uint8_t  sp_neg  = (sp_x2 < 0) ? 1U : 0U;
        uint16_t sp_abs2 = (uint16_t)(sp_neg ? -sp_x2 : sp_x2);
        snprintf(set_str, sizeof(set_str), "SET %s%u.%1uC",
                 sp_neg ? "-" : "+",
                 (unsigned)(sp_abs2 / 2U),
                 (unsigned)((sp_abs2 % 2U) * 5U));
    }

    /* ── Статус нагрева ─────────────────────────────────────── */
    const char *relay_str = relay_on ? "ON " : "OFF";

    if (!initialized)
    {
        /* ── Полный рефреш при старте ─────────────────────────── */
        for (uint32_t i = 0U; i < EPD_BUFFER_SIZE; i++)
        {
            bw_prev[i] = 0xFFU;
            red[i]     = 0x00U;
        }

        /* Статические RED-метки */
        EPD_DrawString(bw_prev, red, 2,  2, "DS1620", EPD_COLOR_RED);
        EPD_DrawString(bw_prev, red, 2, 48, "deg C",  EPD_COLOR_RED);
        EPD_DrawString(bw_prev, red, 2, 70, set_str,  EPD_COLOR_RED);
        EPD_DrawString(bw_prev, red, 2, 88, "HEAT:",  EPD_COLOR_RED);

        /* Разделительные линии */
        for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
        {
            bw_prev[ 13U * EPD_BYTES_PER_ROW + col] = 0x00U;
            bw_prev[ 44U * EPD_BYTES_PER_ROW + col] = 0x00U;
            bw_prev[ 65U * EPD_BYTES_PER_ROW + col] = 0x00U;
            bw_prev[ 83U * EPD_BYTES_PER_ROW + col] = 0x00U;
            bw_prev[ 99U * EPD_BYTES_PER_ROW + col] = 0x00U;
            bw_prev[129U * EPD_BYTES_PER_ROW + col] = 0x00U;
        }

        /* Динамический контент — первый рендер */
        EPD_DrawString_Big(bw_prev, red, 2,  20, temp_str,  EPD_COLOR_BLACK, 3U);
        EPD_DrawString_Big(bw_prev, red, 2, 104, relay_str, EPD_COLOR_BLACK, 3U);

        EPD_Display(bw_prev, red);
        initialized = 1U;
    }
    else
    {
        /* ── Дифференциальный апдейт ──────────────────────────── */

        /* 1. Копируем предыдущий кадр */
        for (uint32_t i = 0U; i < EPD_BUFFER_SIZE; i++)
            bw_new[i] = bw_prev[i];

        /* 2. Очищаем зоны динамического контента (разделители остаются нетронутыми) */
        for (uint32_t row = 14U; row <= 43U; row++)          /* зона А: температура */
            for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
                bw_new[row * EPD_BYTES_PER_ROW + col] = 0xFFU;

        for (uint32_t row = 100U; row <= 128U; row++)         /* зона Б: реле ON/OFF */
            for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
                bw_new[row * EPD_BYTES_PER_ROW + col] = 0xFFU;

        /* 3. Рендерим новые строки */
        EPD_DrawString_Big(bw_new, red, 2,  20, temp_str,  EPD_COLOR_BLACK, 3U);
        EPD_DrawString_Big(bw_new, red, 2, 104, relay_str, EPD_COLOR_BLACK, 3U);

        /* 4. XOR: ищем первую и последнюю изменившуюся строку */
        uint16_t row_first = 0xFFFFU;
        uint16_t row_last  = 0U;

        for (uint32_t row = 14U; row <= 130U; row++)
        {
            for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
            {
                if ((bw_prev[row * EPD_BYTES_PER_ROW + col] ^
                     bw_new [row * EPD_BYTES_PER_ROW + col]) != 0U)
                {
                    if (row < (uint32_t)row_first) row_first = (uint16_t)row;
                    if (row > (uint32_t)row_last)  row_last  = (uint16_t)row;
                    break;
                }
            }
        }

        /* 5. Обновляем только изменившиеся строки */
        if (row_first <= row_last)
        {
            EPD_PartialUpdate(bw_new + row_first * EPD_BYTES_PER_ROW,
                              row_first, row_last);

            for (uint32_t row = row_first; row <= (uint32_t)row_last; row++)
                for (uint32_t col = 0U; col < EPD_BYTES_PER_ROW; col++)
                    bw_prev[row * EPD_BYTES_PER_ROW + col] =
                        bw_new[row * EPD_BYTES_PER_ROW + col];
        }
    }
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
  * @brief  Инициализация выхода реле на PA6 (active HIGH = нагрев включён)
  * @retval None
  */
void Relay_Initialize(void)
{
    gpioa_main = &Driver_GPIO_A;
    gpioa_main->Setup(RELAY_PIN, NULL);
    gpioa_main->SetDirection(RELAY_PIN, ARM_GPIO_OUTPUT);
    gpioa_main->SetOutputMode(RELAY_PIN, ARM_GPIO_PUSH_PULL);
    gpioa_main->SetPullResistor(RELAY_PIN, ARM_GPIO_PULL_NONE);
    gpioa_main->SetOutput(RELAY_PIN, 0U); /* реле выключено при старте */
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
