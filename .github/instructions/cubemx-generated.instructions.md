---
description: CubeMX 生成文件保护规则 + 片上外设设置只讲不改
applyTo: "Core/{Src,Inc}/{main,adc,dma,gpio,i2c,tim,stm32f1xx_*,system_stm32f1xx,syscalls,sysmem}.{c,h}"
---

# CubeMX 生成文件保护规则

**当前正在编辑的文件是由 STM32CubeMX 生成的。** 重新生成代码时，`USER CODE` 区块之外的任何改动都会**被静默覆盖丢失**。

## 第 0 条：片上外设设置只讲不改 ★最高优先级★

**本文件涉及的一切片上外设配置，都由用户在 STM32CubeMX 里手动修改。不得自动更改。**

即使你 100% 确定怎么改、即使只差一个数字，也**只讲，不动手**。必须输出可直接照做的 CubeMX 操作步骤：

```
【需要在 CubeMX 里修改】<外设名>

1. 打开 Inverted_pendulum.ioc
2. Pinout & Configuration → <分类> → <外设>
   - <面板名> → <字段名> = <值>
3. （若涉及引脚）Pinout view → 把 <引脚> 设为 <功能>
4. （若涉及中断）NVIC Settings → 勾选 <中断名>
5. 保存 .ioc → GENERATE CODE → cmake --build build/Debug

【为什么】<一句话>
【改完后注意】<影响>
```

不得用"我帮你改生成代码效果一样"绕过。唯一例外：用户明确说"你直接改，不用我去 MX 改"，此时先复述"下次重新生成会被覆盖"并等确认。

## 必须遵守

1. 只能在 `/* USER CODE BEGIN xxx */` 与 `/* USER CODE END xxx */` 之间写入代码。
2. 不删除、不移动、不重排任何生成内容。
3. 不格式化、不调整生成内容的缩进、不整理生成的 `#include`。
4. 不修改生成的外设初始化参数（`MX_XXX_Init` 里的赋值、`HAL_XXX_Init` 调用、MspInit 里的时钟使能/GPIO/DMA/NVIC 配置）。
5. 不修改 `main.h` 里生成的引脚宏（`KEY1_Pin`、`BIN1_Pin` 等）。
6. 不修改 `SystemClock_Config()`。
7. 不修改 `/* USER CODE END Header */` 上方的文件头注释块。

## 用户想改外设配置时

- 按第 0 条给出 CubeMX 操作步骤，让用户自己改。
- 如果用户坚持要手改生成代码，先说明"重新生成会丢失"，等用户确认后再改。

## 允许做的

- 在 `USER CODE BEGIN Includes` 里加 `#include`
- 在 `USER CODE BEGIN PV` 里加全局变量
- 在 `USER CODE BEGIN 2` / `USER CODE BEGIN WHILE` / `USER CODE BEGIN 3` 里加应用逻辑
- 在 `USER CODE BEGIN 0` / `USER CODE BEGIN 4` 里加自己的函数
- 在 `USER CODE BEGIN EFP` 里加函数声明
- 在 `USER CODE BEGIN Private defines` 里加宏

> 注意：上面"加自己的函数"同样受铁律 2 约束 —— **优先复用现有函数，确需新增必须先问用户同意**。
> 不得以"反正写在 USER CODE 里不会被覆盖"为理由擅自新增函数。

## 额外注意

- `main.c` 的 `while(1)` 里，业务代码写在 `USER CODE BEGIN WHILE` 之后、`USER CODE END WHILE` 之前；`USER CODE BEGIN 3` 之后的内容每轮循环都会执行。
- `stm32f1xx_it.c` 里的中断处理函数，自定义逻辑写在对应 `USER CODE BEGIN xxx_IRQn` 区块内；HAL 回调（如 `HAL_GPIO_EXTI_Callback`）的实现放在自写模块里（参考 `key.c`）。
