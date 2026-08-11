/*
 * bsp_gpio.c —— 板级 GPIO 初始化实现
 */
#include "bsp_gpio.h"

void BSP_GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);  /* 初始熄灭(高电平) */
}
void BSP_Buzzer_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef Buzzer_Init = {0};
    Buzzer_Init.Pin = GPIO_PIN_0;
    Buzzer_Init.Mode = GPIO_MODE_OUTPUT_PP;
    Buzzer_Init.Pull = GPIO_NOPULL;
    Buzzer_Init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA,&Buzzer_Init);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
}
