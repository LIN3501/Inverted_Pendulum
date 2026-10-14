
#include "angle_sensor.h"
#include "adc.h"

// 定义全局变量
int AngleTick = 0;
float adc_high = 3247.0f;  // -180° 对应的 ADC 值
float adc_low = 1111.5f;   // 0°（真竖直/最高点）对应的 ADC 值（两点相距 2103 个码值 = 180°）

/* 注意：ADC1 的采集统一由 adc.c 的 ADC_Start() 负责（双通道 DMA 循环到 adc_dma_buf），
 * 这里不要再自己 HAL_ADC_Start_DMA，否则会和它抢同一个 ADC1 + DMA1_Channel1 */

float Angle_Tick(){

    float angle = 0.0f;
    // 从DMA缓冲区读取数据：[0] = PA2 (ADC1_IN2)，由 ADC_Start() 的双通道 DMA 循环刷新
    AngleTick = adc_dma_buf[0];

    /* 线性标定：ADC 1111.5 = 0°，3247 = -180°，ADC 增大角度变负 */
    angle = (adc_low - (float)AngleTick) / (adc_high - adc_low) * 180.0f;

    /* 过 -180° 后折回正半周：传感器 0/4095 处有一条换向缝，
     * 所以 +180° 紧挨着 -180°，正半周要跨缝读 */
    if(angle < -180.0f){
        angle += 360.0f;
    }

    return angle;
}

    