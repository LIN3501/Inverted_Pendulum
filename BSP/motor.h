      
#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f1xx_hal.h"

#define MOTOR_PWMA_PORT    GPIOA
#define MOTOR_PWMA_PIN     GPIO_PIN_8
#define MOTOR_PWMB_PORT    GPIOA
#define MOTOR_PWMB_PIN     GPIO_PIN_11

#define MOTOR_AIN1_PORT    GPIOA
#define MOTOR_AIN1_PIN     GPIO_PIN_13
#define MOTOR_AIN2_PORT    GPIOA
#define MOTOR_AIN2_PIN     GPIO_PIN_12

#define MOTOR_BIN1_PORT    GPIOB
#define MOTOR_BIN1_PIN     GPIO_PIN_14
#define MOTOR_BIN2_PORT    GPIOB
#define MOTOR_BIN2_PIN     GPIO_PIN_15

#define MOTOR_STBY_PORT    GPIOB
#define MOTOR_STBY_PIN     GPIO_PIN_13

typedef enum {
    MOTOR_A = 0,  // 左电机
    MOTOR_B = 1   // 右电机
} MotorID;

typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_BACKWARD
} MotorDirection;

typedef struct {
    TIM_HandleTypeDef *htim;    // PWM定时器句柄
    uint32_t channel;           // PWM通道
    MotorID id;                 // 电机ID
    int16_t speed;              // 当前速度 (-10000 ~ 10000)
} Motor_HandleTypeDef;

void Motor_Init(Motor_HandleTypeDef *hmotor, TIM_HandleTypeDef *htim, uint32_t channel, MotorID id);
void Motor_SetSpeed(Motor_HandleTypeDef *hmotor, int16_t speed);
void Motor_SetDirection(Motor_HandleTypeDef *hmotor, MotorDirection dir);
void Motor_Stop(Motor_HandleTypeDef *hmotor);
void Motor_Enable(void);
void Motor_Disable(void);

// ===== 编码器（原 encoder 库并入）=====

typedef enum {
    ENCODER_A = 0,  // 左电机编码器
    ENCODER_B = 1   // 右电机编码器
} EncoderID;

typedef struct {
    TIM_HandleTypeDef *htim;           // 定时器句柄
    EncoderID id;                      // 编码器ID
    int32_t count;                     // 当前计数值
    int32_t last_count;                // 上一次计数值
    float speed;                       // 当前速度 (脉冲/秒)
    uint32_t last_time;                // 上一次采样时间 (ms)
    uint16_t pulses_per_rev;           // 每转脉冲数
    float wheel_diameter;              // 车轮直径 (mm)
} Encoder_HandleTypeDef;

void Encoder_Init(Encoder_HandleTypeDef *hencoder, TIM_HandleTypeDef *htim, EncoderID id);
int32_t Encoder_GetCount(Encoder_HandleTypeDef *hencoder);
void Encoder_ResetCount(Encoder_HandleTypeDef *hencoder);
float Encoder_GetSpeed(Encoder_HandleTypeDef *hencoder);
float Encoder_GetDistance(Encoder_HandleTypeDef *hencoder);
void Encoder_Update(Encoder_HandleTypeDef *hencoder);

extern Encoder_HandleTypeDef hencoder;   /* 编码器实例，main.c 里定义和初始化 */

#endif

    