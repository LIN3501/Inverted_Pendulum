
#include "key.h"
#include "adc.h"

int key_0_pending=0;
int key_1_pending=0;
int key_2_pending=0;

static uint32_t key_0_last_time=0;
static uint32_t key_1_last_time=0;
static uint32_t key_2_last_time=0;

#define DEBOUNCE_TIME 50

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

static ADC_HandleTypeDef *potentiometer_adc = NULL;

void Potentiometer_Init(ADC_HandleTypeDef *hadc)
{
    potentiometer_adc = hadc;
}

int Potentiometer_Read(int min, int max)
{
    if(potentiometer_adc == NULL || min >= max)
    {
        return min;
    }

    /* PA6 是 rank2，取 DMA 缓冲区第 2 格 */
    return min + (int)((float)adc_dma_buf[1] / 4095.0f * (max - min) + 0.5f);
}

