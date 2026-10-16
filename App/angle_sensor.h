#ifndef INC_ANGLE_SENSOR_H_
#define INC_ANGLE_SENSOR_H_

#include "main.h"

/**
 *使用前提:
 *  1.STM32CubeMX中开启ADC
 *  2.在Parameter Settings中设置ADC的Scan Conversion Mode为Enable
 *  3.Continuous Conversion Mode为Enable
 *  4.Enable Regular Conversion Conversion设置为Enable
 *  5. Number of Conversion设置为2(一个通道给旋钮，一个通道给角度传感器)
 *  6. 更改每个Rank为对应的配置
 *  7. 在DMA Settings中设置DMA Mode为Circular
 */

/**
 * @brief  读角度传感器，返回摆杆角度
 * @retval 角度（°），0° = 真竖直；实际范围 (-180°, +108.5°]
 *
 * 数据来自 adc_dma_buf[0]（PA2 / ADC1_IN2）。main 里必须先调 ADC_Start()，
 * 否则缓冲全 0，返回值恒为 +93.7°。
 */
float AngleSensor_GetAngle();

#endif

