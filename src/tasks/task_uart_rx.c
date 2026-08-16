/*
 * task_uart_rx.c —— 串口接收演示任务实现
 *
 * 设计要点：
 *   - 中断把收到的字节塞进 BSP 的环形缓冲区，任务只负责慢慢取，互不阻塞
 *   - 非阻塞读取 + osDelay，不空转烧 CPU
 *   - 回显时和打印任务（task_print.c）一样拿互斥锁，
 *     句柄由 main 打包在 task_params_t 里一起递进来
 */
#include "task_uart_rx.h"
#include "bsp_uart.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include "bsp_params.h"

static void UartRx_Handler(void const *argument)
{
    task_params_t *p = (task_params_t *)argument;
    osMutexId mutex = p->print_Mutex;
    uint8_t ch;

    for (;;)
    {
        /* 一次把缓冲区里攒的字节全部读出来 */
        while (BSP_UART_ReadByte(&ch))
        {
            if (mutex != NULL)
            {
                osMutexWait(mutex, osWaitForever);
            }
            printf("[RX] 0X%02X '%c'\r\n", ch, ch);
            if (mutex != NULL)
            {
                osMutexRelease(mutex);
            }
        }
        osDelay(5);
    }
}

osThreadDef(UartRx_Handler, UartRx_Handler, osPriorityNormal, 1, 128);

void Task_UART_RX_Create(task_params_t *params)
{
    osThreadCreate(osThread(UartRx_Handler), params);
}
