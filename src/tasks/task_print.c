/*
 * task_print.c —— 打印任务实现
 *
 * 设计要点：
 *   - 打印任务逐字符输出状态信息——这是关键！
 *     一次 printf 是"整包发货"，中间没缝隙；逐字符发送则在每个字符
 *     之间都留下一次被调度器抢占的机会，串口回显任务随时可能插队进来
 *   - 打印前 osMutexWait 拿钥匙，打完 osMutexRelease 还钥匙，
 *     和串口回显任务（task_uart_rx.c）互斥使用串口，输出保持整行完整
 */
#include "task_print.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

static void Print_Handler(void const *argument)
{
    osMutexId mutex = (osMutexId)argument;   /* 万能钥匙：main 递进来的 Mutex 句柄 */
    const char *line = "[Print] FreeRTOS on STM32F103 running\r\n";

    for (;;)
    {
        osMutexWait(mutex, osWaitForever);   /* 拿钥匙：拿不到就在门口排队 */
        for (int i = 0; line[i] != '\0'; i++)
        {
            putchar(line[i]);                /* 逐字符发送 */
        }
        osMutexRelease(mutex);               /* 还钥匙：谁借的谁还 */
        osDelay(100);
    }
}

/* 任务定义：与 LED/蜂鸣器任务同一规格 */
osThreadDef(Print_Handler, Print_Handler, osPriorityNormal, 1, 128);

/* ---- 对外接口：创建任务 ---- */
void Task_Print_Create(osMutexId mutexId)
{
    osThreadCreate(osThread(Print_Handler), mutexId);
}
