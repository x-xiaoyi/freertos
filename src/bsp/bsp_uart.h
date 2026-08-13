/*
 * bsp_uart.h —— 板级串口初始化（USART1，调试打印用）
 *
 * 接线：PA9 (TX) → USB-TTL 的 RX；PA10 (RX) → USB-TTL 的 TX；GND 共地
 * 串口助手设置：115200, 8 数据位, 无校验, 1 停止位
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#include "stm32f1xx_hal.h"
#include <stdio.h>

/* 初始化 USART1：PA9 作 TX、PA10 作 RX，115200 波特率 */
void BSP_UART_Init(void);

/* 非阻塞读取：有数据返回 1 并写入 *out，无数据返回 0 */
uint8_t BSP_UART_ReadByte(uint8_t *out);

#endif /* BSP_UART_H */
