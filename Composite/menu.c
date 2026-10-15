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
static const char *menu_item_label[] = {
    "Mode1:Angle Cal",
    "Mode2:VOFA Tx",
    "Mode3:Open Loop",
    "Mode4:Balance",
    "Mode5:Swing Up",
};

/* 菜单总项数 = 5，来源是 menu_item_label 数组的元素个数（不是字节数）：
 * 数组里每个元素是 4 字节的 char *，sizeof(数组)/sizeof(元素[0]) 得到项数。
 * 所以加一项只要在 menu_item_label 里多写一行，这里自动变 6，不用改数字 */
#define MENU_ITEM_COUNT  ((int)(sizeof(menu_item_label) / sizeof(menu_item_label[0])))

/* 菜单私有状态：只在本文件用，只被 Menu_Update 改，外面读写不到 */
static int current_page = 0;   /* 当前页号，0 起。
                              总页数 = (MENU_ITEM_COUNT + MENU_ROWS - 1) / MENU_ROWS = 2，
                              即第 0 页放 Mode1~Mode4，第 1 页只放 Mode5 */
static int cursor_row = -1;  /* 光标停在本页的第几行 0~3（对应 OLED 的 y = 0/2/4/6）。
                              -1 是哨兵值，含义是"整页还没画"：
                              进菜单或翻页时置 -1，逼本函数走整页重画分支 */

/* Menu_Update：刷新一帧菜单，主循环每轮调一次
 *
 * 【形参】
 *   int *screen_mode  指向 mode.c 里的全局 screen_mode，记录"屏幕上现在画的是哪个模式"。
 *                     拿它和 mode 一比就知道是不是刚进菜单（刚进则回第 0 页 + 整页重画）。
 *                     传指针进来而不是自己再存一份：菜单和 Mode_1~5 必须共用这一份记录，
 *                     各存一份会出现 Mode_1 -> 菜单 -> Mode_1 时静态文字不再重画。
 *   int mode          主循环当前所处的模式，在菜单里就是 0。
 *
 * 【返回值】（直接赋给主循环的 mode 用）
 *   1 ~ MENU_ITEM_COUNT  按下 KEY0，确认光标那一项，退出菜单并切到该模式
 *   0                  还停在菜单，主循环下一轮继续调本函数
 *
 * 【下标换算】全局项号 = current_page * MENU_ROWS + 本页行号(0~3)；模式号 = 全局项号 + 1
 */
int Menu_Update(int *screen_mode, int mode)
{
    /* select_row：电位器当前选中的行，读数线性映射到 0~3（一页 4 行里选一行） */
    int select_row = Potentiometer_Read(0, MENU_ROWS - 1);

    /* 最后一页可能不满 4 行，先算本页有几项有效菜单 = 总项数 - 前面页已用的项数，
     * 再压到一页上限 4：
     *     第 0 页  5 - 0*4 = 5  → 压成 4
     *     第 1 页  5 - 1*4 = 1  → 保持 1（残页，只有 Mode5）
     * 再把 select_row 钳进 [0, page_item_count-1]：残页上电位器指到空行也选不出东西，
     * 不钳位光标就画在空行上，按 KEY0 会返回不存在的模式号 8 */
    int page_item_count = MENU_ITEM_COUNT - current_page * MENU_ROWS;
    if (page_item_count > MENU_ROWS) page_item_count = MENU_ROWS;
    if (select_row >= page_item_count) select_row = page_item_count - 1;

    if (*screen_mode != mode)                        /* 屏幕上的模式 != 当前模式 = 刚进菜单：记下并回第 0 页，置 -1 触发整页重画 */
    {
        *screen_mode = mode;
        current_page = 0;
        cursor_row = -1;
    }

    if (cursor_row < 0)                              /* 刚进菜单/刚翻页：整页重画（菜单文字全部重写一遍） */
    {
        for (int line = 0; line < MENU_ROWS; line++) /* line = 本页行号 0~3，y = line*2 让每行隔开一行字高 */
        {
            OLED_ShowString(0, line * 2, (uint8_t *)" ", 16); /* 先把第 0 列光标位置清成空格 */
            if (current_page * MENU_ROWS + line < MENU_ITEM_COUNT) /* 本行有对应菜单项才画名字（残页的空行跳过） */
            {
                /* 全局项号 -> menu_item_label 下标，画在 x=8（给第 0 列的光标让位） */
                OLED_ShowString(8, line * 2, (uint8_t *)menu_item_label[current_page * MENU_ROWS + line], 16);
            }
        }
        cursor_row = select_row;
        OLED_ShowString(0, cursor_row * 2, (uint8_t *)">", 16); /* 光标画在第 0 列 */
    }
    else if (cursor_row != select_row)               /* 电位器动了才挪光标：只擦旧位置 + 画新位置 2 个字符，不整页刷 */
    {
        OLED_ShowString(0, cursor_row * 2, (uint8_t *)" ", 16); /* 擦旧光标 */
        OLED_ShowString(0, select_row * 2, (uint8_t *)">", 16); /* 画新光标 */
        cursor_row = select_row;
    }

    if (Key_Press(MENU_KEY_PAGE))                    /* KEY1：切下一页 */
    {
        /* 总页数 = (总项数 + 每页行数 - 1) / 每页行数 = (5+3)/4 = 2（整数除法自动向上取整），
         * % 总页数 = 到最后一页后再按一次 KEY1 回到第 1 页 */
        current_page = (current_page + 1) % ((MENU_ITEM_COUNT + MENU_ROWS - 1) / MENU_ROWS);
        cursor_row = -1;                             /* 本轮不画，等下一轮走整页重画分支 */
    }

    if (Key_Press(MENU_KEY_ENTER))                   /* KEY0：确认光标那一项，把模式号返回给主循环 */
    {
        /* 模式号 = 页号*每页行数 + 本页行号 + 1（+1 因为 mode 从 1 开始编号，菜单是 0） */
        return current_page * MENU_ROWS + select_row + 1;
    }

    return 0;                                        /* 还停在菜单 */
}
