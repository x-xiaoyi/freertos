/*
 * task_pipe_sensor.c —— 练习 1 任务 A：采集源（递增计数）实现 (CMSIS-RTOS v1)
 *
 * 职责：每个循环 count++; 然后 osMessagePut(q, count) 直接把计数放入队列。
 *
 * 【你设计核心逻辑】：
 *   1. Task_A 的优先级定多少？(它只是生产者，数据产生别太快冲爆队列)
 *   2. osMessagePut 的第三个 timeout 填什么？
 *      - 填 osWaitForever：队列满就阻塞等，保证每条数据都不会丢(慢消费)
 *      - 填超时值(如 100ms)：满了丢这条，继续产生下一条
 *      - 你倾向哪种？想想满队列时"等"还是"丢"，练这个点。
 *   3. 循环里 osDelay 多久？（决定数据产生速率，会不会快过 C 消费）
 */
#include "task_pipe_sensor.h"
#include "task_pipe.h"

/* ---- 共享队列：q 由 main_pipe.c 创建，经 Create() 传入 ---- */
static osMessageQId s_q = NULL;

/* ---- 任务函数（内部使用） ---- */
static void SensorTask_Handler(void const *argument)
{
    (void)argument;
    pipe_data_t count = 0;

    for (;;)
    {
        count++;                                            /* 递增计数 */

       osMessagePut(s_q,count,osWaitForever); /* TODO：osMessagePut(s_q, (uint32_t)count, <timeout>);
                   把计数放入队列。注意 put 传的是 uint32_t 值。*/

        osDelay(100);/* TODO：osDelay 控制产生速率 */
    }
}

/* ---- 任务定义 ---- */
/* TODO：任务名/函数/优先级/实例数/栈大小。A 只自增+入队，栈不用太大。 */
osThreadDef(SensorTask_Handler, SensorTask_Handler, osPriorityNormal, 1, 128);

/* ---- 对外接口 ---- */
void Task_Pipe_Sensor_Create(osMessageQId q)
{
    s_q = q;
    osThreadCreate(osThread(SensorTask_Handler), NULL);
}
