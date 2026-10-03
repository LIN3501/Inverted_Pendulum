
#ifndef _PID_H_
#define _PID_H_

#include <stdint.h>

/* 全浮点 PID：参数直接写实际值，无任何缩放
 * 例如 Kp=2.0 直接写 2.0f，alpha=0.8 直接写 0.8f
 * Ki/Kd 为每采样周期增益，需折算采样周期 */
// PID_DEFINE(pid, "Balance",
//     .Kp = 2.0f, .Ki = 0.0f, .Kd = 0.1f,
//     .alpha = 0.8f,
//     .limMax = 1000, .limMin = -1000,
//     .limMaxInt = 200.0f, .limMinInt = -200.0f);

typedef struct {

    /* 蓝牙调参里的 <环名>，用 PID_DEFINE 定义变量时一起填，如 "Balance" */
    const char *name;

    /* 控制器增益参数（实际值） */
    float Kp;	/* 比例增益 */
    float Ki;	/* 积分增益（每采样周期） */
    float Kd;	/* 微分增益（每采样周期） */

    /* 微分低通滤波系数 0~1，新微分项所占比例 */
    float alpha;

    /* 输出限幅（原始单位） */
    int16_t limMin;	/* 输出下限 */
    int16_t limMax;	/* 输出上限 */

    /* 积分器限幅（用于抗积分饱和） */
    float limMinInt;	/* 积分器下限 */
    float limMaxInt;	/* 积分器上限 */

    /* 控制器状态变量（跨周期保持） */
    float integrator;			/* 积分器累计值 */
    float prevError;			/* 上一次的误差值 e(k-1) */
    float prevError2;			/* 上上次的误差值 e(k-2)，用于增量式PID */
    float prevMeasurement;	/* 上一次的测量值，用于位置式PID微分计算 */
    float differential;		/* 微分低通滤波器状态 */

    /* 控制器输出 */
    float out;	/* PID计算输出结果（原始单位，限幅内可带小数） */

} PIDController;

// 相应PID参数清0
void    PIDController_Init(PIDController *pid);
// 位置式PID：输出 = Kp*e + Ki*∫e + Kd*de/dt
float PIDController_Position(PIDController *pid, float setpoint, float measurement);
// 增量式PID：输出 += Kp*Δe + Ki*e + Kd*Δ²e
int16_t PIDController_Incremental(PIDController *pid, float setpoint, float measurement);

/* ==========================================================================
 * 蓝牙/串口 在线调参
 *
 * 命令用冒号分段，行尾 \r\n 会被自动忽略：
 *   SET:<环名>:<参数>:<数值>   设置，如 SET:Balance:KP:250
 *   GET:<环名>:<参数>          查询单个，如 GET:Balance:KP
 *   GET:<环名>:ALL             一次查 Kp/Ki/Kd 三个
 *
 *   <环名> = 定义 PIDController 变量时用 PID_DEFINE 起的名字，如 "Balance"
 *   <参数> = KP / KI / KD / MININT / MAXINT / ALL，其中 ALL 只能跟在 GET 后面
 *            MININT/MAXINT = 积分限幅 limMinInt/limMaxInt，参数名必须全大写
 *
 * 应答发回上位机，同样以 \r\n 结尾：
 *   SET:Balance:KP:250.0000
 *   SET:Balance:MININT:-2000.0000
 *   GET:Balance:KP:250.0000
 *   GET:Balance:MAXINT:2000.0000
 *   GET:Balance:ALL:KP:250.0000:KI:0.0000:KD:10.0000
 *   出错时只有一行：ERR:CMD / ERR:LOOP / ERR:PARAM / ERR:VALUE
 * ========================================================================== */

/* 在定义 PIDController 变量的同时起名并登记进调参查找表，一步到位：
 *   PID_DEFINE(pid_balance, "Balance", .Kp = 200.0f, .Ki = 0.0f, .Kd = 10.0f);
 * 生成的是本文件内的 static 变量；登记动作由上电构造器自动跑，
 * 所以不用进任何模式，随时都能 GET:<名字>:... 查参数。
 * 尾部要打分号。变量只能在定义它的 .c 里用。 */
#define PID_DEFINE(var, pid_name, ...)                                       \
    static PIDController var = { .name = pid_name, __VA_ARGS__ };            \
    static void __attribute__((constructor, used)) _pid_reg_##var(void)      \
    {                                                                        \
        PID_Register(&var);                                                  \
    }

// 把一个 PID 环登记进查找表（环名取自 pid->name）；同名重复登记只更新指针
void PID_Register(PIDController *pid);

// 解析并执行一条调参命令（cmd 会被 strtok 改写），应答通过串口发回上位机
void PID_ProcessCommand(char *cmd);

#endif

