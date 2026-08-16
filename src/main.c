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
#include "bsp_uart.h"
#include "bsp_params.h"

/* ---- 任务模块 ---- */
#include "task_led.h"
#include "task_buzzer.h"
#include "task_print.h"
#include "task_uart_rx.h"

int main(void)
{
    /* 1) 板级初始化（时钟、外设） */
    HAL_Init();
    BSP_Clock_Init();
    BSP_GPIO_Init();
    BSP_Buzzer_GPIO_Init();
    BSP_UART_Init();

    /* 串口通路验证：开机问候。烧录后若串口助手能收到这句话，说明接线没问题 */
    printf("[Boot] BluePill UART ready\r\n");

    /* 2) 创建任务 */

    osSemaphoreDef(semaphore);
    osMutexDef(printmutex);
    task_params_t params;
    params.semId = osSemaphoreCreate(osSemaphore(semaphore),1);
    params.print_Mutex = osMutexCreate(osMutex(printmutex));


    Task_LED_Create(params.semId);
    Task_Buzzer_Create(params.semId);
    Task_Print_Create(params.print_Mutex);
    Task_UART_RX_Create(&params);

    /* 3) 启动 FreeRTOS 调度器（不再返回） */
    osKernelStart();

    while (1) {}   /* 永远不会执行到这里 */
}