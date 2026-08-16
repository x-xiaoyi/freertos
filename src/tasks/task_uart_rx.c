
#include "task_uart_rx.h"
#include "bsp_uart.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include "bsp_params.h"

static void UartRx_Handler(void const *argument)
{
    task_params_t *p = (task_params_t *)argument;
    osMutexId mutex = p->print_Mutex;
    osSemaphoreId semaphoreId = p->semId;
    uint8_t ch;
    for (;;)
    {   
        if(BSP_UART_ReadByte(&ch))
        {
            int key = ch;
            switch(key)
            {
                case '1':
                    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_RESET);
                    break;
                case '0':
                    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_SET);
                    break;
                case 'b':
                    osSemaphoreRelease(semaphoreId);
                    break;
                default:
                    osMutexWait(mutex, osWaitForever);
                    printf("Unknown command: '%c'\r\n",ch);
                    osMutexRelease(mutex);
                    break;
            }
        }
        osDelay(5);
    }
}

osThreadDef(UartRx_Handler, UartRx_Handler, osPriorityNormal, 1, 258);

void Task_UART_RX_Create(task_params_t *params)
{
    osThreadCreate(osThread(UartRx_Handler), params);
}
