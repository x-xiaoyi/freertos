/*
 * task_pipe_consumer.h —— 队列流水线：消费端模块入口
 *
 * 接口统一声明在 task_pipe.h：
 *     void Task_Pipe_Consumer_Create(osMessageQId q, osMutexId print_mutex);
 *
 * 本模块行为概要：
 *   - 任务优先级 osPriorityNormal（高于生产者，保证队列有人及时搬空）
 *   - osMessageGet(q, osWaitForever) 阻塞等，队列空时完全让出 CPU
 *   - 每 PIPE_PRINT_EVERY(10) 条打印 1 条，每 PIPE_STATS_EVERY(50) 条打一行汇总
 *   - printf 前拿 print_mutex（与 task_print / task_uart_rx 同一把），保证整行输出
 */
#ifndef TASK_PIPE_CONSUMER_H
#define TASK_PIPE_CONSUMER_H

#include "task_pipe.h"

#endif /* TASK_PIPE_CONSUMER_H */
