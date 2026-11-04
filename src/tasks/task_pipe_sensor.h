/*
 * task_pipe_sensor.h —— 队列流水线：生产端（采集源）模块入口
 *
 * 接口统一声明在 task_pipe.h（流水线是一根管子，两端的定义必须同源）：
 *     void Task_Pipe_Sensor_Create(osMessageQId q);
 *     void Task_Pipe_Sensor_GetStats(pipe_sensor_stats_t *out);
 *
 * 本模块行为概要：
 *   - 任务优先级 osPriorityBelowNormal（比打印/消费者低一级，别饿死下游）
 *   - 每 PIPE_SENSOR_PERIOD_MS(100ms) 生成一个递增计数
 *   - 入队 timeout 选 0（满了就丢），丢包数进统计供消费者打印
 */
#ifndef TASK_PIPE_SENSOR_H
#define TASK_PIPE_SENSOR_H

#include "task_pipe.h"

#endif /* TASK_PIPE_SENSOR_H */
