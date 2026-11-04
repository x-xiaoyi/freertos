/*
 * task_pipe_consumer.c —— 队列流水线：消费端任务
 *
 * 职责：
 *   1) osMessageGet(q, osWaitForever) 阻塞等待，队列空时任务完全让出 CPU；
 *   2) 每收到 PIPE_PRINT_EVERY(10) 条打印 1 条（1/10 抽样），
 *      避免 10Hz 的数据把 115200 的串口刷爆；
 *   3) 每 PIPE_STATS_EVERY(50) 条打一行汇总（约 5 秒一次），
 *      把生产者视角的 sent/dropped 和队列实时深度一起打出来，
 *      这样"下游跟不上"能直接从 drop 计数上看出来。
 *
 * 关键约束：printf 必须借 task_print / task_uart_rx 的同一把互斥量。
 *   串口是独占资源，一次 printf 的几十个字节在 115200 下要 ~4ms，
 *   这 4ms 里如果串口回显任务插进来写一个字符，输出就会变成
 *   "[Pipe] rx=1U0 seq=1n0kno..." 这种被拦腰截断的乱码。
 *   所以：拿锁 → 整行打完 → 还锁，和 task_print.c 的规矩完全一致。
 */
#include "task_pipe_consumer.h"
#include "task_pipe_sensor.h"
#include <stdio.h>

/* ---- 共享句柄：Create() 里写入，任务启动后只读 ---- */
static osMessageQId s_q = NULL;
static osMutexId  s_print_mutex = NULL;

/* 打印一行抽样：值 + 生产端统计 + 当前队列深度 */
static void Consumer_PrintSample(pipe_data_t value, uint32_t recv, uint32_t printed)
{
    pipe_sensor_stats_t st;

    Task_Pipe_Sensor_GetStats(&st);         /* 临界区快照，不长时间关中断 */

    if (s_print_mutex != NULL)
    {
        osMutexWait(s_print_mutex, osWaitForever);
    }

    printf("[Pipe] rx=%lu seq=%lu printed=%lu sent=%lu drop=%lu q=%lu\r\n",
           (unsigned long)recv,
           (unsigned long)value,
           (unsigned long)printed,
           (unsigned long)st.sent,
           (unsigned long)st.dropped,
           (unsigned long)osMessageWaiting(s_q));

    if (s_print_mutex != NULL)
    {
        osMutexRelease(s_print_mutex);
    }
}

/* 打印一行汇总统计（约 5 秒一次） */
static void Consumer_PrintStats(uint32_t recv, uint32_t printed)
{
    pipe_sensor_stats_t st;

    Task_Pipe_Sensor_GetStats(&st);

    if (s_print_mutex != NULL)
    {
        osMutexWait(s_print_mutex, osWaitForever);
    }

    printf("[Pipe] stats rx=%lu printed=%lu sent=%lu drop=%lu q=%lu\r\n",
           (unsigned long)recv,
           (unsigned long)printed,
           (unsigned long)st.sent,
           (unsigned long)st.dropped,
           (unsigned long)osMessageWaiting(s_q));

    if (s_print_mutex != NULL)
    {
        osMutexRelease(s_print_mutex);
    }
}

/* ---- 任务函数（内部使用） ---- */
static void PipeConsumer_Handler(void const *argument)
{
    (void)argument;                 /* 参数走 s_q / s_print_mutex */

    uint32_t recv = 0;              /* 从队列取到的总条数 */
    uint32_t printed = 0;           /* 实际打印过的行数 */

    for (;;)
    {
        /* 队列空 → 任务在这里阻塞，CPU 交给别人；这是"队列解耦"的核心 */
        osEvent ev = osMessageGet(s_q, osWaitForever);

        if (ev.status != osEventMessage)
        {
            /* 正常路径不会走到：只有句柄无效/队列被删才会返回非 osEventMessage。
             * 退避 100ms 防止把 CPU 空转烧掉。 */
            osDelay(100);
            continue;
        }

        recv++;

        if ((recv % PIPE_STATS_EVERY) == 0u)
        {
            printed++;
            Consumer_PrintStats(recv, printed);
        }
        else if ((recv % PIPE_PRINT_EVERY) == 0u)
        {
            /* osEvent.value.v 就是队列元素本体（pipe_data_t 是 4 字节 uint32_t，
             * 所以这里不会截断；元素类型一旦超过 4 字节这个读法就废了，
             * 参见 task_pipe.h 里对元素宽度的说明）。 */
            printed++;
            Consumer_PrintSample((pipe_data_t)ev.value.v, recv, printed);
        }
    }
}

/*
 * 任务定义：osThreadDef(线程名标识符, 任务函数, 优先级, 实例数, 栈大小)
 *
 * 优先级 osPriorityNormal（= FreeRTOS 3），和打印/回显任务同级：
 *   - 必须高于生产者（BelowNormal = 2），否则队列满了没人及时搬；
 *   - 又不需要更高：它自己不产生数据，只是"被队列叫醒"，
 *     用 AboveNormal 只会无谓地打断打印任务正在进行的整行输出。
 * 栈 192 word = 768 字节：任务体里有 printf（newlib 的格式化代码
 * 在栈上开临时缓冲），128 word 偏紧，给到 192 留余量。
 */
osThreadDef(PipeConsumerTask, PipeConsumer_Handler, osPriorityNormal, 1, 192);

/* ---- 对外接口：创建任务 ---- */
void Task_Pipe_Consumer_Create(osMessageQId q, osMutexId print_mutex)
{
    if (q == NULL)
    {
        return;                     /* 队列没建起来就别创建任务 */
    }

    s_q = q;
    s_print_mutex = print_mutex;
    (void)osThreadCreate(osThread(PipeConsumerTask), NULL);
}
