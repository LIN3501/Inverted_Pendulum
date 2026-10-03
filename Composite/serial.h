
#ifndef __SERIAL_H__
#define __SERIAL_H__

#include "main.h"

#define RX_BUFFER_SIZE 128

extern uint8_t rx_buffer[RX_BUFFER_SIZE];
extern volatile uint8_t rx_flag;
extern volatile uint8_t rx_len;

extern float data1;
extern float data2;
extern float data3;
extern float data4;
extern float data5;
extern float data6;
extern float data7;
extern float data8;
extern float data9;
extern float data10;
extern float data11;
extern float data12;

extern float rx_num1;  // 发送通道1 = data1 + data2 + data3
extern float rx_num2;  // 发送通道2 = data4 + data5 + data6
extern float rx_num3;  // 发送通道3 = data7 + data8 + data9
extern float rx_num4;  // 发送通道4 = data10 + data11 + data12

void Serial_Init(void);
void Serial_SendByte(uint8_t byte);
void Serial_SendString(char *str);
void Serial_SendFireWater(float ch0, float ch1, float ch2, float ch3, float ch4, float ch5);
void Serial_IRQHandler(void);
void Serial_ProcessData(void);
void Serial_UpdateTxData(void);

#endif /* __SERIAL_H__ */

