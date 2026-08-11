/*
 * main.c —— FreeRTOS 工程化骨架（BluePill / STM32F103C8T6）
 *
 * 职责：板级初始化 → 创建所有任务 → 启动调度器
 *       具体外设配置和任务逻辑分散到 bsp/ 和 tasks/ 目录。
 */

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

/* ---- BSP 模块 ---- */
#include "bsp_clock.h"
#include "bsp_gpio.h"

/* ---- 任务模块 ---- */
#include "task_led.h"
#include "task_buzzer.h"

int main(void)
{
    /* 1) 板级初始化（时钟、外设） */
    HAL_Init();
    BSP_Clock_Init();
    BSP_GPIO_Init();
    BSP_Buzzer_GPIO_Init();

    /* 2) 创建任务 */
    osMessageQDef(queue,5,uint32_t);
    osMessageQId queue = osMessageCreate(osMessageQ(queue),NULL);
    Task_LED_Create(queue);
    Task_Buzzer_Create(queue);

    /* 3) 启动 FreeRTOS 调度器（不再返回） */
    osKernelStart();

    while (1) {}   /* 永远不会执行到这里 */
}
