# add_freertos.py
# 将 ST framework 包自带的 FreeRTOS 中间件纳入编译。
#
# 背景：PlatformIO 的 stm32cube 框架构建脚本默认【不会】编译
#   Middlewares/Third_Party/FreeRTOS，因此直接 #include "cmsis_os.h" 会报
#   "无法打开源文件" 且链接时缺少内核符号。本脚本补齐 include 路径并编译内核源码。
#
# 使用方法：在 platformio.ini 中加入
#   extra_scripts = pre:add_freertos.py
# （必须用 pre: 前缀，在主程序构建前注入源码，否则会报
#   "The main program is already constructed" 错误）

import os

Import("env")

platform = env.PioPlatform()
board = env.BoardConfig()

mcu = board.get("build.mcu", "")            # 例如 stm32f103c8tx
framework_dir = platform.get_package_dir("framework-stm32cube%s" % mcu[5:7])
rtos_dir = os.path.join(
    framework_dir, "Middlewares", "Third_Party", "FreeRTOS", "Source"
)

# BluePill (F103) 是 Cortex-M3，对应 ARM_CM3 移植层；
# 若换成 Cortex-M4(带FPU) 的板子需改为 ARM_CM4F。
port_dir_name = os.path.join("portable", "GCC", "ARM_CM3")

# 1) 追加头文件搜索路径（编译器 + IntelliSense 都会用到）
env.Append(
    CPPPATH=[
        os.path.join(rtos_dir, "include"),          # FreeRTOS.h / task.h ...
        os.path.join(rtos_dir, port_dir_name),      # portmacro.h
        os.path.join(rtos_dir, "CMSIS_RTOS"),       # cmsis_os.h (CMSIS-RTOS v1)
        os.path.join("$PROJECT_DIR", "src", "bsp"),     # bsp_clock.h / bsp_gpio.h
        os.path.join("$PROJECT_DIR", "src", "tasks"),   # task_led.h ...
    ]
)

# 2) 将 FreeRTOS 内核源码编译为库
#    只挑选需要的文件：内核 *.c + 移植层 + 内存堆(heap_4) + CMSIS-RTOS 封装
env.BuildSources(
    os.path.join("$BUILD_DIR", "FreeRTOS"),
    rtos_dir,
    src_filter=[
        "+<*.c>",                                   # tasks.c / queue.c / list.c ...
        "+<%s>" % port_dir_name.replace(os.sep, "/"),  # port.c
        "+<portable/MemMang/heap_4.c>",            # 动态内存分配(osThreadCreate 依赖)
        "+<CMSIS_RTOS/cmsis_os.c>",                # CMSIS-RTOS v1 API 封装
    ],
)
