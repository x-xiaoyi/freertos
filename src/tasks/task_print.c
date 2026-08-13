/*
 * task_print.c —— 打印任务实现（资源竞争与互斥锁实验）
 *
 * 设计要点：
 *   - 两个任务都用 putchar 逐字符打印——这是实验的关键！
 *     一次 printf 是"整包发货"，中间没缝隙；逐字符发送则在每个字符
 *     之间都留下一次被调度器抢占的机会，无锁时 A 的一句话随时会被 B 拦腰插断
 *   - USE_MUTEX=0：不拿锁，直接抢串口 → 输出互相插队（混乱）
 *   - USE_MUTEX=1：打印前 osMutexWait 拿钥匙，打完 osMutexRelease 还钥匙 → 输出整齐
 *   - 两任务节奏错开（A:100ms / B:150ms），避免同步后掩盖竞争现象
 */
#include "task_print.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

/* ---- 任务 A ---- */
static void PrintA_Handler(void const *argument)
{
#if USE_MUTEX
    osMutexId mutex = (osMutexId)argument;   /* 万能钥匙：main 递进来的 Mutex 句柄 */
#else
    (void)argument;                          /* 无锁版用不到句柄，防编译警告 */
#endif
    const char *line = "[A] AAAAAAAAAAAAAAAA\r\n";
    for (;;)
    {
#if USE_MUTEX
        osMutexWait(mutex, osWaitForever);   /* 拿钥匙：拿不到就在门口排队 */
#endif
        for (int i = 0; line[i] != '\0'; i++)
        {
            putchar(line[i]);                /* 逐字符发送 */
        }
#if USE_MUTEX
        osMutexRelease(mutex);               /* 还钥匙：谁借的谁还 */
#endif
        osDelay(100);
    }
}

/* ---- 任务 B ---- */
static void PrintB_Handler(void const *argument)
{
#if USE_MUTEX
    osMutexId mutex = (osMutexId)argument;
#else
    (void)argument;
#endif
    const char *line = "[B] bbbbbbbbbbbbbbbbbbbb\r\n";
    for (;;)
    {
#if USE_MUTEX
        osMutexWait(mutex, osWaitForever);
#endif
        for (int i = 0; line[i] != '\0'; i++)
        {
            putchar(line[i]);
        }
#if USE_MUTEX
        osMutexRelease(mutex);
#endif
        osDelay(150);
    }
}

/* 任务定义：与 LED/蜂鸣器任务同一规格 */
osThreadDef(PrintA_Handler, PrintA_Handler, osPriorityNormal, 1, 128);
osThreadDef(PrintB_Handler, PrintB_Handler, osPriorityNormal, 1, 128);

/* ---- 对外接口：创建任务 ---- */
void Task_PrintA_Create(osMutexId mutexId)
{
    osThreadCreate(osThread(PrintA_Handler), mutexId);
}

void Task_PrintB_Create(osMutexId mutexId)
{
    osThreadCreate(osThread(PrintB_Handler), mutexId);
}
