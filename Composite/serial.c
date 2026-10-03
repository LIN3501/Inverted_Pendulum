/*使用说明：
  * 1. 初始化调用：Serial_Init()
  * 2. 在USART1_IRQHandler()中调用：Serial_IRQHandler()
  * 3. 发送数据调用：Serial_SendFireWater()
  * 4. 处理接收数据调用：Serial_ProcessData()
  * 5. 更新发送变量调用：Serial_UpdateTxData()
  * 数据格式（FireWater协议）：
  * - 接收格式：数值1,数值2,数值3,...,数值12
  * - 发送格式：ch0,ch1,ch2,ch3,ch4,ch5\n
 */

#include "serial.h"
#include "usart.h"
#include "pid.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define UART huart1

uint8_t rx_buffer[RX_BUFFER_SIZE];
volatile uint8_t rx_flag = 0;
volatile uint8_t rx_len = 0;
static uint8_t rx_idx = 0;

float data1 = 0.0f;
float data2 = 0.0f;
float data3 = 0.0f;
float data4 = 0.0f;
float data5 = 0.0f;
float data6 = 0.0f;
float data7 = 0.0f;
float data8 = 0.0f;
float data9 = 0.0f;
float data10 = 0.0f;
float data11 = 0.0f;
float data12 = 0.0f;

float rx_num1 = 0.0f;  // 发送通道1 = data1 + data2 + data3
float rx_num2 = 0.0f;  // 发送通道2 = data4 + data5 + data6
float rx_num3 = 0.0f;  // 发送通道3 = data7 + data8 + data9
float rx_num4 = 0.0f;  // 发送通道4 = data10 + data11 + data12

//串口初始化
void Serial_Init(void)
{
    __HAL_UART_ENABLE_IT(&UART, UART_IT_IDLE);
    __HAL_UART_ENABLE_IT(&UART, UART_IT_RXNE);
}

//发送一个字节
void Serial_SendByte(uint8_t byte)
{
    while(!__HAL_UART_GET_FLAG(&UART, UART_FLAG_TXE)) { }  // 忙等发送寄存器空
    UART.Instance->DR = byte;
}

//发送字符串
void Serial_SendString(char *str)
{
    while(*str)
    {
        Serial_SendByte(*str++);
    }
}

//浮点数转字符串并发送（内部使用）
static void Serial_SendFloat(float data)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.4f", data);
    Serial_SendString(buffer);
}

//VOFA+ FireWater协议发送6通道波形数据
//ch0: 通道0数据
//ch1: 通道1数据
//ch2: 通道2数据
//ch3: 通道3数据
//ch4: 通道4数据
//ch5: 通道5数据
void Serial_SendFireWater(float ch0, float ch1, float ch2, float ch3, float ch4, float ch5)
{
    Serial_SendFloat(ch0);    // 发送通道0
    Serial_SendByte(',');      // 发送分隔符逗号
    Serial_SendFloat(ch1);    // 发送通道1
    Serial_SendByte(',');      // 发送分隔符逗号
    Serial_SendFloat(ch2);    // 发送通道2
    Serial_SendByte(',');      // 发送分隔符逗号
    Serial_SendFloat(ch3);    // 发送通道3
    Serial_SendByte(',');      // 发送分隔符逗号
    Serial_SendFloat(ch4);    // 发送通道4
    Serial_SendByte(',');      // 发送分隔符逗号
    Serial_SendFloat(ch5);    // 发送通道5
    Serial_SendByte('\n');     // 发送帧结束符换行
}

//串口中断处理函数
void Serial_IRQHandler(void)
{
    if(__HAL_UART_GET_FLAG(&UART, UART_FLAG_RXNE))
    {
        if(rx_idx < RX_BUFFER_SIZE - 1)
        {
            rx_buffer[rx_idx++] = UART.Instance->DR;
        }
        else
        {
            (void)UART.Instance->DR;
        }
    }

    if(__HAL_UART_GET_FLAG(&UART, UART_FLAG_IDLE))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&UART);

        rx_len = rx_idx;

        if(rx_len > 0)
        {
            rx_buffer[rx_len] = '\0';
        }

        rx_flag = 1;
        rx_idx = 0;
    }
}

//处理接收到的数据
void Serial_ProcessData(void)
{
    if(rx_flag)
    {
        rx_flag = 0;

        /* 调参命令 SET:... / GET:... 交给 PID 库解析，其余仍按 FireWater 的逗号格式拆 */
        if (strncmp((char *)rx_buffer, "SET:", 4) == 0 ||
            strncmp((char *)rx_buffer, "GET:", 4) == 0)
        {
            PID_ProcessCommand((char *)rx_buffer);
            return;
        }

        char *token;

        //解析数据
        token = strtok((char*)rx_buffer, ",");
        if(token) data1 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data2 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data3 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data4 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data5 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data6 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data7 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data8 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data9 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data10 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data11 = (float)strtod(token, NULL);

        token = strtok(NULL, ",");
        if(token) data12 = (float)strtod(token, NULL);
    }
}

//更新发送变量（可视情况更改）
void Serial_UpdateTxData(void)
{
    rx_num1 = data1 + data2 + data3;
    rx_num2 = data4 + data5 + data6;
    rx_num3 = data7 + data8 + data9;
    rx_num4 = data10 + data11 + data12;
}

