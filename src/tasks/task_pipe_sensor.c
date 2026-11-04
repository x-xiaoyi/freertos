/*
 * task_pipe_sensor.c —— 队列流水线：生产端（采集源）任务
 *
 * 职责：每 PIPE_SENSOR_PERIOD_MS 生成一个递增值，osMessagePut 丢进队列，
 *       并统计"成功入队 / 因队列满被丢弃"的次数。
 *
 * 设计说明（三处拍脑袋的参数，这里给出量化依据）：
 *
 * 1) 优先级：osPriorityBelowNormal
 *    cmsis_os.c 里 makeFreeRtosPriority() 的换算是
 *        FreeRTOS 优先级 = osPriority + 3
 *    所以本任务 = 2，打印/消费者/回显任务（osPriorityNormal）= 3。
 *    生产者必须比消费者低：
 *      - 消费者醒着的时候，它需要立刻把队列里的东西搬走、把空间腾出来；
 *      - 如果生产者优先级 >= 消费者，队列一满，生产者用 osWaitForever 等空位，
 *        而消费者又抢不到 CPU 去取数据 → 直接死锁；
 *      - 即使生产者用"满了就丢"，高优先级生产者也会以 10Hz 稳定打断
 *        消费者正在做的 printf，把串口输出切碎。
 *    低一级 + osDelay 主动让出，生产者只在需要时醒一下，天然不抢戏。
 *
 * 2) osMessagePut 的 timeout：0（满了就丢，绝不长时间阻塞）
 *    权衡：
 *      - osWaitForever（队列满就一直睡）：一条都不丢，但生产者的节拍被消费者
 *        绑架——消费者卡住（比如被打印互斥量挡住、串口线没插、printf 阻塞），
 *        生产者就永远停在上一条，队列里的旧数据也越来越旧（延迟无上界）。
 *        对一个"传感器采样"任务来说，这是最坏的结果：采样时间戳全乱。
 *      - 固定超时（如 100ms）：折中，但 100ms 正好等于采样周期，等于让
 *        采样周期在"正常 100ms"和"最坏 200ms"之间抖动，节拍不稳。
 *      - 0（非阻塞）：宁可丢这一条，也要保证 10Hz 的采样节拍稳定；
 *        丢弃次数进统计，消费者会把它打出来——丢包本身就是"下游跟不上"的
 *        诊断信号，比悄悄阻塞更有价值。
 *    结论：选 0（丢弃策略）。注意 CMSIS-RTOS v1 的一个坑：
 *      osMessagePut() 内部有 `ticks = millisec / portTICK_PERIOD_MS;
 *      if (ticks == 0) ticks = 1;`，所以传 0 并不等于"完全不等待"，
 *      而是"最多等 1 个 tick（1ms）"。1ms 相对 100ms 的采样周期可以忽略，
 *      但如果你在 1kHz 控制环里用 osMessagePut(...,0) 就一定要知道这件事。
 *
 * 3) osDelay：PIPE_SENSOR_PERIOD_MS（100ms）
 *    10Hz 的采样率。消费者每 10 条打印 1 条 → 1 行/秒，
 *    115200 波特率下一行 ~50 字节 ≈ 4.3ms，串口占用率 <0.5%，
 *    队列深度 20 能吸收约 2 秒的抖动，正常运行时队列深度应该长期在 0~1。
 */
#include "task_pipe_sensor.h"
#include "FreeRTOS.h"
#include "task.h"

/* ---- 共享句柄：Create() 里写入，任务启动后只读 ---- */
static osMessageQId s_q = NULL;

/*
 * 生产统计：只有生产者写，消费者通过 Task_Pipe_Sensor_GetStats() 读。
 * 读写都在临界区里做，保证 3 个字段是同一时刻的快照。
 */
static pipe_sensor_stats_t s_stats;

/* ---- 任务函数（内部使用） ---- */
static void PipeSensor_Handler(void const *argument)
{
    (void)argument;                 /* 参数走 s_q，不需要从 argument 拿 */

    pipe_data_t count = 0;

    for (;;)
    {
        osStatus ret;

        count++;                    /* 模拟一次采样：递增计数 */

        /* 入队。timeout=0 的选型理由见文件头第 2 条 */
        ret = osMessagePut(s_q, (uint32_t)count, 0u);

        /* 统计更新：临界区里改，避免消费者读到一半 */
        taskENTER_CRITICAL();
        s_stats.last_value = count;
        if (ret == osOK)
        {
            s_stats.sent++;
        }
        else
        {
            s_stats.dropped++;      /* 队列满（或句柄无效），本条丢弃 */
        }
        taskEXIT_CRITICAL();

        /* 生产者节拍：必须让出 CPU，否则低优先级也白搭 */
        osDelay(PIPE_SENSOR_PERIOD_MS);
    }
}

/*
 * 任务定义：osThreadDef(线程名标识符, 任务函数, 优先级, 实例数, 栈大小)
 *
 * 第一个参数是【标识符】，只用来拼出 os_thread_def_<name> 这个变量名和
 * 任务名字符串（宏里做了 #name），不是函数指针。
 * 旧代码写成 osThreadDef(SensorTask_Handler, SensorTask_Handler, ...)：
 * 编译能过（两个参数处在不同命名空间），但读的人会以为第一个参数是函数，
 * 而且调试器里任务名和函数名一模一样，栈回溯时分不清"这是任务还是函数"。
 * 这里改成 PipeSensorTask（名字）+ PipeSensor_Handler（函数）。
 *
 * 栈 128 word = 512 字节：任务体只有一个 osMessagePut + 整数自增，
 * 没有 printf/浮点，128 够用（configMINIMAL_STACK_SIZE = 128）。
 */
osThreadDef(PipeSensorTask, PipeSensor_Handler, osPriorityBelowNormal, 1, 128);

/* ---- 对外接口：创建任务 ---- */
void Task_Pipe_Sensor_Create(osMessageQId q)
{
    if (q == NULL)
    {
        return;                     /* 队列没建起来就别创建任务，省得任务里空指针 */
    }

    s_q = q;
    (void)osThreadCreate(osThread(PipeSensorTask), NULL);
}

/* ---- 对外接口：统计快照 ---- */
void Task_Pipe_Sensor_GetStats(pipe_sensor_stats_t *out)
{
    if (out == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    *out = s_stats;
    taskEXIT_CRITICAL();
}
