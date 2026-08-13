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
    osSemaphoreId sem = osSemaphoreCreate(osSemaphore(semaphore),1);

    Task_LED_Create(sem);
    Task_Buzzer_Create(sem);

    /* 互斥锁：保护串口这个共享资源（和信号量一样，资源在 main 创建，
     * 句柄经 argument 递进任务）。
     * USE_MUTEX=0 时任务内不碰锁，传 NULL 即可复现无锁混乱。 */
#if USE_MUTEX
    osMutexDef(printMutex);
    osMutexId print_mutex = osMutexCreate(osMutex(printMutex));
#else
    osMutexId print_mutex = NULL;
#endif

    Task_PrintA_Create(print_mutex);
    Task_PrintB_Create(print_mutex);
    Task_UART_RX_Create(print_mutex);

    /* 3) 启动 FreeRTOS 调度器（不再返回） */
    osKernelStart();

    while (1) {}   /* 永远不会执行到这里 */
}
