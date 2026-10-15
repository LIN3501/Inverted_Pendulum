/**
  ******************************************************************************
  * @file    mode.h
  * @brief   模式切换：mode 0 是模式选择，1/2/3 是各功能模式
  ******************************************************************************
  * 使用说明
  *   1. main.c 里初始化电位器：Potentiometer_Init(&hadc1);
  *   2. 主循环里 switch (mode) 分发（mode 0 = 菜单：电位器移光标，
    *      KEY0 进入选中模式，KEY1 切换下一页，菜单本体在 Composite/menu.c）：
  *        switch (mode)
  *        {
  *          case 0:  Mode_Select();      break;   // 模式选择
  *          case 1:  Mode_1();           break;
  *          case 2:  Mode_2();           break;
  *          case 3:  Mode_3();           break;
  *          case 4:  Mode_4();           break;
  *          default: mode = MODE_SELECT; break;
  *        }
    *   3. 加新模式：改 menu.c 的 menu_item_label 加一项，把 MODE_MAX 改成新项数，
  *      再在 mode.c 里照着 Mode_1 写一个 Mode_x 函数即可
  ******************************************************************************
  */

#ifndef __MODE_H
#define __MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ==========================================================================
 * 模式编号
 * ========================================================================== */
#define MODE_SELECT     0       /* 模式 0：模式选择（默认） */
#define MODE_MAX        5       /* 模式总数，与 menu.c 的 menu_item_label 项数保持一致 */

/* 当前模式，在主循环里 switch 它 */
extern int mode;

/* ==========================================================================
 * 各模式的处理函数：在里面画自己的界面、读自己的按键
 * ========================================================================== */
void Mode_Select(void);         /* 模式 0：模式选择（菜单绘制在 Composite/menu.c） */
void Mode_1(void);              /* 模式 1 */
void Mode_2(void);              /* 模式 2 */
void Mode_3(void);              /* 模式 3：电位器开环控电机 PWM，-150~150 */
void Mode_4(void);              /* 模式 4：直立环 */
void Mode_5(void);              /* 模式 5：自动起摆（相位锁定甩摆 -> 捕获 -> 直立） */

#ifdef __cplusplus
}
#endif

#endif /* __MODE_H */
