
#include "angle_sensor.h"
#include "adc.h"

// 定义全局变量
int AngleTick = 0;
float adc_high = 3247.0f;  // -180° 对应的 ADC 值
float adc_low = 1111.5f;   // 0°（真竖直/最高点）对应的 ADC 值（两点相距 2135.5 个码值 = 180°）

/* 注意：ADC1 的采集统一由 adc.c 的 ADC_Start() 负责（双通道 DMA 循环到 adc_dma_buf），
 * 这里不要再自己 HAL_ADC_Start_DMA，否则会和它抢同一个 ADC1 + DMA1_Channel1 */

float AngleSensor_GetAngle(){

    float angle = 0.0f;
    // 从DMA缓冲区读取数据：[0] = PA2 (ADC1_IN2)，由 ADC_Start() 的双通道 DMA 循环刷新
    AngleTick = adc_dma_buf[0];

    /* 线性标定：ADC 1111.5 = 0°，3247 = -180°，ADC 增大角度变负
     * 两点码值差 = 3247.0 - 1111.5 = 2135.5，码值差 2135.5 对应 180°，
     * 即换算系数 180 / 2135.5 ≈ 0.0843 °/码 */
    angle = (adc_low - (float)AngleTick) / (adc_high - adc_low) * 180.0f;

    /* 折回正半周：角度算到 -180° 以下（也就是 ADC 码值大于 adc_high = 3247）时 +360，
     * 把负半周的值搬回正半周，避免跨传感器换向点时角度从 -180° 突跳到 +180°。
     * 折回后的上界受 ADC 量程限制：码值最大 4095 对应 -251.5°，折回 +108.5°，
     * 所以本函数实际返回范围是 (-180°, +108.5°]，而不是名义上的 (-180°, 180°]。 */
    if(angle < -180.0f){
        angle += 360.0f;
    }

    return angle;
}

    