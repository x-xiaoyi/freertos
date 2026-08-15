/*
 * task_uart_rx.c —— 串口接收演示任务实现
 *
 * 设计要点：
 *   - 中断把收到的字节塞进 BSP 的环形缓冲区，任务只负责慢慢取，互不阻塞
 *   - 非阻塞读取 + osDelay，不空转烧 CPU
 *   - 打印时和 PrintA/PrintB 一样拿互斥锁，USE_MUTEX=1 时输出保持整行完整
 */
#include "task_uart_rx.h"
#include "bsp_uart.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

static void UartRx_Handler(void const *argument)
{
    osMutexId mutex = (osMutexId)argument;
    uint8_t ch;
    uint16_t round = 0;

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
        round ++;
        if(round >= 200)
        {
            round = 0;
            if(mutex != NULL)osMutexWait(mutex,osWaitForever);
            printf("[stats]total receive: %u bytes\r\n",BSP_UART_GetRxCount());
            if(mutex != NULL)osMutexRelease(mutex);
        }
        osDelay(5);
    }
}

osThreadDef(UartRx_Handler, UartRx_Handler, osPriorityNormal, 1, 128);

void Task_UART_RX_Create(osMutexId mutexId)
{
    osThreadCreate(osThread(UartRx_Handler), mutexId);
}