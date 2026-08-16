/*
 * task_uart_rx.h —— 串口接收演示任务模块
 *
 * 任务循环调用 BSP_UART_ReadByte() 非阻塞读取，
 * 收到的字节以 "[RX] 0x.. 'x'" 的形式打印回串口，用于验证接收通路。
 * 打印时和打印任务共用同一个互斥锁（句柄经 task_params_t 打包传入）。
 */
#ifndef TASK_UART_RX_H
#define TASK_UART_RX_H

#include "cmsis_os.h"
#include "bsp_params.h"

/* 创建并注册串口接收演示任务（内部完成 osThreadDef + osThreadCreate） */
void Task_UART_RX_Create(task_params_t *params);

#endif /* TASK_UART_RX_H */
