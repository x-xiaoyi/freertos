/*
 * key.h —— 按键驱动（GPIO 输入 + 20ms 周期扫描 + 消抖 + 短按/长按事件）
 *
 * 设计要点：
 *   1) 驱动本身【不阻塞、不延时】：Key_Scan() 每 20ms 被调用一次，
 *      内部只做"读电平 → 计数 → 状态机推进"，几微秒返回；
 *   2) 消抖用"连续 N 次采样一致才认账"，不是 HAL_Delay 死等：
 *      N = KEY_DEBOUNCE_MS / KEY_SCAN_PERIOD_MS = 60/20 = 3，
 *      即电平要稳定 60ms 才算数（机械按键抖动一般 <10ms）；
 *   3) 事件通过回调抛给应用层：短按 = 按下后未到长按阈值就松开；
 *      长按 = 按住满 KEY_LONG_PRESS_MS 立即触发一次（不等松手），
 *      松手后不会再补一个短按；
 *   4) 新增按键只需在 key.c 的 s_key_hw[] 表里加一行，并把 KEY_ID_x 加进枚举。
 *
 * 接线（BluePill）：
 *   KEY1 = PA1 → GND，按下为低电平；芯片内部上拉，松开为高。
 *   按键一端接 PA1，另一端接 GND 即可，不需要外部电阻。
 */
#ifndef KEY_H
#define KEY_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* 扫描周期：主循环必须按这个节奏调用 Key_Scan() */
#define KEY_SCAN_PERIOD_MS   20u

/* 消抖窗口：电平连续稳定这么久才更新状态 */
#define KEY_DEBOUNCE_MS      60u

/* 长按阈值：按住满 1 秒触发长按事件（只触发一次） */
#define KEY_LONG_PRESS_MS    1000u

/* 消抖需要连续一致的采样次数（编译期算出来，避免运行时除法） */
#define KEY_DEBOUNCE_SAMPLES ((KEY_DEBOUNCE_MS + KEY_SCAN_PERIOD_MS - 1u) / KEY_SCAN_PERIOD_MS)

/* 按键事件类型 */
typedef enum
{
    KEY_EVT_NONE = 0,
    KEY_EVT_SHORT_PRESS,   /* 短按：按下后未到长按阈值就松开 */
    KEY_EVT_LONG_PRESS     /* 长按：按住满 KEY_LONG_PRESS_MS（松手不再补短按） */
} key_event_t;

/* 按键编号：有几个按键就有几个 ID */
typedef enum
{
    KEY_ID_1 = 0,
    KEY_ID_COUNT
} key_id_t;

/* 事件回调：应用层注册，驱动在扫描过程中同步调用（要短，别在里面死等） */
typedef void (*key_event_cb_t)(key_id_t id, key_event_t evt);

/* 初始化按键 GPIO（内部上拉输入）并清空状态机 */
void Key_Init(void);

/* 周期扫描：必须每 KEY_SCAN_PERIOD_MS(20ms) 调一次 */
void Key_Scan(void);

/* 注册事件回调（传 NULL 表示只更新状态、不抛事件） */
void Key_SetCallback(key_event_cb_t cb);

/* 查询当前消抖后的状态：1 = 按下 */
uint8_t Key_IsPressed(key_id_t id);

/* 查询已按住时长（ms）；未按下返回 0 */
uint16_t Key_GetHoldTime(key_id_t id);

#endif /* KEY_H */
