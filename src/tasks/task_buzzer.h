/*
 * task_buzzer.h —— 蜂鸣器任务模块
 *
 * 每个任务模块暴露两个东西：
 *   1. 任务函数  —— 实际执行的业务逻辑（for 循环 + osDelay）
 *   2. Create 函数 —— 封装 osThreadDef + osThreadCreate，main() 只需调一行
 *
 * 注意：BluePill 无板载蜂鸣器，需外接有源蜂鸣器到 BUZZER_PIN 对应引脚
 */
#ifndef TASK_BUZZER_H
#define TASK_BUZZER_H
#define CMD_Buzzer 0x0001

#include "cmsis_os.h"

/* 创建并注册蜂鸣器任务（内部完成 osThreadDef + osThreadCreate） */
void Task_Buzzer_Create(osSemaphoreId sem);

#endif /* TASK_BUZZER_H */
