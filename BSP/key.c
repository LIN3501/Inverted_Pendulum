
/**
 * @file    key.c
 * @brief   按键 + 电位器输入实现，详见 key.h
 *
 * 按键流程分两步：
 *   PB13/14/15 下降沿中断 -> HAL_GPIO_EXTI_Callback() 只把对应标志位置 1
 *   -> 主循环轮询 Key_Press()，通过 50ms 时间判断决定这次算不算一次有效按下
 * 这样中断里不做耗时逻辑，真正使用按键的地方（菜单、控制环）都能直接调 Key_Press()。
 */

#include "key.h"
#include "adc.h"

/* 中断置 1，Key_Press() 取走后清 0 */
int key_0_pending=0;
int key_1_pending=0;
int key_2_pending=0;

/* 上一次取到有效按下的时刻（ms），用于防连击 */
static uint32_t key_0_last_time=0;
static uint32_t key_1_last_time=0;
static uint32_t key_2_last_time=0;

/* 两次有效按下的最小间隔（ms）：50ms 内的再次按下视为连击，丢弃 */
#define DEBOUNCE_TIME 50

/**
 * @brief  按键 EXTI 中断回调（HAL 在 EXTI_IRQHandler 里回调）
 * @param  GPIO_Pin 触发中断的引脚
 *
 * 中断上下文里只做置标志，不在这里判断、不消抖、不写 OLED。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch(GPIO_Pin)
    {
        case GPIO_PIN_13:
            key_0_pending=1;
            break;
        case GPIO_PIN_14:
            key_1_pending=1;
            break;
        case GPIO_PIN_15:
            key_2_pending=1;
            break;
        default:
            break;
    }
}

/**
 * @brief  取一次按键按下事件，详见 key.h
 *
 * 三个按键各判各的：标志位为 1 且距上次取到按下已超过 DEBOUNCE_TIME 才算一次有效按下，
 * 取走后清标志并更新时间戳，保证长按/抖动不会连续触发。
 */
int Key_Press(int key_id)
{
    uint32_t current_time = HAL_GetTick();

    if(key_id==0 && key_0_pending==1 && (current_time - key_0_last_time > DEBOUNCE_TIME))
    {
        key_0_pending=0;
        key_0_last_time = current_time;
        return 1;
    }
    else if(key_id==1 && key_1_pending==1 && (current_time - key_1_last_time > DEBOUNCE_TIME))
    {
        key_1_pending=0;
        key_1_last_time = current_time;
        return 1;
    }
    else if(key_id==2 && key_2_pending==1 && (current_time - key_2_last_time > DEBOUNCE_TIME))
    {
        key_2_pending=0;
        key_2_last_time = current_time;
        return 1;
    }
    return 0;
}

/* 电位器用的 ADC 句柄，NULL 表示还没初始化 */
static ADC_HandleTypeDef *potentiometer_adc = NULL;

/**
 * @brief  保存电位器的 ADC 句柄，详见 key.h
 */
void Potentiometer_Init(ADC_HandleTypeDef *hadc)
{
    potentiometer_adc = hadc;
}

/**
 * @brief  读电位器并映射到 [min, max]，详见 key.h
 */
int Potentiometer_Read(int min, int max)
{
    if(potentiometer_adc == NULL || min >= max)
    {
        return min;
    }

    /* PA6 是 rank2，取 DMA 缓冲区第 2 格 */
    return min + (int)((float)adc_dma_buf[1] / 4095.0f * (max - min) + 0.5f);
}

