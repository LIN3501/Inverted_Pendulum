
/**
 * @file    key.h
 * @brief   按键 + 电位器输入（BSP 层，自写模块，可自由修改）
 *
 * 本模块管两件事：
 *   1. 按键：PB13/PB14/PB15 三个按键，EXTI 中断里只置"待处理"标志，
 *      主循环轮询 Key_Press() 取结果，配合 50ms 间隔做软件防连击。
 *   2. 电位器：ADC1 通道 2（PA6），由 adc.c 的 ADC_Start() 用循环 DMA
 *      持续搬到 adc_dma_buf[]，本模块只负责把原始值线性映射成想要的量程。
 *
 * 按键与实际引脚、菜单按键号的对应关系：
 *   key_id = 0  -> KEY0，PB13，菜单里就是 MENU_KEY_ENTER（确认）
 *   key_id = 1  -> KEY1，PB14，菜单里就是 MENU_KEY_PAGE（翻页）
 *   key_id = 2  -> KEY2，PB15，暂无菜单用途
 */

#ifndef __KEY_H
#define __KEY_H

#include "stm32f1xx_hal.h"

/**
 * 按键待处理标志（1 = 中断已捕获到按下，尚未被 Key_Press 取走）。
 * 一般不需要在外部读写，Key_Press() 内部会清零。
 */
extern int key_0_pending;
extern int key_1_pending;
extern int key_2_pending;

/**
 * @brief  查询某个按键这一轮有没有被按下（带 50ms 防连击）
 * @param  key_id 按键编号：0 = KEY0(PB13)，1 = KEY1(PB14)，2 = KEY2(PB15)
 * @retval 1  这次调用取到一次有效按下
 * @retval 0  没有新按下，或距上次取到的按下还不到 50ms（连击被丢掉）
 *
 * 用法：在主循环或周期任务里反复调用，返回 1 时做一次对应动作即可。
 * 中断（EXTI）里只置标志，真正的判断在这里做，所以本函数是阻塞的、可以随便调。
 *
 * @code
 * if (Key_Press(0)) { speed += 10; }        // KEY0：加速
 * if (Key_Press(1)) { speed -= 10; }        // KEY1：减速
 * @endcode
 */
int Key_Press(int key_id);

/**
 * @brief  记录电位器用的 ADC 句柄
 * @param  hadc  ADC 句柄，本项目传 &hadc1
 *
 * 只需在 main 初始化阶段调用一次，且必须在 ADC_Start() 启动 DMA 采集之前/之后均可，
 * 但 Potentiometer_Read() 调用之前一定要先调它。
 */
void Potentiometer_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief  读电位器，把 ADC 原始值线性映射到 [min, max] 区间
 * @param  min  映射下限（例如 -1500）
 * @param  max  映射上限（例如 1500），必须大于 min
 * @retval 映射并四舍五入后的整数；未初始化（没调 Potentiometer_Init）或 min>=max 时返回 min
 *
 * 底层取的是循环 DMA 缓冲 adc_dma_buf[1]（PA6 / ADC1 通道 2，12 位，0~4095）。
 * 因为是循环 DMA，这里读到的永远是最新一次采样，不阻塞、无需重新启动 ADC。
 *
 * @code
 * int pwm = Potentiometer_Read(0, 1000);      // 菜单选择：0~1000
 * int pwm = Potentiometer_Read(-1500, 1500);   // 电机输出偏置
 * @endcode
 */
int  Potentiometer_Read(int min, int max);

#endif

/*
 * 使用说明
 * 1.开启外部中断NVIC
 * 2.更改key.c中的HAL_GPIO_EXTI_Callback里的GPIO_PIN_XX为MX设置引脚
 * 3.初始化电位器Potentiometer_Init
 * 4.调用一次 ADC_Start() 启动双通道 DMA 采集（见 adc.h）
 */

