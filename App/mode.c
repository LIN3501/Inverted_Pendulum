/**
  ******************************************************************************
  * @file    mode.c
  * @brief   模式切换：mode 0 是模式选择，1/2/3 是各功能模式
  ******************************************************************************
  * mode = 0        模式选择：4 行菜单 + 光标，电位器移光标，KEY0 进入，KEY1 翻页
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
#include "pid.h"
#include <stdio.h>

extern Motor_HandleTypeDef hmotor;   /* main.c 里定义，motor.h 只 extern 了 hencoder */

/* 第二行显示用的字符串缓冲区 */
static char message[20];

int mode = MODE_SELECT;         /* 当前模式，0 = 模式选择 */

static int menu_page = 0;       /* 菜单当前页，0 起 */
static int menu_row = -1;       /* 光标所在行 0~3，-1 = 需要整页重画 */

/* 菜单每行显示的文字，第 0 列留给光标 '>'，所以每行最多 15 字符 */
static const char *mode_names[4] = {
    "Mode1:Angle Cal",
    "Mode2:VOFA Tx",
    "Mode3:Balance",
    "Mode4:Open Loop",
};
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

    if (Key_Press(MODE_KEY_ENTER))
    {
        mode = MODE_SELECT;
    }
}

/* 模式 0：模式选择
 * 一页画 4 行菜单（y=0/2/4/6），第 0 列是光标 '>'：
 * 电位器（PA6）旋转上下移光标，KEY0 进入光标选中的模式，KEY1 切换下一页
 * 静态文字只在进菜单/翻页时整页重画，光标移动只画两个字符，
 * 避免整屏重刷（OLED 每字符约 6.4ms，整屏 ~180ms）卡主循环 */
void Mode_Select(void)
{
    int row = Potentiometer_Read(0, 3);             /* 电位器 -> 本页行 0~3 */

    if (screen_mode != MODE_SELECT)                 /* 刚进菜单：回第 1 页并整页重画 */
    {
        screen_mode = MODE_SELECT;
        menu_page = 0;
        menu_row = -1;
    }

    if (menu_row < 0)                               /* 进菜单或翻页后：整页画 4 行 + 光标 */
    {
        for (int i = 0; i < 4; i++)
        {
            OLED_ShowString(0, i * 2, (uint8_t *)" ", 16);          /* 清光标列 */
            OLED_ShowString(8, i * 2, (uint8_t *)mode_names[i], 16);
        }
        menu_row = row;
        OLED_ShowString(0, menu_row * 2, (uint8_t *)">", 16);
    }
    else if (menu_row != row)                       /* 电位器动了才挪光标 */
    {
        OLED_ShowString(0, menu_row * 2, (uint8_t *)" ", 16);        /* 擦旧光标 */
        OLED_ShowString(0, row * 2, (uint8_t *)">", 16);             /* 画新光标 */
        menu_row = row;
    }

    if (Key_Press(MODE_KEY_PAGE))                   /* KEY1：切下一页，页数不够时循环回本页 */
    {
        menu_page = (menu_page + 1) % ((MODE_MAX + 3) / 4);
        menu_row = -1;                              /* 逼下一轮整页重画 */
    }

    if (Key_Press(MODE_KEY_ENTER))                  /* KEY0：进入光标选中的模式 */
    {
        mode = menu_page * 4 + row + 1;
    }
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

/* ===== 模式 3：直立环（角度 PID，100Hz）=====
 * 调 PID 可以直接改下面的 Kp/Ki/Kd，也可以运行时用串口发 SET:Balance:KP:xxx 在线调；
 * 电机方向反了（越调越远）就在 out 前面加负号；
 * 摆杆竖直时角度不是 0 就把 setpoint 改成 Mode 1 里读到的角度(×100) */
/* 直立环 PID：PID_DEFINE 的第二个参数就是蓝牙调参命令里的 <环名>；
 * 默认参数写在静态初值里，PIDController_Init 只清状态不清增益，
 * 所以重新进模式 3 不会把蓝牙调好的 Kp/Ki/Kd 冲回默认值 */
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
static uint32_t mode3_last_tick = 0;
static uint8_t  mode3_tx_div = 0;

/* KEY0 退出时立刻停电机；VOFA+ 每 50ms 一帧：
 * ch0 = 0水平参考(恒0)，ch1 = PID 输出，ch2 = 直立环积分，ch3 = 误差(0-角度)，ch4 = 速度环角度设定，ch5 = 速度环积分 */
void Mode_3(void)
{
    if (screen_mode != mode)                /* 刚进模式：初始化 PID、使能电机 */
    {
        PIDController_Init(&pid_balance);
        PIDController_Init(&pid_velocity);
        /* 微分作用于测量值，预置当前角度，避免第一拍微分满幅尖峰 */
        pid_balance.prevMeasurement = Angle_Tick();
        Motor_Enable();
        mode3_last_tick = HAL_GetTick();
        mode3_tx_div = 0;

        OLED_ShowString(0, 2, (uint8_t *)"Balance PID", 16);
        OLED_ShowString(0, 4, (uint8_t *)"KEY0 = Exit", 16);
    }

    Mode_Page("Mode 3");

    if (mode != 3)                          /* KEY0 退出：立刻停电机 */
    {
        Motor_Stop(&hmotor);
        return;
    }

    if (HAL_GetTick() - mode3_last_tick >= 10)      /* 10ms = 100Hz */
    {
        mode3_last_tick = HAL_GetTick();

        float angle = Angle_Tick();                             /* 单位 ° */

        /* 速度外环：摆臂速度 -> 角度设定偏移（°），把速度压到 0 防止单方向漂移 */
        Encoder_Update(&hencoder);
        float speed = Encoder_GetSpeed(&hencoder);
        float angle_sp = PIDController_Position(&pid_velocity, 0.0f, speed);

        int16_t out = (int16_t)PIDController_Position(&pid_balance, angle_sp, angle);

        /* 过冲保护：偏离竖直超过 45° -> 立刻停电机退出模式3 */
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

        if (++mode3_tx_div >= 5)            /* 每 50ms 发一帧，避免串口阻塞拖累控制周期 */
        {
            mode3_tx_div = 0;
            /* ch0=0水平参考 ch1=PID输出 ch2=直立环积分 ch3=误差(0-角度) ch4=速度环角度设定 ch5=速度环积分，均为最近一拍的值 */
            Serial_SendFireWater(0.0f, (float)out, pid_balance.integrator, 0.0f - angle, angle_sp, pid_velocity.integrator);
        }
    }
}

/* ===== 模式 4：电位器开环控电机 =====
 * 旋转 PA6 电位器直接给电机 PWM，范围 -1500~1500（映射 0~4095，约 ±15% 占空比）；
 * OLED 第二行显示当前下发值；KEY0 回模式选择，退出时立刻停电机 */
void Mode_4(void)
{
    static int last_pwm = 0x7FFF;   /* 上次下发的 PWM，哨兵值（超出 -1500~1500）逼首拍执行 */

    if (screen_mode != mode)                /* 刚进模式：使能驱动芯片（STBY），画静态文字 */
    {
        Motor_Enable();
        last_pwm = 0x7FFF;
        OLED_ShowString(0, 2, (uint8_t *)"PWM:", 16);
    }

    Mode_Page("Mode 4");

    if (mode != 4)                          /* KEY0 退出：立刻停电机 */
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
