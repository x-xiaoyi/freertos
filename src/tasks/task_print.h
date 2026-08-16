/*
 * task_print.h —— 打印任务模块
 *
 * 打印任务与串口回显任务（task_uart_rx.c）共享同一个互斥锁：
 *   打印任务逐字符输出一行状态信息，回显任务输出收到的字节，
 *   互斥锁保证任何时刻只有一个人在完整地说话，输出不会被拦腰插断。
 */
#ifndef TASK_PRINT_H
#define TASK_PRINT_H

#include "cmsis_os.h"

/* 创建并注册打印任务（内部完成 osThreadDef + osThreadCreate） */
void Task_Print_Create(osMutexId mutexId);

#endif /* TASK_PRINT_H */
