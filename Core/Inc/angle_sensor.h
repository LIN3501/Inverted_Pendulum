
#ifndef INC_ANGLE_SENSOR_H_
#define INC_ANGLE_SENSOR_H_

#include "main.h"

/* ADC1 的采集统一由 adc.c 的 ADC_Start() 负责，这里只做原始值到角度的换算 */
float Angle_Tick();

#endif

