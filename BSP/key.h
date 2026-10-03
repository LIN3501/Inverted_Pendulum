
#ifndef __KEY_H
#define __KEY_H

#include "stm32f1xx_hal.h"

// 声明外部变量
extern int key_0;
extern int key_1;
extern int key_2;


int Key_Press(int key);

void Potentiometer_Init(ADC_HandleTypeDef *hadc);

int  Potentiometer_Read(int min, int max);

#endif

/*
 * 使用说明
 * 1.开启外部中断NVIC
 * 2.更改key.c中的HAL_GPIO_EXTI_Callback里的GPIO_PIN_XX为MX设置引脚
 * 3.初始化电位器Potentiometer_Init
 * 4.调用一次 ADC_Start() 启动双通道 DMA 采集（见 adc.h）
 */

