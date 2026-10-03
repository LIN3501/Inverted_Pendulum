# Inverted_pendulum

STM32F103C8T6 直流电机倒立摆（角度电位器方案 + 蓝牙/串口 PID 调参）。

## 目录分层

```
├── Core/            # CubeMX 生成代码（外设初始化、中断、main 骨架）
│                    # ⚠️ 只在 USER CODE 区内写代码；外设改动请回 .ioc 重新生成
├── Drivers/         # HAL / CMSIS 库（不要手改）
├── App/             # 应用逻辑层：业务状态机、传感器标定
│   ├── mode.c/h         模式状态机（菜单 / 角度显示 / VOFA 波形 / 直立环 / 开环）
│   └── angle_sensor.c/h 摆角传感器 ADC 原始值 → 角度标定换算
├── Composite/       # 复合层：跨层组合多个下层模块的"胶水"模块
│   └── serial.c/h       串口/蓝牙调参（USART1 收发 + VOFA+ FireWater 协议
│                        + SET:/GET: 命令桥接 PID 库）
├── Middleware/      # 可复用算法库：不直接碰外设句柄
│   └── pid.c/h          PID（位置式 + 增量式，含蓝牙调参命令解析）
├── BSP/             # 板级驱动层：每个模块绑定具体外设句柄
│   ├── motor.c/h        TB6612 电机（TIM1 PWM）+ TIM4 编码器测速
│   ├── oled.c/h         SSD1306 OLED（I2C1）
│   ├── oledfont.c/h     点阵字库
│   └── key.c/h          按键（EXTI + 消抖）+ 电位器（ADC DMA）
└── CMakeLists.txt   # 用户源文件与 include 路径在此登记（CubeMX 只生成一次）
```

依赖方向：`App → Composite → Middleware → BSP → Core/HAL`（上层可直接调用任意下层，
禁止反向）。

## 新文件放哪

- 只包一层硬件（某个外设句柄、某颗芯片的时序）→ `BSP/`
- 纯算法、数学、协议格式，不碰外设 → `Middleware/`
- 需要组合多个下层模块才能工作（如"串口调 PID"）→ `Composite/`
- 业务流程、状态机、标定参数 → `App/`
- 外设初始化、引脚分配 → 回 CubeMX `.ioc` 配置后重新生成，落在 `Core/`

新增 `.c/.h` 后需在根 `CMakeLists.txt` 的 `add_executable`/`target_sources`
和 `target_include_directories` 中登记对应目录。

## 构建

```bash
cmake --preset Debug
cmake --build build/Debug
```

## 已知遗留问题（暂未处理）

- `serial.c` 中 `data1..12`、`rx_num1..4`、`Serial_UpdateTxData()` 为死代码；
- 电位器函数（`Potentiometer_*`）放在 `key.c`，更宜拆到独立模块；
- `pid.c` 与 `serial.c` 互相包含（BSP 收到命令调 PID、PID 应答调串口发送），
  如需彻底解耦可改为回调注册，由 `App` 层中转；
- `TIM3` 已初始化但无任何用户代码使用，疑似 `.ioc` 残留。
