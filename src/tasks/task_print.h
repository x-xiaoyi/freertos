/*
 * task_print.h —— 打印任务模块（第5课 Part2：资源竞争与互斥锁实验）
 *
 * 两个任务抢同一个串口：
 *   USE_MUTEX = 0 → 无锁版，输出互相插队（环节三：先看见混乱）
 *   USE_MUTEX = 1 → 互斥锁版，输出整齐完整（环节四：锁住资源）
 */
#ifndef TASK_PRINT_H
#define TASK_PRINT_H

#include "cmsis_os.h"

/* 实验开关：0 = 不加锁（观察混乱）；1 = 加互斥锁（观察整齐） */
#define USE_MUTEX 1

/* 创建并注册打印任务 A / B（内部完成 osThreadDef + osThreadCreate） */
void Task_PrintA_Create(osMutexId mutexId);
void Task_PrintB_Create(osMutexId mutexId);

#endif /* TASK_PRINT_H */
