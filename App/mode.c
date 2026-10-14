/**
  ******************************************************************************
  * @file    mode.c
  * @brief   模式切换：mode 0 是模式选择，1/2/3 是各功能模式
  ******************************************************************************
    * mode = 0        模式选择：菜单的绘制在 Composite/menu.c，这里只取选中结果
  * mode = 1 / 2 / 3  各功能模式，按 KEY0 回到模式选择
  ******************************************************************************
  */

#include "mode.h"
#include "key.h"
#include "oled.h"
#include "adc.h"
#include "angle_sensor.h"
#include "serial.h"
#include "motor.h"
#include "menu.h"
#include "pid.h"
#include <stdio.h>
#include <math.h>

extern Motor_HandleTypeDef hmotor;   /* main.c 里定义，motor.h 只 extern 了 hencoder */

/* 第二行显示用的字符串缓冲区 */
static char message[20];

int mode = MODE_SELECT;         /* 当前模式，0 = 模式选择 */

static uint32_t mode1_last_tick = 0;    /* 模式 1 上次刷新的时刻，单位 ms */
static uint32_t mode2_last_tick = 0;    /* 模式 2 上次发串口的时刻，单位 ms */

/* 各模式的公共部分：显示标题 + KEY0 返回模式选择
 * 标题占第一行，提示放最后一行，中间两行留给各模式自己用
 * 静态文字只在换模式后画一次：OLED 每字符要 22 次单字节 I2C（100kHz 下约 6.4ms），
 * 每轮循环都重刷会吃掉 100ms 以上，把 Mode_2 的串口帧率从 50Hz 拖到 10Hz 以下。
 * main.c 在模式切换时会 OLED_Clear()，所以用 screen_mode 记住"屏幕上画的是哪个模式"，
 * 换模式后它和 mode 不相等，静态文字自然重画一遍 */
static int screen_mode = -1;        /* 屏幕上画的是哪个模式的静态内容，-1 = 屏幕已清空 */

static void Mode_Page(const char *title)
{
    if (screen_mode != mode)
    {
        screen_mode = mode;

        OLED_ShowString(0, 0, (uint8_t *)title, 16);
    }

    if (Key_Press(MENU_KEY_ENTER))
        {
            mode = MODE_SELECT;
        }
    }

    /* 模式 0：模式选择
     * 菜单本体在 Composite/menu.c（分页 / 光标 / 菜单项文字都在那边），
     * 这里只把它返回的模式号写回 mode，0 表示还停在菜单 */
    void Mode_Select(void)
    {
        mode = Menu_Update(&screen_mode, mode);
    }

/* 模式 1：每 50ms 刷新一次
 * 第二行 = PA2 的 ADC 采样原始值，第三行 = Angle_Tick() 换算出的角度
 * "ADC:"/"Angle=" 是静态文字，跟标题一样只在换模式后画一次；
 * 每次刷新只重画数字：原来一次约 15 字符 ≈ 64ms，现在约 12 字符 ≈ 33ms */
void Mode_1(void)
{
    if (screen_mode != mode)                /* 刚进这个模式，静态文字先画 */
    {
        OLED_ShowString(0, 2, (uint8_t *)"ADC:", 16);
        OLED_ShowString(0, 4, (uint8_t *)"Angle=", 16);
    }

    Mode_Page("Mode 1");

    if (HAL_GetTick() - mode1_last_tick >= 50)      /* 50ms 到点 */
    {
        mode1_last_tick = HAL_GetTick();

        OLED_ShowNum(40, 2, adc_dma_buf[0], 4, 16);          /* [0] = PA2 原始值 0~4095 */

        sprintf(message, "%8.2f", Angle_Tick());     /* 定宽 8，从 x=48 起盖住旧值 */
        OLED_ShowString(48, 4, (uint8_t *)message, 16);
    }
}

/* 模式 2：把角度传感器原始采样值通过 USART1 发给 VOFA+（FireWater 协议）
 * VOFA+ 里选 FireWater 模式，波特率 115200，数据源选串口
 * 每 20ms 发一帧（50Hz）：ch0 = PA2 角度传感器原始值 0~4095，ch1 = Angle_Tick() 角度值（°），ch2 = Encoder_GetSpeed() 编码器速度（脉冲/秒），ch3 = Encoder_GetCount() 编码器计数值，ch4/ch5 = 占位 0 */
void Mode_2(void)
{
    Mode_Page("Mode 2");

    if (HAL_GetTick() - mode2_last_tick >= 20)      /* 20ms 到点 */
    {
        mode2_last_tick = HAL_GetTick();

        Encoder_Update(&hencoder);
        Serial_SendFireWater((float)adc_dma_buf[0], Angle_Tick(), Encoder_GetSpeed(&hencoder), (float)Encoder_GetCount(&hencoder), 0.0f, 0.0f);
    }
}

/* ===== 模式 3：电位器开环控电机 =====
 * 旋转 PA6 电位器直接给电机 PWM，范围 -150~150（映射 0~4095，约 ±15% 占空比）；
 * OLED 第二行显示当前下发值；KEY0 回模式选择，退出时立刻停电机 */
void Mode_3(void)
{
    static int last_pwm = 0x7FFF;   /* 上次下发的 PWM，哨兵值（超出 -1500~1500）逼首拍执行 */

    if (screen_mode != mode)                /* 刚进模式：使能驱动芯片（STBY），画静态文字 */
    {
        Motor_Enable();
        last_pwm = 0x7FFF;
        OLED_ShowString(0, 2, (uint8_t *)"PWM:", 16);
    }

    Mode_Page("Mode 3");

    if (mode != 3)                          /* KEY0 退出：立刻停电机 */
    {
        Motor_Stop(&hmotor);
        return;
    }

    int pwm = Potentiometer_Read(-1500, 1500);

    if (pwm != last_pwm)                    /* 值变了才下发+刷屏，OLED 刷一次约 6ms */
    {
        last_pwm = pwm;
        Motor_SetSpeed(&hmotor, pwm);

        sprintf(message, "%5d", pwm);
        OLED_ShowString(40, 2, (uint8_t *)message, 16);
    }
}

/* ===== 模式 4：直立环（角度 PID，100Hz）=====
 * 调 PID 可以直接改下面的 Kp/Ki/Kd，也可以运行时用串口发 SET:Balance:KP:xxx 在线调；
 * 电机方向反了（越调越远）就在 out 前面加负号；
 * 摆杆竖直时角度不是 0 就把 setpoint 改成 Mode 1 里读到的角度(×100) */
/* 直立环 PID：PID_DEFINE 的第二个参数就是蓝牙调参命令里的 <环名>；
 * 默认参数写在静态初值里，PIDController_Init 只清状态不清增益，
 * 所以重新进模式 4 不会把蓝牙调好的 Kp/Ki/Kd 冲回默认值 */
/* PWM 满幅 1000 → 10000（TIM1 Prescaler=0、Period=9999，7.2kHz），
 * 输出侧增益/限幅同步 ×10，每单位占空比与旧刻度一致：
 * Kp 40→400，Kd 120→1200，积分限幅 250→2500；SET:Balance:... 在线调参也都是新刻度 */
PID_DEFINE(pid_balance, "Balance",
    .Kp        = 400.0f,    /* 每 1° 误差输出 400（4% PWM） */
    .Ki        = 0.0f,      /* 直立环不带积分——实测会以 2.5s 周期泵出慢摆，积分职责已移到速度环 */
    .Kd        = 1000.0f,   /* 每 1°/拍 角度变化输出 1200（12% PWM） */
    .alpha     = 0.8f,      /* 微分低通 0.8 */
    .limMin    = -10000,
    .limMax    =  10000,
    .limMinInt = -2500.0f,
    .limMaxInt =  2500.0f);

/* 速度外环（级联在直立环外）：目标速度 0，输出（单位 °）作为直立环的角度设定偏移，
 * 限幅 ±5°。
 * 摆臂单方向漂移的根源是直立环的常值平均力矩没人管，速度环把摆臂速度压到 0。
 * 方向反了直接发 SET:Velocity:KP:-<值> 在线取负，不用改代码。
 * 初始值 1.0 = 速度 1(count/ms) 对应 1° 设定偏移，按 VOFA 波形 ×2 步进调 */
PID_DEFINE(pid_velocity, "Velocity",
    .Kp        = 12.0f,      /* 阻尼项：10ms 窗已加 0.8/0.2 低通滤平台阶，Kp 按 ×2 步进加大（5→10→20），目标 Kp:Ki≈100~200:1；出现快震回退一档 */
    .Ki        = 0.08f,     /* 弹簧项：积分=∫速度=摆臂位置反馈，把摆臂拉回原位；过低压不住慢摆（会跑到 360° 大摆），过高只抬摆动频率不助衰减 */
    .Kd        = 0.0f,
    .alpha     = 0.8f,
    .limMin    = -8,
    .limMax    =  8,
    .limMinInt = -8.0f,
    .limMaxInt =  8.0f);
/* TIM3 中断里算完的最近一拍快照，供主循环 50ms 遥测帧读取（volatile：ISR 写、主循环读） */
static volatile float mode4_out      = 0.0f;   /* PID 输出 */
static volatile float mode4_angle    = 0.0f;   /* 角度（°），遥测 ch3 = 0 - 它 */
static volatile float mode4_angle_sp = 0.0f;   /* 速度环给出的角度设定（°） */
static uint32_t mode4_last_tick = 0;           /* 主循环上次发遥测的时刻（ms） */

/* PID 计算在 TIM3 中断（100Hz）里跑，这里只管：初始化、KEY0 退出、50ms 遥测。
 * VOFA+ 每 50ms 一帧：
 * ch0 = 0水平参考(恒0)，ch1 = PID 输出，ch2 = 直立环积分，ch3 = 误差(0-角度)，ch4 = 速度环角度设定，ch5 = 速度环积分 */
void Mode_4(void)
{
    if (screen_mode != mode)                /* 刚进模式：初始化 PID、使能电机 */
    {
        PIDController_Init(&pid_balance);
        PIDController_Init(&pid_velocity);
        /* 微分作用于测量值，预置当前角度，避免第一拍微分满幅尖峰 */
        pid_balance.prevMeasurement = Angle_Tick();
        Motor_Enable();
        mode4_last_tick = HAL_GetTick();

        OLED_ShowString(0, 2, (uint8_t *)"Balance PID", 16);
        OLED_ShowString(0, 4, (uint8_t *)"KEY0 = Exit", 16);
    }

    Mode_Page("Mode 4");

    if (mode != 4)                          /* KEY0 退出：立刻停电机 */
    {
        Motor_Stop(&hmotor);
        return;
    }

    if (HAL_GetTick() - mode4_last_tick >= 50)      /* 50ms 发一帧遥测（计算在 TIM3 中断里） */
    {
        mode4_last_tick = HAL_GetTick();
        /* ch0=0水平参考 ch1=PID输出 ch2=直立环积分 ch3=误差(0-角度) ch4=速度环角度设定 ch5=速度环积分，均为最近一拍中断里的快照 */
        Serial_SendFireWater(0.0f, mode4_out, pid_balance.integrator, 0.0f - mode4_angle, mode4_angle_sp, pid_velocity.integrator);
    }
}

/* ===== 模式 5：自动起摆（发挥(1)）=====
 * 两态状态机：SWING 甩摆 -> HOLD 直立环（Mode 4 同款），摔了回 SWING 重甩。
 *
 * SWING 两条规则：
 *   1. 进模式头 S5_KICK_MS -> 朝正方向踹一脚，破"垂着不动 θ̇=0"的启动死区。
 *      （只踹这一次：中途摔落重甩时摆杆还带着速度，规则自己能接上；
 *      若真彻底停死，重进一次 Mode 5 即可）
 *   2. 其余时间相位锁定泵能：out = S5_SIGN·S5_PWM·sign(θ̇·cosθ)。
 *      推导：摆杆能量 dE/dt = ml·θ̇·cosθ·a_p，指令与 θ̇cosθ 同号则每拍都做正功，
 *      摆幅单调涨到顶。自带锁相，不依赖共振周期估计。
 *
 * θ 从倒立量起（0°=倒立、±180°=下垂）。下垂点正是传感器 ±180° 缝，
 * 求 θ̇ 前先把跨缝差值拉回 360° 内，否则每周期冒假尖峰把相位打反。
 */
#define S5_SIGN        (+1)     /* 泵能方向：VOFA 上摆幅越甩越小 -> 改 -1 */
#define S5_PWM         3500     /* 甩摆/踹脚共用幅值（满幅 10000 刻度） */
#define S5_KICK_MS     300      /* 进模式先踹一脚的时长 */
#define S5_CATCH_DEG   20.0f    /* 捕获窗半宽：|摆角| 小于此且速度够慢就交给直立环 */
#define S5_CATCH_VEL   200.0f   /* 捕获允许的最大摆速 °/s */
#define S5_FALL_DEG    60.0f    /* HOLD 中偏离此角判摔，回 SWING 重甩 */

enum { S5_SWING = 0, S5_HOLD = 1 };

/* ISR 写、主循环读的快照（volatile） */
static volatile struct {
    uint8_t  state;
    uint32_t t0;         /* 进模式时刻：踹脚计时 + 成绩计时起点 */
    uint32_t first_ms;   /* 首次捕获用时 ms，0=还没成过 */
    float    angle;      /* 摆角（°），0=倒立 */
    float    vel;        /* 摆角速度（°/s），差分+低通后 */
    float    out;        /* 电机输出（-10000~10000） */
} s5 = { .state = S5_SWING };

/* 单边私有状态：两边各用各的，不跨上下文（不需要 volatile） */
static struct {
    float    prev_raw;   /* 仅 ISR：上一拍摆角，求跨缝差值用 */
    uint32_t last_tick;  /* 主循环：遥测节拍 */
    uint32_t disp_tick;  /* 主循环：刷屏节拍 */
} s5_priv;

/* Mode 5 的 100Hz 控制节拍，从 HAL_TIM_PeriodElapsedCallback 里调 */
static void Mode5_ControlISR(void)
{
    float angle = Angle_Tick();                     /* (−180,180]，0=倒立 */

    float d = angle - s5_priv.prev_raw;             /* 跨 ±180° 缝时差值冒 ±360，拉回来 */
    if (d > 180.0f)       d -= 360.0f;
    else if (d < -180.0f) d += 360.0f;
    s5_priv.prev_raw = angle;
    s5.vel = 0.7f * s5.vel + 0.3f * (d / 0.01f);    /* 100Hz 差分 + 一阶低通；除以 0.01s 是单位换算：°/10ms -> °/s */

    Encoder_Update(&hencoder);                      /* 每拍刷新，HOLD 才能直接取到新鲜速度 */

    int16_t out;

    if (s5.state == S5_SWING)
    {
        if      (HAL_GetTick() - s5.t0 < S5_KICK_MS)        out = S5_SIGN * S5_PWM;  /* 踹一脚起步 */
        else if (s5.vel * cosf(angle * 0.0174533f) >= 0.0f) out =  S5_SIGN * S5_PWM; /* 泵能：与 θ̇cosθ 同号 */
        else                                                out = -S5_SIGN * S5_PWM;

        /* 捕获：贴顶且速度收敛 -> 交给直立环（Init 清状态不清增益，Mode4 调好的参数沿用） */
        if (fabsf(angle) < S5_CATCH_DEG && fabsf(s5.vel) < S5_CATCH_VEL)
        {
            s5.state = S5_HOLD;
            if (s5.first_ms == 0) s5.first_ms = HAL_GetTick() - s5.t0;   /* 成绩冻结 */
            PIDController_Init(&pid_balance);
            PIDController_Init(&pid_velocity);
            pid_balance.prevMeasurement = angle;    /* 防微分首拍尖峰 */
        }
    }
    else    /* S5_HOLD：与 Mode 4 完全同款的速度外环 + 直立环 */
    {
        float speed    = Encoder_GetSpeed(&hencoder);
        float angle_sp = PIDController_Position(&pid_velocity, 0.0f, speed);
        out = (int16_t)PIDController_Position(&pid_balance, angle_sp, angle);

        if (fabsf(angle) > S5_FALL_DEG)             /* 摔了：回 SWING 重甩 */
        {
            s5.state = S5_SWING;
            out = 0;
        }
    }

    Motor_SetSpeed(&hmotor, out);                   /* 内部自带 ±10000 钳幅 */

    s5.angle = angle;                               /* 快照给主循环遥测/显示 */
    s5.out   = (float)out;
}

/* 模式 5 主循环侧：初始化、KEY0 退出、遥测与显示（控制全在 TIM3 中断里） */
void Mode_5(void)
{
    if (screen_mode != mode)                /* 刚进模式：状态机复位，从甩摆开始 */
    {
        s5.state         = S5_SWING;
        s5_priv.prev_raw = Angle_Tick();
        s5.vel           = 0.0f;
        s5.first_ms      = 0;
        s5.t0            = HAL_GetTick();   /* 踹脚计时和成绩计时都从这里起 */
        Motor_Enable();

        OLED_ShowString(0, 2, (uint8_t *)"St:", 16);
        OLED_ShowString(0, 4, (uint8_t *)"T=", 16);
    }

    Mode_Page("Mode 5");

    if (mode != 5)                          /* KEY0 退出：立刻停电机 */
    {
        Motor_Stop(&hmotor);
        return;
    }

    if (HAL_GetTick() - s5_priv.last_tick >= 50)      /* 50ms 一帧遥测 */
    {
        s5_priv.last_tick = HAL_GetTick();
        /* ch0=摆角 ch1=角速度 ch2=0(占位) ch3=状态(0甩/1立) ch4=输出 ch5=起摆用时s */
        Serial_SendFireWater(s5.angle, s5.vel, 0.0f, (float)s5.state, s5.out,
                             s5.first_ms / 1000.0f);
    }

    if (HAL_GetTick() - s5_priv.disp_tick >= 100)     /* 100ms 刷一次显示 */
    {
        s5_priv.disp_tick = HAL_GetTick();

        OLED_ShowString(24, 2, (uint8_t *)(s5.state == S5_HOLD ? "HOLD " : "SWING"), 16);

        /* T = 首次捕获用时（成绩，冻结）；没成过就显示已进行时间 */
        float t = (s5.first_ms > 0) ? (s5.first_ms / 1000.0f)
                                    : (HAL_GetTick() - s5.t0) / 1000.0f;
        sprintf(message, "%6.2f", t);
        OLED_ShowString(16, 4, (uint8_t *)message, 16);
    }
}

/* TIM3 更新中断 = 100Hz 固定控制周期（.ioc：Prescaler=71 → 1MHz，Counter Period 需=9999 才是 10ms）。
 * 只做 PID + 电机输出，不做任何阻塞操作（OLED、串口发送必须留在主循环）。
 * mode 4/5 各走各的控制分支；mode != 4/5 或 screen_mode 还没跟上都不动手：
 * screen_mode 在各模式完成初始化、Motor_Enable 之后才被置位，
 * 保证中断第一次执行时状态已就绪；
 * KEY0 退出或保护逻辑把 mode 改掉的那一瞬，中断立即停手 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM3)
    {
        return;
    }

    if (mode != 4 && mode != 5)
    {
        return;
    }

    if (screen_mode != mode)
    {
        return;
    }

    if (mode == 5)
    {
        Mode5_ControlISR();
        return;
    }

    float angle = Angle_Tick();                             /* 单位 ° */

    /* 速度外环：摆臂速度 -> 角度设定偏移（°），把速度压到 0 防止单方向漂移 */
    Encoder_Update(&hencoder);
    float speed = Encoder_GetSpeed(&hencoder);
    float angle_sp = PIDController_Position(&pid_velocity, 0.0f, speed);

    int16_t out = (int16_t)PIDController_Position(&pid_balance, angle_sp, angle);

    /* 过冲保护：偏离竖直超过 45° -> 立刻停电机退出模式4 */
    if (angle >= 45.0f || angle <= -45.0f)
    {
        Motor_Stop(&hmotor);
        mode = MODE_SELECT;                 /* 回模式选择，主循环会清屏 */
        return;
    }

    /* 死区补偿：输出非零就先越过电机启动阈值，过零段不再"断油"；总输出仍钳回限幅（满幅 10000 刻度） */
    // if (out > 0)      out += 400;
    // else if (out < 0) out -= 400;
    if (out > (int16_t)pid_balance.limMax)       out = (int16_t)pid_balance.limMax;
    else if (out < (int16_t)pid_balance.limMin)  out = (int16_t)pid_balance.limMin;

    Motor_SetSpeed(&hmotor, out);

    mode4_out      = (float)out;           /* 快照给主循环遥测帧 */
    mode4_angle    = angle;
    mode4_angle_sp = angle_sp;
}
