
#include "pid.h"
#include "serial.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void PIDController_Init(PIDController *pid) {

	/* 清除控制器状态 */
	pid->integrator = 0;			/* 清零积分器 */
	pid->prevError  = 0;			/* 清零上一次误差 e(k-1) */
	pid->prevError2 = 0;			/* 清零上上次误差 e(k-2) */
	pid->prevMeasurement = 0;		/* 清零上一次测量值 */
	pid->differential = 0;			/* 清零滤波状态 */

	pid->out = 0;					/* 清零输出 */

}

float PIDController_Position(PIDController *pid, float setpoint, float measurement) {

    float error = setpoint - measurement;

	/* 比例项 */
    float proportional = pid->Kp * error;

	/* 积分项：梯形积分法；|误差| < 2° 时保持不累加，防止平衡点附近积分慢性漂移 */

		pid->integrator += pid->Ki * (error + pid->prevError) * 0.5f;


	/* 抗积分饱和 */
    if (pid->integrator > pid->limMaxInt)
	{
        pid->integrator = pid->limMaxInt;
    }
	else if (pid->integrator < pid->limMinInt)
	{
        pid->integrator = pid->limMinInt;
    }

	/* 微分项：作用于测量值，一阶低通滤波
	 * differential = (1-alpha)*diff + alpha*Kd*delta
	 */
	float delta = measurement - pid->prevMeasurement;
	pid->differential = (1.0f - pid->alpha) * pid->differential
	                    + pid->alpha * pid->Kd * delta;

	/* 合成输出并限幅 */
    float out = proportional + pid->integrator - pid->differential;

    if (out > pid->limMax)
	{
        out = (float)pid->limMax;
    }
	else if (out < pid->limMin)
	{
        out = (float)pid->limMin;
    }

    pid->prevError       = error;
    pid->prevMeasurement = measurement;
    pid->out = out;
    return pid->out;
}

int16_t PIDController_Incremental(PIDController *pid, float setpoint, float measurement) {

    float error = setpoint - measurement;

	/* 增量式PID计算
	 * Δout = Kp*(e(k)-e(k-1)) + Ki*e(k) + Kd*(e(k)-2*e(k-1)+e(k-2))
	 */

	/* 比例增量：Kp * (e(k) - e(k-1)) */
	float delta_p = pid->Kp * (error - pid->prevError);

	/* 积分增量：Ki * e(k) */
	float delta_i = pid->Ki * error;

	/* 微分增量：Kd * (e(k) - 2*e(k-1) + e(k-2))，带一阶低通滤波 */
	float delta2_error = error - 2.0f * pid->prevError + pid->prevError2;
	pid->differential = (1.0f - pid->alpha) * pid->differential
	                    + pid->alpha * pid->Kd * delta2_error;

	/* 增量合成 */
	float increment = delta_p + delta_i + pid->differential;

	/* 更新状态 */
	pid->prevError2 = pid->prevError;  // e(k-1) -> e(k-2)
	pid->prevError = error;            // e(k) -> e(k-1)

	return (int16_t)increment;
}

/* ===== 蓝牙/串口 在线调参：已登记的 PID 环查找表 ===== */
#define PID_MAX_LOOPS   4       /* 最多登记 4 个 PID 环 */

static PIDController *pid_list[PID_MAX_LOOPS];
static uint8_t        pid_count = 0;

// 把一个 PID 环登记进查找表（环名取自 pid->name）；同名重复登记只更新指针
void PID_Register(PIDController *pid)
{
	uint8_t i;

	for (i = 0; i < pid_count; i++)
	{
	    if (strcmp(pid_list[i]->name, pid->name) == 0) /* 同名已登记：只换指针 */
	    {
	        pid_list[i] = pid;
	        return;
	    }
	}

	if (pid_count >= PID_MAX_LOOPS) return;     /* 表满，丢弃 */

	pid_list[pid_count++] = pid;
}

// 解析并执行一条调参命令：SET:<环>:<参数>:<数值> / GET:<环>:<参数>，应答发回上位机
void PID_ProcessCommand(char *cmd)
{
	char           buf[16];     /* 格式化浮点用，容量与 serial.c 的 Serial_SendFloat 一致 */
	char          *end;
	float         *p_gain;
	PIDController *pid = NULL;
	uint8_t        i;
	uint8_t        is_set;
	uint8_t        is_all;

	/* 分隔符带上 \r\n，行尾换行就不用单独剥掉 */
	char *p_act   = strtok(cmd, ":\r\n");
	char *p_name  = strtok(NULL, ":\r\n");
	char *p_param = strtok(NULL, ":\r\n");
	char *p_value = strtok(NULL, ":\r\n");

	/* 第 1 段：动作只能是 SET / GET */
	if (p_act == NULL || p_name == NULL || p_param == NULL)
	{
	    Serial_SendString("ERR:CMD\r\n");
	    return;
	}

	is_set = (strcmp(p_act, "SET") == 0);
	if (!is_set && strcmp(p_act, "GET") != 0)
	{
	    Serial_SendString("ERR:CMD\r\n");
	    return;
	}

	/* 第 2 段：按名字找已登记的 PID 环 */
	for (i = 0; i < pid_count; i++)
	{
	    if (strcmp(pid_list[i]->name, p_name) == 0)
	    {
	        pid = pid_list[i];
	        break;
	    }
	}
	if (pid == NULL)
	{
	    Serial_SendString("ERR:LOOP\r\n");
	    return;
	}

	/* 第 3 段：KP / KI / KD；GET 还可以是 ALL */
	is_all = (strcmp(p_param, "ALL") == 0);
	if (is_all && is_set)                   /* SET 不接受 ALL，一次只能改一个参数 */
	{
	    Serial_SendString("ERR:PARAM\r\n");
	    return;
	}

	if      (strcmp(p_param, "KP") == 0) p_gain = &pid->Kp;
	else if (strcmp(p_param, "KI") == 0) p_gain = &pid->Ki;
	else if (strcmp(p_param, "KD") == 0) p_gain = &pid->Kd;
	else if (strcmp(p_param, "MININT") == 0) p_gain = &pid->limMinInt;
	else if (strcmp(p_param, "MAXINT") == 0) p_gain = &pid->limMaxInt;
	else if (is_all)                     p_gain = NULL;
	else
	{
	    Serial_SendString("ERR:PARAM\r\n");
	    return;
	}

	/* GET:<环>:ALL —— 一次回发 Kp/Ki/Kd 三个 */
	if (is_all)
	{
	    Serial_SendString("GET:");
	    Serial_SendString(p_name);
	    Serial_SendString(":ALL");

	    snprintf(buf, sizeof(buf), "%.4f", pid->Kp);
	    Serial_SendString(":KP:");
	    Serial_SendString(buf);
	    snprintf(buf, sizeof(buf), "%.4f", pid->Ki);
	    Serial_SendString(":KI:");
	    Serial_SendString(buf);
	    snprintf(buf, sizeof(buf), "%.4f", pid->Kd);
	    Serial_SendString(":KD:");
	    Serial_SendString(buf);
	    Serial_SendString("\r\n");
	    return;
	}

	/* SET:<环>:<参数>:<数值> —— 写入 */
	if (is_set)
	{
	    if (p_value == NULL)
	    {
	        Serial_SendString("ERR:VALUE\r\n");
	        return;
	    }

	    float v = (float)strtod(p_value, &end);
	    if (end == p_value)                 /* 一个字符都没解析成数字 */
	    {
	        Serial_SendString("ERR:VALUE\r\n");
	        return;
	    }
	    *p_gain = v;
	}

	/* SET 成功 / GET 单个，都回发 <动作>:<环>:<参数>:<数值> */
	if (is_set) Serial_SendString("SET:");
	else        Serial_SendString("GET:");

	Serial_SendString(p_name);
	Serial_SendString(":");
	Serial_SendString(p_param);
	Serial_SendString(":");

	snprintf(buf, sizeof(buf), "%.4f", *p_gain);
	Serial_SendString(buf);
	Serial_SendString("\r\n");
}

