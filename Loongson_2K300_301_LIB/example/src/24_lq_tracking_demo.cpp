#include "lq_all_demo.hpp"

/********************************************************************************
 * @file    lq_tracking_demo.cpp
 * @brief   8 路灰度循迹传感器 demo.
 * @author  龙邱科技-012
 * @date    2026-04-21
 * @version V2.1.0
 * @note    适用与龙芯 2K0300/0301 平台.
 *          本 demo 实现 8 路灰度循迹传感器功能.
 ********************************************************************************/

/********************************************************************************
 * @brief   8 路灰度循迹传感器 demo.
 * @param   none.
 * @return  none.
 * @note    本 demo 实现 8 路灰度循迹传感器功能.
 ********************************************************************************/
void lq_tracking_demo(void)
{
    // 初始化 8 路灰度循迹传感器，数据引脚连接到 PIN_75, PIN_67, PIN_88 (可任意修改)
    lq_tracking tracking(LS_ADC_CH5, PIN_77, PIN_89, PIN_88);

    printf("\n");
    while (ls_system_running.load())
    {
        // 打印 8 路传感器的采样值前, 先调用 get_polling_value 进行一轮采集和处理, 确保 get_value 获取到的是最新数据
        tracking.get_polling_value();
        printf("\rval : %03d, %03d, %03d, %03d, %03d, %03d, %03d, %03d",
               tracking.get_value(0), tracking.get_value(1), tracking.get_value(2), tracking.get_value(3),
               tracking.get_value(4), tracking.get_value(5), tracking.get_value(6), tracking.get_value(7));
        fflush(stdout);
        usleep(100 * 1000);
    }
}
