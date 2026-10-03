      
#include "motor.h"
#include <stdlib.h>

//电机初始化
void Motor_Init(Motor_HandleTypeDef *hmotor, TIM_HandleTypeDef *htim, uint32_t channel, MotorID id)
{
    hmotor->htim = htim;
    hmotor->channel = channel;
    hmotor->id = id;
    hmotor->speed = 0;

    /* 启动PWM输出 */
    HAL_TIM_PWM_Start(htim, channel);

    /* 设置初始占空比为0 */
    __HAL_TIM_SET_COMPARE(htim, channel, 0);

    /* 初始化方向引脚为停止状态 */
    if (id == MOTOR_A) {
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_RESET);
    }

    /* 使能电机驱动芯片 */
    Motor_Enable();
}

//设置电机速度
void Motor_SetSpeed(Motor_HandleTypeDef *hmotor, int16_t speed)
{
    uint16_t pwm_value;

    if (speed > 1000) speed = 1000;
    if (speed < -1000) speed = -1000;

    hmotor->speed = speed;

    if (speed > 0) {
        Motor_SetDirection(hmotor, MOTOR_FORWARD);
    } else if (speed < 0) {
        Motor_SetDirection(hmotor, MOTOR_BACKWARD);
    } else {
        Motor_Stop(hmotor);
        return;
    }

    /* 计算PWM值（假设定时器周期为1000） */
    pwm_value = (uint16_t)(abs(speed));

    /* 设置PWM占空比 */
    __HAL_TIM_SET_COMPARE(hmotor->htim, hmotor->channel, pwm_value);
}

//设置电机方向
void Motor_SetDirection(Motor_HandleTypeDef *hmotor, MotorDirection dir)
{
    if (hmotor->id == MOTOR_A) {
        /* 电机A方向控制 */
        switch(dir) {
            case MOTOR_FORWARD:
                HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_SET);
                HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
                break;
            case MOTOR_BACKWARD:
                HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_SET);
                break;
            case MOTOR_STOP:
            default:
                HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
                break;
        }
    } else {
        /* 电机B方向控制 */
        switch(dir) {
            case MOTOR_FORWARD:
                HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_SET);
                HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_RESET);
                break;
            case MOTOR_BACKWARD:
                HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_SET);
                break;
            case MOTOR_STOP:
            default:
                HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_RESET);
                break;
        }
    }
}

//停止电机
void Motor_Stop(Motor_HandleTypeDef *hmotor)
{
    /* 设置占空比为0 */
    __HAL_TIM_SET_COMPARE(hmotor->htim, hmotor->channel, 0);

    /* 设置方向引脚为停止状态 */
    Motor_SetDirection(hmotor, MOTOR_STOP);

    hmotor->speed = 0;
}

//使能电机驱动芯片（STBY引脚置高）
void Motor_Enable(void)
{
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);
}

//禁用电机驱动芯片（STBY引脚置低）
void Motor_Disable(void)
{
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_RESET);
}

//===== 编码器（原 encoder 库并入）=====

//编码器初始化
void Encoder_Init(Encoder_HandleTypeDef *hencoder, TIM_HandleTypeDef *htim, EncoderID id)
{
    hencoder->htim = htim;
    hencoder->id = id;
    hencoder->count = 0;
    hencoder->last_count = 0;
    hencoder->speed = 0.0f;
    hencoder->last_time = HAL_GetTick();
    hencoder->pulses_per_rev = 11;  // 默认每转11个脉冲（需根据实际编码器调整）
    hencoder->wheel_diameter = 65.0f; // 默认车轮直径65mm（需根据实际车轮调整）

    /* 启动编码器模式 */
    HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);

    /* 重置计数器 */
    __HAL_TIM_SET_COUNTER(htim, 0);
}

//获取编码器计数值
int32_t Encoder_GetCount(Encoder_HandleTypeDef *hencoder)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(hencoder->htim);
}

//重置编码器计数值
void Encoder_ResetCount(Encoder_HandleTypeDef *hencoder)
{
    __HAL_TIM_SET_COUNTER(hencoder->htim, 0);
    hencoder->count = 0;
    hencoder->last_count = 0;
    hencoder->speed = 0.0f;
}

//获取编码器速度（脉冲/秒）
float Encoder_GetSpeed(Encoder_HandleTypeDef *hencoder)
{
    return hencoder->speed;
}

//获取累计行驶距离（mm）
float Encoder_GetDistance(Encoder_HandleTypeDef *hencoder)
{
    /* 计算公式：距离 = (脉冲数 / 每转脉冲数) * π * 车轮直径 */
    float distance = ((float)hencoder->count / (float)hencoder->pulses_per_rev) * 3.1415926f * hencoder->wheel_diameter;
    return distance;
}

//更新编码器数据（计算速度）
void Encoder_Update(Encoder_HandleTypeDef *hencoder)
{
    uint32_t current_time = HAL_GetTick();
    uint32_t delta_time = current_time - hencoder->last_time;

    if (delta_time > 0) {
        /* 获取当前计数值 */
        hencoder->count = Encoder_GetCount(hencoder);

        /* 计算脉冲变化量 */
        int32_t delta_count = hencoder->count - hencoder->last_count;

        /* 计算速度（脉冲/秒） */
        hencoder->speed = (float)delta_count / (float)delta_time;

        /* 更新上次计数值和时间 */
        hencoder->last_count = hencoder->count;
        hencoder->last_time = current_time;
    }
}

    