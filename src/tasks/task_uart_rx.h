/*
 * task_uart_rx.h —— 串口接收演示任务模块
 *
 * 任务循环调用 BSP_UART_ReadByte() 非阻塞读取，
 * 收到的字节以 "[RX] 0x.. 'x'" 的形式打印回串口，用于验证接收通路。
 */
#ifndef TASK_UART_RX_H
#define TASK_UART_RX_H

#include "cmsis_os.h"

/* 创建并注册串口接收演示任务（mutexId 为 NULL 时不加锁） */
void Task_UART_RX_Create(osMutexId mutexId);

#endif /* TASK_UART_RX_H */
