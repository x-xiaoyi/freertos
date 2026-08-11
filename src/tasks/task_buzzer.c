/*
 * task_buzzer.c —— 蜂鸣器任务实现
 *
 * 设计要点：
 *   - 任务函数 BuzzerTask_Handler 是 static，外部不需要知道
 *   - osThreadDef 也放在 .c 内部，不污染头文件
 *   - 外部只需调用 Task_Buzzer_Create() 一行
 *   - BluePill 无板载蜂鸣器，需外接有源蜂鸣器到 BUZZER_PORT/PIN
 */
#include "task_buzzer.h"
#include "stm32f1xx_hal.h"
static void Buzzer_Handle(void const *argument)
{
    osMessageQId queue = (osMessageQId)argument;
    osEvent evn;
    for(;;)
    {
        evn = osMessageGet(queue, osWaitForever);
        if(evn.status == osEventMessage)
        {
            uint32_t message = evn.value.v;
            uint16_t cmd = message>>16;
            uint16_t param = message&0xFFFF;
            if(cmd == CMD_Buzzer)
            {
                for(int n=0;n<param;n++)
                {
                    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_0,GPIO_PIN_SET);
                    osDelay(100);
                    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_0,GPIO_PIN_RESET);
                    osDelay(100);
                }
            }
        }
    }
}
osThreadDef(Buzzer_Handle,Buzzer_Handle,osPriorityNormal,1,128);

/* ---- 对外接口：创建任务 ---- */
void Task_Buzzer_Create(osMessageQId queue)
{
    osThreadCreate(osThread(Buzzer_Handle), queue);
}
