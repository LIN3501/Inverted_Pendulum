/**
  ******************************************************************************
  * @file    menu.c
  * @brief   模式选择菜单：一页 4 行菜单（y=0/2/4/6）+ 光标，电位器移光标，KEY0 进入，KEY1 翻页
  ******************************************************************************
  * 静态文字只在进菜单/翻页时整页重画，光标移动只画两个字符，
  * 避免整屏重刷（OLED 每字符约 6.4ms，整屏 ~180ms）卡主循环
  ******************************************************************************
  */

#include "menu.h"
#include "key.h"
#include "oled.h"

/* 菜单每行显示的文字，第 0 列留给光标 '>'，所以每行最多 15 字符 */
static const char *mode_names[] = {
    "Mode1:Angle Cal",
    "Mode2:VOFA Tx",
    "Mode3:Open Loop",
    "Mode4:Balance",
    "Mode5:Swing Up",
};

/* 菜单项数 = 数组长度，加一项只改 mode_names 即可 */
#define MENU_ITEM_NUM  ((int)(sizeof(mode_names) / sizeof(mode_names[0])))

static int menu_page = 0;       /* 菜单当前页，0 起 */
static int menu_row = -1;       /* 光标所在行 0~3，-1 = 需要整页重画 */

/* 画菜单并返回选中的模式号（1~MENU_ITEM_NUM），还停在菜单则返回 0。
 * screen_mode/mode 传进来而不是自己存一份：菜单和 Mode_1~5 共用同一个
 * "屏幕上画的是哪个模式"的记录（mode.c 里的 screen_mode），
 * 各存一份会出现 Mode_1 -> 菜单 -> Mode_1 时静态文字不再重画 */
int Menu_Update(int *screen_mode, int mode)
{
    int row = Potentiometer_Read(0, MENU_ROWS - 1);   /* 电位器 -> 本页行 0~3 */

    /* 最后一页可能不满 4 行：光标行钳在有效范围内（第 2 页只有 Mode5 一行） */
    int rows_on_page = MENU_ITEM_NUM - menu_page * MENU_ROWS;
    if (rows_on_page > MENU_ROWS) rows_on_page = MENU_ROWS;
    if (row >= rows_on_page) row = rows_on_page - 1;

    if (*screen_mode != mode)                          /* 刚进菜单：回第 1 页并整页重画 */
    {
        *screen_mode = mode;
        menu_page = 0;
        menu_row = -1;
    }

    if (menu_row < 0)                                 /* 进菜单或翻页后：整页画 4 行 + 光标 */
    {
        for (int i = 0; i < MENU_ROWS; i++)
        {
            OLED_ShowString(0, i * 2, (uint8_t *)" ", 16);          /* 清光标列 */
            if (menu_page * MENU_ROWS + i < MENU_ITEM_NUM)          /* 最后一页空行不画名字 */
            {
                OLED_ShowString(8, i * 2, (uint8_t *)mode_names[menu_page * MENU_ROWS + i], 16);
            }
        }
        menu_row = row;
        OLED_ShowString(0, menu_row * 2, (uint8_t *)">", 16);
    }
    else if (menu_row != row)                          /* 电位器动了才挪光标 */
    {
        OLED_ShowString(0, menu_row * 2, (uint8_t *)" ", 16);       /* 擦旧光标 */
        OLED_ShowString(0, row * 2, (uint8_t *)">", 16);            /* 画新光标 */
        menu_row = row;
    }

    if (Key_Press(MENU_KEY_PAGE))                     /* KEY1：切下一页，页数不够时循环回本页 */
    {
        menu_page = (menu_page + 1) % ((MENU_ITEM_NUM + MENU_ROWS - 1) / MENU_ROWS);
        menu_row = -1;                                /* 逼下一轮整页重画 */
    }

    if (Key_Press(MENU_KEY_ENTER))                    /* KEY0：返回光标选中的模式号 */
    {
        return menu_page * MENU_ROWS + row + 1;
    }

    return 0;                                         /* 还停在菜单 */
}