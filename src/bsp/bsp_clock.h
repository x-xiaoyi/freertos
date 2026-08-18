/*
 * bsp_clock.h —— 板级时钟配置
 */
#ifndef BSP_CLOCK_H
#define BSP_CLOCK_H

#include "stm32f1xx_hal.h"

/* 初始化系统时钟：HSE 8MHz × PLL9 = 72MHz；HSE 起振失败自动回退 HSI 8MHz */
void BSP_Clock_Init(void);

#endif /* BSP_CLOCK_H */
