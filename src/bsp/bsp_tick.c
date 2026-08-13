/*
 * bsp_tick.c —— SysTick 中断统一入口（HAL 时基 + FreeRTOS 时基）
 *
 * 关键点：FreeRTOS 的 port.c 里有自己的 SysTick 处理函数 xPortSysTickHandler，
 * 而 FreeRTOSConfig.h 默认会把它重命名成 SysTick_Handler，直接顶掉了 HAL 的时基。
 * HAL_Delay() 依赖 SysTick 中断里调用 HAL_IncTick() 来推进 uwTick；
 * 时基被顶掉后 uwTick 永远不涨，HAL_Delay(200) 就再也回不来，
 * 于是 LED 第一次翻转后被点亮，就一直亮着了（BluePill 的 PC13 是低电平点亮）。
 *
 * 修法：不再重命名 FreeRTOS 的处理函数，由我们自己写 SysTick_Handler，
 * 调度器启动前只喂 HAL 时基，启动后两个时基一起喂。
 */
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* port.c 里定义的 RTOS 时基处理函数（FreeRTOSConfig.h 不能再把它宏改名为 SysTick_Handler） */
extern void xPortSysTickHandler(void);

void SysTick_Handler(void)
{
    HAL_IncTick();   /* 喂 HAL 时基：让 HAL_Delay / HAL_GetTick 正常工作 */

    /* 调度器没启动时不要调用 xPortSysTickHandler，避免 RTOS 内核在未就绪时被打扰 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}
