      
#include "oled.h"
#include "i2c.h"
#include "oledfont.h"  // 包含字库声明

// OLED 初始化命令序列
static const uint8_t CMD_Data[] = {
    0xAE, 0x00, 0x10, 0x40, 0xB0, 0x81, 0xFF, 0xA1, 0xA6, 0xA8, 0x3F,
    0xC8, 0xD3, 0x00, 0xD5, 0x80, 0xD8, 0x05, 0xD9, 0xF1, 0xDA, 0x12,
    0xDB, 0x30, 0x8D, 0x14, 0xAF
};

// 写入初始化命令
void WriteCmd(void)
{
    for (int i = 0; i < sizeof(CMD_Data); i++) {
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x00, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&CMD_Data[i], 1, 0x100);
    }
}

// 写命令
void OLED_WR_CMD(uint8_t cmd)
{
    HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 0x100);
}

// 写数据
void OLED_WR_DATA(uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x40, I2C_MEMADD_SIZE_8BIT, &data, 1, 0x100);
}

// OLED 初始化
void OLED_Init(void)
{
    HAL_Delay(200);
    WriteCmd();
}

// 清屏
void OLED_Clear(void)
{
    /* 一页 128 字节合成一次 I2C：原来 1048 次单字节事务（约 300ms），现在 16 次（约 95ms） */
    static uint8_t zero[128] = {0};
    uint8_t cmd[3];

    for (uint8_t i = 0; i < 8; i++) {
        cmd[0] = 0xB0 + i;
        cmd[1] = 0x00;
        cmd[2] = 0x10;
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x00, I2C_MEMADD_SIZE_8BIT, cmd, 3, 0x100);
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x40, I2C_MEMADD_SIZE_8BIT, zero, 128, 0x100);
    }
}

// 幂运算（用于数字分解）
static unsigned int oled_pow(uint8_t m, uint8_t n)
{
    unsigned int result = 1;
    while (n--) result *= m;
    return result;
}

// 显示数字
void OLED_ShowNum(uint8_t x, uint8_t y, unsigned int num, uint8_t len, uint8_t size2)
{
    uint8_t t, temp;
    uint8_t enshow = 0;
    for (t = 0; t < len; t++) {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                OLED_ShowChar(x + (size2 / 2) * t, y, ' ', size2);
                continue;
            } else {
                enshow = 1;
            }
        }
        OLED_ShowChar(x + (size2 / 2) * t, y, temp + '0', size2);
    }
}

// 显示单个字符
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t Char_Size)
{
    unsigned char c = chr - ' ';
    uint8_t cmd[3];
    uint8_t data[8];

    if (x > 128 - 1) {
        x = 0;
        y += 2;
    }

    /* 3 条定位命令合成一次 I2C、8 字节字模合成一次 I2C：
     * 16 号字原来每字符 22 次单字节事务（约 6.4ms），现在 4 次（约 2.8ms） */
    cmd[1] = ((x & 0xF0) >> 4) | 0x10;
    cmd[2] = x & 0x0F;

    if (Char_Size == 16) {
        cmd[0] = 0xB0 + y;
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x00, I2C_MEMADD_SIZE_8BIT, cmd, 3, 0x100);
        for (int i = 0; i < 8; i++) data[i] = F8X16[c * 16 + i];
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x40, I2C_MEMADD_SIZE_8BIT, data, 8, 0x100);

        cmd[0] = 0xB0 + y + 1;
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x00, I2C_MEMADD_SIZE_8BIT, cmd, 3, 0x100);
        for (int i = 0; i < 8; i++) data[i] = F8X16[c * 16 + i + 8];
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x40, I2C_MEMADD_SIZE_8BIT, data, 8, 0x100);
    } else {
        cmd[0] = 0xB0 + y;
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x00, I2C_MEMADD_SIZE_8BIT, cmd, 3, 0x100);
        for (int i = 0; i < 6; i++) data[i] = F6x8[c][i];
        HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x40, I2C_MEMADD_SIZE_8BIT, data, 6, 0x100);
    }
}

// 显示字符串
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *chr, uint8_t Char_Size)
{
    unsigned char j = 0;
    while (chr[j] != '\0') {
        OLED_ShowChar(x, y, chr[j], Char_Size);
        x += 8;
        if (x > 120) {
            x = 0;
            y += 2;
        }
        j++;
    }
}

    