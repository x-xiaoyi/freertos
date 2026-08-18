/*
 * task_led.h —— LED 闪烁任务模块
 *
 * 每个任务模块暴露两个东西：
 *   1. 任务函数  —— 实际执行的业务逻辑（for 循环 + osDelay）
 *   2. Create 函数 —— 封装 osThreadDef + osThreadCreate，main() 只需调一行
 */
#ifndef TASK_LED_H
#define TASK_LED_H

#include "cmsis_os.h"

/* 创建并注册 LED 任务（内部完成 osThreadDef + osThreadCreate） */
void Task_LED_Create(void);

#endif /* TASK_LED_H */