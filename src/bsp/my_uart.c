/*
 * my_uart.c —— 极简 UART 发送实现（手写练习，不依赖完整 bsp_uart）
 *
 * 步骤：开时钟 → 配引脚 → 配外设波特率/使能 → printf 重定向
 * 当前完成：全部四步
 */

#include "stm32f1xx_hal.h"
#include <stdio.h>

/* ---- 全局 UART 句柄（static：只在本文件可见；_write 函数需要访问它） ---- */
static UART_HandleTypeDef huart1 = {0};

/* ---- ① 开时钟 ---- */
static void My_UART_Clock_Enable(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();     /* GPIOA 端口时钟 */
    __HAL_RCC_USART1_CLK_ENABLE();    /* USART1 外设时钟 */
}

/* ---- ② 配引脚 ---- */
static void My_UART_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* PA9: USART1_TX — 复用推挽，引脚控制权交给 USART1 外设 */
    gpio.Pin   = GPIO_PIN_9;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PA10: USART1_RX — 输入模式 + 上拉（F1 无 AF_INPUT，外设自动读引脚） */
    gpio.Pin   = GPIO_PIN_10;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
}

/* ---- ③ 外设配置 + 初始化 ---- */
void My_UART_Init(void)
{
    My_UART_Clock_Enable();
    My_UART_GPIO_Init();

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;

    HAL_UART_Init(&huart1);

    /* 关闭 stdout 缓冲：让 printf 的每个字符立刻发出去，不攒批 */
    setvbuf(stdout, NULL, _IONBF, 0);
}

/* ---- ④ printf 重定向：所有 printf 字符最终流经 _write ---- */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}