#include "lq_all_demo.hpp"
#include "lq_display_tft18.hpp"

/********************************************************************************
 * @file    lq_tft18_demo.cpp
 * @brief   TFT18 测试.
 * @author  龙邱科技-012
 * @date    2026-01-10
 * @version V2.1.0
 * @note    适用与龙芯 2K0300/0301 平台.
 *          本 demo 实现 TFT18 功能，用于测试 TFT18 控制器的基本功能.
 ********************************************************************************/

/********************************************************************************
 * @brief   TFT18 测试.
 * @param   none.
 * @return  none.
 * @note    none.
 ********************************************************************************/
void lq_tft18_demo()
{
    lq_tft18_drv_init(1);

    while (ls_system_running.load()) {
        lq_tft18_drv_cls(U16BLUE);
        sleep(1);
        lq_tft18_drv_fill_area(10, 20, 30, 40, U16YELLOW);
        sleep(1);
        lq_tft18_drv_draw_line(10, 20, 30, 40, U16RED);
        sleep(1);
        lq_tft18_drv_draw_circle(50, 50, 30, U16BLACK);
        sleep(1);
    }
}