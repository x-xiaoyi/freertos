/*
 * main.c —— FreeRTOS 工程化骨架（BluePill / STM32F103C8T6）
 *
 * 职责：板级初始化 → 创建所有任务 → 启动调度器
 *       具体外设配置和任务逻辑分散到 bsp/ 和 tasks/ 目录。
 */

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

/* ---- BSP 模块 ---- */
#include "bsp_clock.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include "bsp_params.h"

/* ---- 任务模块 ---- */
#include "task_led.h"
#include "task_buzzer.h"
#include "task_print.h"
#include "task_uart_rx.h"

/*
 * 任务间共享参数必须是【静态/全局】变量，绝不能是 main() 的栈上局部变量。
 * 原因：osKernelStart() 启动调度器后，port.c 会把 MSP 拨回 RAM 顶端，
 * 用 main() 的栈帧当作中断栈；中断一来就把 main 的局部变量覆盖掉，
 * 任务里再解引用 &params 就是野指针 → HardFault / 卡死。
 */
static task_params_t params;

/*
 * LED 信标：串口不可用时用板载 LED（PC13，低电平亮）当"心跳"，
 * 通过闪烁节奏定位启动卡点：
 *   - 3 快闪 + 1 长闪 = 启动流程正常
 *   - 只有 3 快闪     = 卡在 BSP_Clock_Init（HSE 已加回退，理论不再发生）
 *   - 什么都不闪     = 固件没跑起来（烧录/供电问题）
 */
static void LED_Beacon(uint32_t blinks, uint32_t on_ms, uint32_t off_ms)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin   = GPIO_PIN_13;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    for (uint32_t i = 0; i < blinks; i++)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* 亮 */
        HAL_Delay(on_ms);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 灭 */
        HAL_Delay(off_ms);
    }
}

int main(void)
{
    /* 1) 板级初始化（时钟、外设） */
    HAL_Init();
    LED_Beacon(3, 100, 100);  /* 3 快闪：CPU 活着 */
    BSP_Clock_Init();
    LED_Beacon(1, 500, 100);  /* 1 长闪：时钟配置完成 */
    BSP_GPIO_Init();
    BSP_Buzzer_GPIO_Init();
    BSP_UART_Init();

    /* 串口通路验证：开机问候。烧录后若串口助手能收到这句话，说明接线没问题 */
    printf("[Boot] BluePill UART ready\r\n");

    /* 2) 创建任务 */

    osSemaphoreDef(semaphore);
    osMutexDef(printmutex);
    params.semId = osSemaphoreCreate(osSemaphore(semaphore),0); /* 初值 0：开机无预置令牌，等 'b' 命令才响 */
    params.print_Mutex = osMutexCreate(osMutex(printmutex));
    Task_Buzzer_Create(params.semId);
    Task_Print_Create(params.print_Mutex);
    Task_UART_RX_Create(&params);

    /* 3) 启动 FreeRTOS 调度器（不再返回） */
    osKernelStart();

    while (1) {}   /* 永远不会执行到这里 */
}