/*
 * task_led.c —— LED 闪烁任务实现
 *
 * 设计要点：
 *   - 任务函数 LedTask_Handler 是 static，外部不需要知道
 *   - osThreadDef 也放在 .c 内部，不污染头文件
 *   - 外部只需调用 Task_LED_Create() 一行
 */
#include "task_led.h"
#include "stm32f1xx_hal.h"

/* ---- 任务函数（内部使用） ---- */
static void LedTask_Handler(void const *argument)
{
    (void)argument;   /* LED 任务不需要外部参数 */
    static TickType_t lastwaketime;
    lastwaketime = xTaskGetTickCount();
    for (;;)
    {
        HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);   /* BluePill LED 低电平点亮 */
        vTaskDelayUntil(&lastwaketime,500);      /* 任务级延时，让出 CPU */
    }
}

/* 任务定义：名称, 函数, 优先级, 实例数, 栈大小(单位: word=4字节) */
osThreadDef(LedTask_Handler, LedTask_Handler, osPriorityNormal, 1, 128);

/* ---- 对外接口：创建任务 ---- */
void Task_LED_Create(void)
{
    osThreadCreate(osThread(LedTask_Handler), NULL);
}