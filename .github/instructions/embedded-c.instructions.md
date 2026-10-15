---
description: 本项目 Core 下 C 源码的嵌入式编码约定（STM32F103 HAL）
applyTo: "Core/**/*.{c,h}"
---

# 嵌入式 C 编码约定

适用于 `Core/` 下所有 `.c` / `.h`。这些是**既有代码的既有风格**，新代码必须与之保持一致，不要引入新的风格。

> 涉及片上外设配置（时钟、GPIO、TIM、ADC、DMA、I2C、NVIC、EXTI 等）时，
> 一律遵守 `.github/copilot-instructions.md` 铁律 1：**只告诉用户去 CubeMX 怎么改，不得自动更改**。
>
> 涉及新增/修改函数时，一律遵守铁律 2：**优先复用现有函数，确需新增必须先问用户**。

## 函数复用（优先级高于本文件其他所有风格约定）

- 写任何逻辑前**先读 `Core/Inc/*.h`**，确认功能是否已存在。
- 现有可复用函数清单见 `.github/copilot-instructions.md` 文末「现有函数库」。
- **不得**新增 `static` 辅助函数、工具函数、包装函数、宏函数。
- **不得**修改现有函数的签名、参数、返回值或行为（等同新增，需先问）。
- 需要新增时，先向用户说明：函数名 + 完整签名 + 放哪个文件 + 为什么现有函数不够用（具体到函数名），等用户同意。
- 典型错误示范（禁止）：
  - 已有 `Motor_SetSpeed` 还去写 `SetMotorPwm`
  - 已有 `TIM_Task_Check` 还去写新的计时/延时函数
  - 已有 `Potentiometer_Read` 还去自己写一遍 ADC 到角度的换算
  - 已有 `OLED_ShowNum` 还去写数字转字符串再逐字符显示
- 需要"抽重复代码为函数"时**先问**，不要顺手重构。

## 命名与结构

- 模块前缀 + 下划线分隔的 PascalCase：`Motor_Init`、`Motor_SetSpeed`、`Potentiometer_Read`、`TIM_Task_Check`。
- 静态内部函数用 `static` 并加 `_` 前缀或保持文件内可见即可，不要暴露到头文件。
- 类型名以 `_HandleTypeDef` 结尾（自定义句柄也沿用）：`Motor_HandleTypeDef`。
- 句柄变量名沿用 CubeMX 风格 `hxxx`：`hmotor`、`hadc1`、`htim1`、`hi2c1`。
- 宏全大写加模块前缀：`MOTOR_AIN1_PORT`、`DEBOUNCE_TIME`。
- 枚举值全大写加模块前缀：`MOTOR_FORWARD`、`MOTOR_STOP`。
- 每个模块一对 `.c` / `.h`，头文件用 `#ifndef __XXX_H` guard，并包 `extern "C"`（与 `main.h` 一致）。

## 类型与数值

- 硬件相关量一律用 `<stdint.h>` 定长类型：`uint8_t` / `int16_t` / `uint32_t`。
- 不用 `int` / `short` 表达寄存器值、引脚、计数、时间戳。
- 有符号的速度、角度、误差用 `int16_t` / `int32_t`，不要用 `uint16_t` 存可能为负的量。
- 变量名或注释必须体现单位与量程，例如 `speed (-1000 ~ 1000)`、`time_tick`（1ms tick）、`adc_val`（12 位原始值 0~4095）。

## 浮点

- STM32F103 **没有 FPU**，浮点是软件模拟，开销大。
- 控制环 / 中断里**禁止**使用 `float` / `double` 运算。
- 需要比例换算时优先定点运算（整数乘除 + 移位），或预先算好系数。
- 只有非实时的显示、标定、初始化代码才允许用浮点。

## 中断与并发

- 中断服务函数与主循环共享的变量**必须** `volatile`（如现有 `key_0_pending`、`time_tick`）。
- 主循环对共享变量的读-改-写要注意原子性；32 位对齐变量的单次读写是原子的，复合操作需短暂关中断或用标志位传递（参考现有 `key_x_pending` 标志位 + `Key_Press` 消抖的写法）。
- 中断里**禁止**：`HAL_Delay`、`malloc`/`free`、`printf`、浮点运算、OLED 刷新、任何阻塞式 HAL 调用。
- 中断里只做置标志、计数、更新变量，实际处理放到主循环。
- 不要在中断里做长耗时的 `switch`/查表之外的复杂逻辑。

## 内存

- **新增代码禁止使用 `malloc` / `free`**（现有 `key.c` 里的 `TIM_Task_Create` 是历史遗留，不要模仿、不要扩散）。
- 需要对象就用静态分配的结构体 + 指针传入。
- 大数组、缓冲区优先用 `static`，避免放在栈上（默认栈很小）。

## 时间与调度

- 毫秒级延时/计时用 `HAL_GetTick()`。
- 周期性任务用现有的 `TIM_Task_Check(task, period)` 机制，**不要自己再造一套调度器**。
- 不使用 `HAL_Delay` 做控制环节拍。

## 头文件与包含

- `#include` 的文件名大小写必须与磁盘上的真实文件名**完全一致**（避免在 Linux / CubeIDE 下编译失败）。
- 头文件里只放声明、宏、类型定义；**不要定义变量**（要共享就 `extern` 声明 + 在 `.c` 里定义）。
- 自写模块的头文件只 `#include "stm32f1xx_hal.h"`，不要 `#include "main.h"` 造成循环依赖。
- 不在头文件里写实现。

## 注释

- 用**中文**注释，沿用现有风格（函数上方一行 `//` 说明用途）。
- 注释说明"为什么"和硬件含义（如 `// 左电机`、`// 假设定时器周期为1000`），不要逐行复述代码在做什么。
- 不写大段 Doxygen 块，除非用户要求。

## 错误处理

- 初始化失败沿用 `Error_Handler()`，不要新建错误处理框架。
- 不要凭空添加 `assert`、错误码枚举、返回值检查链条。

## 改动后

- 运行 `cmake --build build/Debug` 验证编译通过、无新增 warning。
- 不修改 `Core/Src|Inc` 下 CubeMX 生成的同名文件（`adc`、`dma`、`gpio`、`i2c`、`tim`、`main`、`stm32f1xx_*`）里 USER CODE 区块之外的内容。
- 需要改外设配置时，输出 CubeMX 操作步骤交给用户，不要动代码。
- 需要新增函数时，先问用户，不要先写代码再问。
