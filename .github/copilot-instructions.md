# 项目全局约束（优先级最高，与用户当次指令冲突时以用户为准）

## 第 0 条：开工前自检

**先答后做**：提问先纯文字回答，不夹带改动；改文件前先说明「改哪个文件、改什么、为什么」+ 下面 5 条自检结论，等用户回话再动手。只读操作（读、搜、编译）不限。

动手前 5 条自检：
1. 改哪些文件？命中铁律 6 禁改清单（`.ioc`、CMake、`build/`、`Drivers/`、IDE 目录、生成文件 USER CODE 区块外）→ 停手。
2. 涉及片上外设？→ 铁律 1：只给 CubeMX 步骤，不改代码、不改 `.ioc`。
3. 新增函数或改签名？→ 铁律 2：先列"函数名+签名+放哪+为什么现有函数不够用"，等同意。
4. 超 1 个文件 / 超约 30 行 / 新增文件 / 需求有多种解读？→ 铁律 4：先说明，等确认。
5. 改完跑 `cmake --build build/Debug` 且无新增 warning（铁律 9）。

`.ioc` 值与用户口述不一致时不得代改，只指出差异并给 CubeMX 步骤。

## 项目事实

- STM32F103xB（Cortex-M3，**无 FPU**），HAL，STM32CubeIDE + CMake + Ninja（starm-clang）
- 构建：`cmake --build build/Debug`；引脚/外设唯一真源：`Inverted_pendulum.ioc`
- 自写模块（可自由改）：`Core/Src|Inc` 下 `motor`、`oled`、`oledfont`、`key`
- 生成文件（受保护）：`main`、`adc`、`dma`、`gpio`、`i2c`、`tim`、`stm32f1xx_*`、`system_stm32f1xx`、`syscalls`、`sysmem`
- 背景：倒立摆（电机 + 电位器角度 + OLED + 按键），控制环逐步搭建

## 铁律

### 1. 片上外设设置只讲不改 ★最高★

任何外设配置（时钟、GPIO、TIM、ADC、DMA、I2C、SPI、USART、NVIC、EXTI、看门狗、RTC、PWR、引脚标签、外设使能，及对应 `.ioc` 内容）一律由用户在 CubeMX 手动改。**即使只差一个数字也只讲不动手**；不得编辑 `.ioc`、生成文件 USER CODE 区块外的内容、`main.h` 引脚宏；不得以"我改生成代码一样"绕过。

必须输出可照做的步骤：

```
【需要在 CubeMX 里修改】<外设名>
1. 打开 Inverted_pendulum.ioc
2. Pinout & Configuration → <分类> → <外设>
   - <面板名> → <字段名> = <值>
3. （若有引脚变更）Pinout view → 把 <引脚> 设为 <功能>
4. （若涉及中断）NVIC Settings → 勾选 <中断名>
5. 保存 .ioc → GENERATE CODE → cmake --build build/Debug
【为什么】<一句话>
【改完后注意】<影响>
```

唯一例外：用户明确说"你直接改，不用我去 MX 改"→ 先复述"重新生成会被覆盖"并等确认。

### 2. 复用优先，禁止擅自新增函数 ★

动手前先读 `Core/Inc/*.h`，优先复用文末函数库，不得重新实现已有功能。新增/修改函数（含 `static` 辅助、宏、改签名、改行为、顺带改头文件加宏加全局变量）必须先停下，说明：**函数名 + 完整签名 + 放哪个文件 + 为什么现有函数不够用（具体到函数名）**，等用户明确同意。抽重复代码成函数也先问。

### 3. 最小改动
只改用户明确要求的那一件事；不顺手重构/格式化/重命名；无关问题只在回答里提一句，不动手。

### 4. 先确认再动手
满足铁律 0 自检 4 中任一条件（多文件、新文件、新函数、超约 30 行、多种解读）必须先问。

### 5. 禁止擅自添加
未经要求不得引入：新文件/模块/库、日志/`printf`、错误码/断言框架、状态机/调度器/队列/环形缓冲、防御性检查（除非确有故障）、备用代码/`TODO`、无关宏/枚举/typedef。

### 6. 禁止触碰的区域
`Drivers/`、`CMakeLists.txt`、`CMakePresets.json`、链接脚本、启动文件、`Inverted_pendulum.ioc`、`.mxproject`、`build/`、`.vscode/`、`.settings/`、`.idea/`、CubeMX 生成文件 USER CODE 区块之外的任何一行。

### 7. 只用 HAL，不碰寄存器
用 HAL 函数与宏（`HAL_TIM_PWM_Start`、`__HAL_TIM_SET_COMPARE` 等）；不写 `TIM1->CCR4 = x;` 之类寄存器操作，除非用户明确要求。

### 8. 不确定就问，不要猜
引脚/外设实例/PWM 通道、时钟与定时器参数、物理量单位量程、硬件极性与接线——代码查不到且用户没说就停下来问，不填"看起来合理"的值。

### 9. 改完必须验证
改 C 代码后跑 `cmake --build build/Debug`，编译通过且无新增 warning，失败修到干净；无法编译时明确说"未编译验证"。

## 回答风格

中文；先结论后说明；代码给可直接编译的完整片段；外设配置给 CubeMX 步骤；新增函数先列清单等回答；不确定的事直说"不确定"。

## 代码约定

见 `.github/instructions/embedded-c.instructions.md` 与 `cubemx-generated.instructions.md`（按文件类型自动加载）。

## 现有函数库（铁律 2 复用清单；头文件是唯一真源，动手前重读）

- **motor.h**：`Motor_Init`、`Motor_SetSpeed(hmotor, speed)`（-1000~1000，自动方向）、`Motor_SetDirection`、`Motor_Stop`、`Motor_Enable/Disable`；类型 `MotorID`（MOTOR_A 左 / MOTOR_B 右）、`MotorDirection`
- **key.h**：`Key_Press(key_id)`（0/1/2，50ms 消抖）、`TIM_Task_Check(task, period)`（**周期任务用这个**）、`TIM_Task_GetElapsed`、`TIM_Task_Create`（内部 malloc，**新代码不用**）、`Potentiometer_Init(hadc)`、`Potentiometer_Read(min, max)`；全局 `volatile int time_tick`（1ms）、`int key_0_pending/1_pending/2_pending`
- **oled.h**：`OLED_Init/Clear`、`OLED_ShowString(x, y, str, size)`（size 一般 16）、`OLED_ShowChar`、`OLED_ShowNum(x, y, num, len, size)`、`OLED_Set_Pos`、`OLED_Display_On/Off`、`OLED_On`；`WriteCmd` 是遗留声明**不用**
- **可直接调用的 HAL（不算新增）**：`HAL_GetTick`、`HAL_GPIO_WritePin/ReadPin`、`HAL_TIM_PWM_Start/Stop`、`__HAL_TIM_SET_COMPARE`、`HAL_ADC_GetValue`、`HAL_I2C_Master_Transmit` 等
