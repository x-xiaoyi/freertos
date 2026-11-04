/*
 * task_pipe.h —— 队列流水线公共定义（生产者 → 队列 → 消费者）
 *
 * 这条流水线是工程里"任务间通信"最完整的一条链：
 *
 *   task_pipe_sensor.c          队列               task_pipe_consumer.c
 *   ┌────────────────┐      ┌──────────┐      ┌──────────────────────┐
 *   │ 每 100ms count++│ ───► │ 深度 20   │ ───► │ osMessageGet 阻塞等   │
 *   │ osMessagePut    │      │ 元素=uint32│      │ 每 10 条打印 1 条     │
 *   └────────────────┘      └──────────┘      └──────────────────────┘
 *        优先级 BelowNormal                        优先级 Normal
 *
 * 为什么用一个头文件放类型/深度/统计：生产者和消费者必须对"队列里放的
 * 是什么"达成一致，放在同一个头文件里就不会出现两边 sizeof 不一样的惨案。
 */
#ifndef TASK_PIPE_H
#define TASK_PIPE_H

#include <stdint.h>
#include "cmsis_os.h"

/*
 * 队列元素类型：一个 32 位递增计数。
 *
 * 为什么锁死 4 字节：
 *   CMSIS-RTOS v1 的 osMessagePut(q, uint32_t info, timeout) 只搬运一个
 *   uint32_t；osMessageGet 也把收到的内容塞进 osEvent.value.v（uint32_t）。
 *   - 元素宽度 < 4 字节：xQueueReceive 只覆盖低字节，剩下的是栈上残留，
 *     读出来的值会"随机化"；
 *   - 元素宽度 > 4 字节：osMessagePut 根本传不进去（形参就是 uint32_t）。
 *   所以这里的元素类型必须是 uint32_t。要传结构体请改用 osMailQ 或
 *   传指针（指针也要保证所指内存在生命周期内有效）。
 */
typedef uint32_t pipe_data_t;

/*
 * 队列深度：20 条。
 *
 * 量化一下：生产者 100ms 一条 → 满队列 ≈ 2 秒的积压。
 *   - 太浅（2~4 条）：消费者只要被打印互斥量挡一下、或串口一慢，
 *     立刻开始丢包，丢包率反映不出"到底是偶尔抖还是彻底卡死"；
 *   - 太深（>64 条）：F103C8 只有 20KB RAM，而且积压太久会让统计
 *     失去实时意义（你看到的值是 6 秒前的数据）。
 * 20 条 × 4 字节 = 80 字节数据区 + 约 80 字节队列控制块，
 * 全部走 FreeRTOS heap（configTOTAL_HEAP_SIZE = 15KB），占用可忽略。
 */
#define PIPE_QUEUE_DEPTH       20u

/* 生产者节拍：100ms 一条，即 10 Hz。 */
#define PIPE_SENSOR_PERIOD_MS  100u

/* 消费者抽样：每收到 10 条打印 1 条 → 1 行/秒，串口不被刷屏。 */
#define PIPE_PRINT_EVERY       10u

/* 消费者汇总统计：每收 50 条（约 5 秒）额外打一行运行统计。 */
#define PIPE_STATS_EVERY       50u

/*
 * 生产端统计快照。
 * 生产者任务写，消费者任务通过 Task_Pipe_Sensor_GetStats() 在临界区里拷一份，
 * 避免读到"sent 更新了、dropped 还没更新"的半新半旧状态。
 */
typedef struct
{
    uint32_t sent;        /* osMessagePut 返回 osOK 的次数 */
    uint32_t dropped;     /* 队列满、本条被丢弃的次数 */
    uint32_t last_value;  /* 最近一次生成的值（观察生产者是否还活着） */
} pipe_sensor_stats_t;

/* ======================================================================
 * 流水线对外接口
 *
 * 生产者和消费者是同一根管子的两端，接口故意放在同一个头文件里：
 * 改队列深度/元素类型/抽样率时，两边拿到的一定是同一份定义。
 * 各模块的 .h（task_pipe_sensor.h / task_pipe_consumer.h）只做聚合，
 * 不再重复声明，避免两处声明漂移。
 * ====================================================================== */

/* 创建生产端任务：q 由 main.c 用 osMessageCreate(osMessageQ(PipeQueue), NULL) 创建 */
void Task_Pipe_Sensor_Create(osMessageQId q);

/* 创建消费端任务：print_mutex 必须与 task_print / task_uart_rx 是同一把锁 */
void Task_Pipe_Consumer_Create(osMessageQId q, osMutexId print_mutex);

/* 取一份生产统计快照（临界区内整体拷贝，保证三个字段是同一时刻的值） */
void Task_Pipe_Sensor_GetStats(pipe_sensor_stats_t *out);

#endif /* TASK_PIPE_H */
