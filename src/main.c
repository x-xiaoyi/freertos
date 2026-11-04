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

/* ---- 队列流水线（生产者 → 队列 → 消费者） ---- */
#include "task_pipe.h"
#include "task_pipe_sensor.h"
#include "task_pipe_consumer.h"

/*
 * 任务间共享参数必须是【静态/全局】变量，绝不能是 main() 的栈上局部变量。
 * 原因：osKernelStart() 启动调度器后，port.c 会把 MSP 拨回 RAM 顶端，
 * 用 main() 的栈帧当作中断栈；中断一来就把 main 的局部变量覆盖掉，
 * 任务里再解引用 &params 就是野指针 → HardFault / 卡死。
 */
static task_params_t params;

/*
 * 流水线队列句柄。
 * 同样是 static：Create() 只是把句柄存进各任务模块的静态变量，
 * 但句柄本身在调度器启动后仍要长期有效，放栈上没有理由。
 */
static osMessageQId pipe_q;

/*
 * 队列定义（编译期常量）：
 *   osMessageQDef(名字, 深度, 元素类型)
 * 深度和元素类型来自 task_pipe.h —— 生产者和消费者共用同一份定义，
 * 不会出现"生产者以为放的是 uint32_t、消费者按 uint8_t 取"的错位。
 * 这个宏只是定义一个 const osMessageQDef_t 描述符，真正的队列要到
 * main() 里 osMessageCreate() 才向 FreeRTOS heap 申请内存。
 */
osMessageQDef(PipeQueue, PIPE_QUEUE_DEPTH, pipe_data_t);

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

    /* 2) 创建内核对象 */
    osSemaphoreDef(semaphore);
    osMutexDef(printmutex);

    /*
     * 信号量：'b' 命令 → 蜂鸣器响一次的"命令令牌"。
     *
     * 这里有个 CMSIS-RTOS v1 + 本 port 的坑（原代码写 0，会在启动时挂死）：
     *   osSemaphoreCreate(def, count) 的实现是
     *     count == 1 → vSemaphoreCreateBinary()，创建的是【已经 give 过一次】
     *                  的二进制信号量，即开机就有 1 个令牌；
     *     count != 1 → xQueueCreateCountingSemaphore(count, count)，
     *                  而 queue.c 里第一行就是 configASSERT(uxMaxCount != 0)。
     *   所以传 0 不是"初值 0"，而是"创建一个最大计数为 0 的计数信号量"，
     *   直接触发 configASSERT 里的 for(;;) 死循环 → 板子卡在 osKernelStart 之前。
     *
     * 正确写法：按 count == 1 创建二进制信号量，再立刻把它自带的那个令牌
     * 取走（timeout=0 非阻塞），得到"开机无令牌、等 'b' 命令才响"的效果。
     */
    params.semId = osSemaphoreCreate(osSemaphore(semaphore), 1);
    (void)osSemaphoreWait(params.semId, 0);   /* 吃掉初始令牌：开机必须静音 */

    params.print_Mutex = osMutexCreate(osMutex(printmutex));

    /* 流水线队列：深度 PIPE_QUEUE_DEPTH(20)、元素 4 字节 */
    pipe_q = osMessageCreate(osMessageQ(PipeQueue), NULL);

    /* 3) 创建任务 */
    Task_LED_Create();                  /* PC13 心跳，500ms 翻转一次 */
    Task_Buzzer_Create(params.semId);   /* 等 'b' 命令的信号量才响 */
    Task_Print_Create(params.print_Mutex);
    Task_UART_RX_Create(&params);

    /* 流水线两端：队列没建起来时 Create 内部会直接返回，不会创建野任务 */
    if (pipe_q != NULL)
    {
        Task_Pipe_Sensor_Create(pipe_q);                          /* 生产：10Hz 递增计数 */
        Task_Pipe_Consumer_Create(pipe_q, params.print_Mutex);    /* 消费：1/10 抽样打印 */
    }
    else
    {
        printf("[Boot] pipe queue create failed, pipeline disabled\r\n");
    }

    /* 4) 启动 FreeRTOS 调度器（不再返回） */
    osKernelStart();

    while (1) {}   /* 永远不会执行到这里 */
}