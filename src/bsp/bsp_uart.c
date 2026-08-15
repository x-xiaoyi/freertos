/*
 * bsp_uart.c —— 板级串口初始化实现（USART1 / PA9 / 115200）
 *
 * 初始化三步曲（和 GPIO 同一个套路）：
 *   1) 开时钟   —— GPIOA + USART1，外设不供电就是块砖
 *   2) 配引脚   —— PA9 交给 USART1 当 TX 用（复用功能）
 *   3) 配外设   —— 波特率/数据位/停止位，然后使能
 */
#include "bsp_uart.h"

/* USART1 句柄：HAL 所有操作都围绕这个"控制块"进行（又是句柄模式！） */
UART_HandleTypeDef huart1;

/* ---- 接收环形缓冲区（中断写、任务读：单生产者/单消费者）---- */
#define RX_BUF_SIZE 64
static uint8_t rx_byte;                     /* HAL 中断接收的目标字节 */
static volatile uint8_t  rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;       /* 只有中断写 */
static volatile uint16_t rx_tail = 0; 
static volatile uint16_t rx_count = 0;

static void BSP_UART_StartRxIT(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void BSP_UART_Init(void)
{
    /* 1) 开时钟：先 GPIO 后 USART1（顺序讲究：引脚归 GPIO 端口管，端口得先有电） */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    /* 2) 配引脚：PA9 → USART1_TX
     * 注意 Mode 是 AF_PP（复用推挽），不是 OUT_PP！
     *   OUT_PP：引脚听 CPU 的（你写 1 它就出 1）
     *   AF_PP：引脚交给外设接管，由 USART1 自己按波特率拉高拉低
     */
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* TX：PA9 → USART1_TX（复用推挽，由外设自己驱动） */
    GPIO_InitStruct.Pin   = GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* RX：PA10 → USART1_RX（输入 + 上拉，空闲时保持高电平，防止悬空误收） */
    GPIO_InitStruct.Pin   = GPIO_PIN_10;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 3) 配外设参数：8 数据位 / 无校验 / 1 停止位 / 可收可发 */
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart1);

    /* 4) 打开 USART1 中断并武装接收。
     * 优先级 5 与 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 一致；
     * 中断里只调 HAL、不调 FreeRTOS FromISR API，安全。 */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    BSP_UART_StartRxIT();

    /* 5) 关闭 stdout 缓冲：否则 printf 会先把字符攒在内存里，攒够一批才发，
     *    表现为"串口半天没输出"。关掉缓冲后，每个字符即写即发。 */
    setvbuf(stdout, NULL, _IONBF, 0);
}

/*
 * printf 重定向：GCC/newlib 下，printf 的所有字符最终流经 _write 这个"总下水口"。
 * （Keil 工程里对应的是 fputc，原理相同）
 * 重写它，printf 就从串口出去了。
 */
int _write(int file, char *ptr, int len)
{
    (void)file;   /* file 是文件描述符（1=stdout），我们不用它，防编译警告 */
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* ---- 中断服务：HAL 要求我们在 USART1 中断里调用它的分发函数 ---- */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* 收到 1 字节：压入环形缓冲区，然后立刻重新武装下一次接收 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    uint16_t next = (uint16_t)((rx_head + 1u) & (RX_BUF_SIZE - 1u));
    if (next != rx_tail)          /* 缓冲区没满才存，满了丢这一个字节 */
    {
        rx_count++;
        rx_buf[rx_head] = rx_byte;
        rx_head = next;
    }

    BSP_UART_StartRxIT();
}

/* 出错（比如接收过载 ORE）后 HAL 会停掉接收，这里重新武装，防止接收"死掉" */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        BSP_UART_StartRxIT();
    }
}

/* 非阻塞读 1 字节：有数据返回 1 并写入 *out，没有则立刻返回 0 */
uint8_t BSP_UART_ReadByte(uint8_t *out)
{
    if (rx_head == rx_tail)
    {
        return 0;
    }

    *out = rx_buf[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & (RX_BUF_SIZE - 1u));
    return 1;
}
uint16_t BSP_UART_GetRxCount(void)
{
    return rx_count;
}
