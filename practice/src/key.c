/*
 * key.c —— 按键驱动实现（GPIO 输入 + 20ms 周期扫描 + 消抖 + 短按/长按事件）
 *
 * 状态机（每个按键一份）：
 *
 *        原始电平 != 稳定态            连续 N 次一致
 *   ┌──────────────────────┐      ┌──────────────────────┐
 *   │ raw_cnt++            │ ───► │ stable = raw         │
 *   │ （可能是抖动）        │      │ raw_cnt = 0          │
 *   └──────────────────────┘      └──────────────────────┘
 *                                        │
 *              按下(stable=1) ────────────┼──────────── 松开(stable=0)
 *                    │                                    │
 *              hold_ms 开始累加                     long_fired ? 无事件
 *                    │                              : 抛 KEY_EVT_SHORT_PRESS
 *              hold_ms >= 1000 且未抛过长按
 *                    │
 *              抛 KEY_EVT_LONG_PRESS，long_fired = 1（松手不重复）
 *
 * 为什么要消抖而不是直接读电平：
 *   机械按键在按下/松开的几毫秒内会来回弹跳，直接读会得到一串
 *   "按下-松开-按下-松开"，一次按键被识别成好几次。
 *   这里用"连续 3 次(60ms)采样一致才更新稳定态"的计数式消抖：
 *   不需要 HAL_Delay，不会阻塞主循环，也不会漏掉真正的按键。
 */
#include "key.h"

/* ---- 硬件描述表：加按键就在这里加一行 ---- */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       active_low;   /* 1 = 按下时引脚为低（接 GND + 内部上拉） */
} key_hw_t;

static const key_hw_t s_key_hw[KEY_ID_COUNT] =
{
    /* KEY1: PA1，一端接 GND，内部上拉 */
    { GPIOA, GPIO_PIN_1, 1u },
};

/* ---- 运行时状态（每个按键一份） ---- */
typedef struct
{
    uint8_t  stable;      /* 消抖后的状态：1 = 按下 */
    uint8_t  raw_cnt;     /* 原始电平与 stable 不同的连续采样次数 */
    uint16_t hold_ms;     /* 已按住时长（ms），松手清零 */
    uint8_t  long_fired;  /* 本次按下是否已经抛过长按事件 */
} key_state_t;

static key_state_t     s_key_state[KEY_ID_COUNT];
static key_event_cb_t  s_cb = NULL;
static uint8_t         s_inited = 0;

/* 读原始电平，归一化成"1 = 按下" */
static uint8_t Key_ReadRaw(const key_hw_t *hw)
{
    GPIO_PinState level = HAL_GPIO_ReadPin(hw->port, hw->pin);

    if (hw->active_low)
    {
        return (level == GPIO_PIN_RESET) ? 1u : 0u;
    }
    return (level == GPIO_PIN_SET) ? 1u : 0u;
}

static void Key_Emit(key_id_t id, key_event_t evt)
{
    if (s_cb != NULL)
    {
        s_cb(id, evt);      /* 同步回调：应用层要短，别在这里死等 */
    }
}

void Key_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* 1) 开时钟：PA1 属于 GPIOA */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 2) 配引脚：输入 + 内部上拉（F1 没有 AF_INPUT 这种模式，纯输入即可） */
    gpio.Pin   = GPIO_PIN_1;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 3) 清状态机：用当前电平作为初始稳定态，避免上电误判一次按键 */
    for (uint8_t i = 0; i < (uint8_t)KEY_ID_COUNT; i++)
    {
        s_key_state[i].stable     = Key_ReadRaw(&s_key_hw[i]);
        s_key_state[i].raw_cnt    = 0;
        s_key_state[i].hold_ms    = 0;
        s_key_state[i].long_fired = 0;
    }

    s_cb = NULL;
    s_inited = 1;
}

void Key_SetCallback(key_event_cb_t cb)
{
    s_cb = cb;
}

void Key_Scan(void)
{
    if (!s_inited)
    {
        return;             /* 没初始化就直接返回，防止读未配置的引脚 */
    }

    for (uint8_t i = 0; i < (uint8_t)KEY_ID_COUNT; i++)
    {
        const key_hw_t *hw = &s_key_hw[i];
        key_state_t    *st = &s_key_state[i];
        uint8_t raw = Key_ReadRaw(hw);

        /* ---- ① 消抖：原始电平连续 N 次与稳定态不同，才认这次跳变 ---- */
        if (raw != st->stable)
        {
            st->raw_cnt++;

            if (st->raw_cnt >= KEY_DEBOUNCE_SAMPLES)
            {
                st->raw_cnt = 0;
                st->stable  = raw;

                if (raw != 0u)
                {
                    /* 刚确认按下：开始计按住时长，长按标志清零 */
                    st->hold_ms    = 0;
                    st->long_fired = 0;
                }
                else
                {
                    /* 刚确认松开：没抛过长按才补一个短按 */
                    if (!st->long_fired)
                    {
                        Key_Emit((key_id_t)i, KEY_EVT_SHORT_PRESS);
                    }
                }
            }
        }
        else
        {
            st->raw_cnt = 0;    /* 电平回到稳定态，抖动计数清零 */
        }

        /* ---- ② 按住计时：到阈值立刻抛长按，不等松手 ---- */
        if (st->stable != 0u)
        {
            if (st->hold_ms < 0xFFFFu)
            {
                st->hold_ms += (uint16_t)KEY_SCAN_PERIOD_MS;
            }

            if ((!st->long_fired) && (st->hold_ms >= (uint16_t)KEY_LONG_PRESS_MS))
            {
                st->long_fired = 1;
                Key_Emit((key_id_t)i, KEY_EVT_LONG_PRESS);
            }
        }
        else
        {
            st->hold_ms = 0;
        }
    }
}

uint8_t Key_IsPressed(key_id_t id)
{
    if (((uint8_t)id >= (uint8_t)KEY_ID_COUNT) || (!s_inited))
    {
        return 0;
    }
    return s_key_state[id].stable;
}

uint16_t Key_GetHoldTime(key_id_t id)
{
    if (((uint8_t)id >= (uint8_t)KEY_ID_COUNT) || (!s_inited))
    {
        return 0;
    }
    return s_key_state[id].hold_ms;
}
