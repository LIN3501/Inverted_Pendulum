      
#ifndef __OLED_H__
#define __OLED_H__

#include "stm32f1xx_hal.h"

// OLED 控制引脚宏定义（虽然实际使用 I2C HAL，但保留以兼容注释）
#define OLED_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define GPIOx_OLED_PORT GPIOB
#define OLED_SCK_PIN GPIO_PIN_8
#define OLED_SDA_PIN GPIO_PIN_9

// 函数声明
void WriteCmd(void);
void OLED_WR_CMD(uint8_t cmd);
void OLED_WR_DATA(uint8_t data);
void OLED_Init(void);
void OLED_Clear(void);

void OLED_ShowNum(uint8_t x, uint8_t y, unsigned int num, uint8_t len, uint8_t size2);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t Char_Size);
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *chr, uint8_t Char_Size);

#endif /* __OLED_H__ */

    