/*
 * bsp_gpio.h —— 板级 GPIO 初始化
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "stm32f1xx_hal.h"

/* 初始化板载 LED (PC13) 为推挽输出 */
void BSP_GPIO_Init(void);
void BSP_Buzzer_GPIO_Init(void);

#endif /* BSP_GPIO_H */
