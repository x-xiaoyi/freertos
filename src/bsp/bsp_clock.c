/*
 * bsp_clock.c —— 板级时钟配置实现
 *
 * 克隆板 HSE 晶振可能不起振：直接等 HSERDY 会卡死整个系统。
 * 这里用纯计数循环做显式超时——起振成功走 HSE x PLL = 72MHz，
 * 失败则关闭 HSE 回退 HSI 8MHz（UART 115200 波特率误差约 0.6%，可用）。
 */
#include "bsp_clock.h"

#define HSE_TIMEOUT_LOOPS  200000UL   /* 8MHz 下约 100ms；晶振正常起振只需 ~2ms */

void BSP_Clock_Init(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    uint32_t timeout = HSE_TIMEOUT_LOOPS;
    uint32_t hse_ok  = 0;

    /* 1) 直接操作寄存器打开 HSE 并等 HSERDY
     *    （绕开 HAL 的等待逻辑，防止坏晶振把 HAL 内部超时拖死） */
    RCC->CR |= RCC_CR_HSEON;
    while ((!(RCC->CR & RCC_CR_HSERDY)) && (timeout-- > 0U)) { }

    if (RCC->CR & RCC_CR_HSERDY)
    {
        /* HSE 起振成功：PLL 8MHz x 9 = 72MHz（HAL 内部确认 HSERDY 会立即通过） */
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
        RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
        RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;
        HAL_RCC_OscConfig(&RCC_OscInitStruct);
        hse_ok = 1;
    }
    else
    {
        /* HSE 起振失败：关掉 HSE，回退内部 HSI 8MHz */
        RCC->CR &= ~RCC_CR_HSEON;
    }

    /* 2) 切换系统时钟源（总线时钟随动） */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    if (hse_ok)
    {
        RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
        RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
        RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
        HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
    }
    else
    {
        RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
        RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
        RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
        HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
    }
}
