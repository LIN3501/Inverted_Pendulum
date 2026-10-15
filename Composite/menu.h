/**
  ******************************************************************************
  * @file    menu.h
  * @brief   OLED 菜单：一页 4 行 + 光标，电位器移光标，KEY0 进入，KEY1 翻页
  ******************************************************************************
  * 使用说明
  *   1. 主循环里 mode = 0（菜单）时调 Menu_Update(&screen_mode, mode)，
  *      返回值直接赋给 mode：1~5 = 选中的模式号，0 = 还停在菜单
  *   2. 加一项菜单：改 menu.c 里的 menu_item_label 数组（项数由数组长度决定），
  *      同时把 mode.h 的 MODE_MAX 改成新项数，再在 mode.c 里照着 Mode_1
  *      写一个 Mode_x
  ******************************************************************************
  */

#ifndef __MENU_H
#define __MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* 按键：KEY0 = key.c 里的 key_0_pending，也就是 PC13（CubeMX 里标为 KEY3） */
#define MENU_KEY_ENTER  0
#define MENU_KEY_PAGE   1       /* KEY1：菜单里切换下一页 */

#define MENU_ROWS       4       /* 一页 4 行，分别画在 y=0/2/4/6 */

int Menu_Update(int *screen_mode, int mode);

#ifdef __cplusplus
}
#endif

#endif /* __MENU_H */