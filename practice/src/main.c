/*
 * main.c —— 蓝桥杯练习工程：按键 + LED（裸机 super-loop，无 RTOS）
 *
 * 为什么这个工程不用 FreeRTOS：
 *   蓝桥杯嵌入式赛题（CT117E-M4 / STM32G431）的官方框架就是裸机 + 调度器，
 *   赛场上没有 RTOS 可用。这里用同一块 BluePill（F103C8T6）练"20ms 节拍 +
 *   消抖 + 事件回调"这套手法，GPIO/HAL 的写法与 G431 基本同构，迁移成本低。
 *
 * 本文件的职责：
 *   1) HAL_Init + 时钟（HSE 8MHz × 9 = 72MHz，起振失败自动回退 HSI 64MHz）
 *   2) 初始化 LED(PC13) 与按键(PA1)
 *   3) 主循环里维持 20ms 的按键扫描节拍（不阻塞、不用 HAL_Delay）
 *   4) 按键事件 → LED 指示：
 *        短按：LED 常亮/熄灭 切换（并退出闪烁模式）
 *        长按：进入/退出 250ms 快闪模式（表示"长按已被识别"）
 *
 * 注意：本工程与仓库根目录的 FreeRTOS 工程是两个独立的 PlatformIO 工程，
 *       各自有 platformio.ini，互不影响；根目录 `pio run` 不会编译 practice/。
 */
#include "stm32f1xx_hal.h"
#include "key.h"

/* ---- 板载 LED：BluePill 的 PC13，低电平点亮 ---- */
#define LED_PORT        GPIOC
#define LED_PIN         GPIO_PIN_13

/* 闪烁模式的翻转周期（ms） */
#define LED_BLINK_MS    250u

typedef enum
{
    LED_MODE_STEADY = 0,    /* 常亮/常灭，由 s_led_on 决定 */
    LED_MODE_BLINK  = 1     /* 250ms 快闪 */
} led_mode_t;

static led_mode_t s_led_mode = LED_MODE_STEADY;
static uint8_t    s_led_on   = 0;
static uint32_t   s_blink_tick = 0;

/* ---- 时钟配置 ---- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* 首选：HSE 8MHz × PLL9 = 72MHz
     * HAL_RCC_OscConfig 内部对 HSERDY 有 100ms 超时（HSE_TIMEOUT_VALUE），
     * 晶振不起振会返回 HAL_TIMEOUT 而不是死等——克隆板晶振虚焊很常见。 */
    osc.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState        = RCC_HSE_ON;
    osc.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState    = RCC_PLL_ON;
    osc.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL      = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        /* 回退：HSI 8MHz / 2 = 4MHz，× 16 = 64MHz（PLL 上限 72MHz，留余量） */
        RCC_OscInitTypeDef osc_hsi = {0};

        osc_hsi.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
        osc_hsi.HSIState            = RCC_HSI_ON;
        osc_hsi.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        osc_hsi.PLL.PLLState        = RCC_PLL_ON;
        osc_hsi.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
        osc_hsi.PLL.PLLMUL          = RCC_PLL_MUL16;

        if (HAL_RCC_OscConfig(&osc_hsi) != HAL_OK)
        {
            /* 时钟彻底起不来：停在死循环，方便调试器 attach 看 RCC 寄存器 */
            while (1) { }
        }
    }

    /* 总线分频：AHB=SYSCLK，APB1 最高 36MHz 所以 /2，APB2 全速 */
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    /* 72MHz 需要 2 个 Flash 等待周期（FLASH_LATENCY_2） */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        while (1) { }
    }
}

/* ---- LED ---- */
static void LED_Write(uint8_t on)
{
    /* PC13 低电平点亮，所以 on=1 要写 RESET */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void LED_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);

    LED_Write(0);           /* 上电先熄灭 */
}

/* ---- 按键事件回调：驱动在 Key_Scan() 里同步调用，必须短 ---- */
static void On_Key_Event(key_id_t id, key_event_t evt)
{
    (void)id;               /* 目前只有一个按键 */

    if (evt == KEY_EVT_SHORT_PRESS)
    {
        /* 短按：退出闪烁模式，并切换常亮/常灭 */
        s_led_mode = LED_MODE_STEADY;
        s_led_on   = (uint8_t)(!s_led_on);
        LED_Write(s_led_on);
    }
    else if (evt == KEY_EVT_LONG_PRESS)
    {
        /* 长按：进入/退出快闪；进入时立刻点亮，给出即时反馈 */
        if (s_led_mode == LED_MODE_BLINK)
        {
            s_led_mode = LED_MODE_STEADY;
            s_led_on   = 1;
        }
        else
        {
            s_led_mode   = LED_MODE_BLINK;
            s_led_on     = 1;
            s_blink_tick = HAL_GetTick();
        }
        LED_Write(s_led_on);
    }
}

/* ---- 闪烁节拍：非阻塞，靠 HAL_GetTick() 比较时间戳 ---- */
static void LED_Blink_Update(uint32_t now)
{
    if (s_led_mode != LED_MODE_BLINK)
    {
        return;
    }

    if ((uint32_t)(now - s_blink_tick) >= LED_BLINK_MS)
    {
        s_blink_tick = now;
        s_led_on     = (uint8_t)(!s_led_on);
        LED_Write(s_led_on);
    }
}

int main(void)
{
    uint32_t next_scan;

    /* 1) HAL 时基 + 时钟 */
    HAL_Init();
    SystemClock_Config();

    /* 2) 外设 */
    LED_Init();
    Key_Init();
    Key_SetCallback(On_Key_Event);

    /* 3) 主循环：20ms 节拍扫描按键，其余时间空转（以后加 UART/LCD 逻辑） */
    next_scan = HAL_GetTick();

    for (;;)
    {
        uint32_t now = HAL_GetTick();

        if ((uint32_t)(now - next_scan) >= KEY_SCAN_PERIOD_MS)
        {
            next_scan = now;        /* 以实际时间为基准，不累积漂移 */
            Key_Scan();
        }

        LED_Blink_Update(now);
    }
}

/*
 * SysTick 中断：HAL 的时基来源。
 * HAL_Delay / HAL_GetTick 全靠这里把 uwTick 加 1，缺了它 HAL_Delay 会永远卡住。
 * startup_stm32f103xb.s 里 SysTick_Handler 是 weak 的，这里给出强定义覆盖它。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
