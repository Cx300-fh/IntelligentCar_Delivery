#include "lq_all_demo.hpp"

/********************************************************************************
 * @file    lq_rgb_led_demo.cpp
 * @brief   三色 RGB 灯控制程序.
 * @author  龙邱科技-012
 * @date    2026-01-10
 * @version V2.1.0
 * @note    适用与龙芯 2K0300/0301 平台
 *!         本 demo 实现 GPIO 控制 RGB 灯的示例程序.
 ********************************************************************************/

/********************************************************************************
 * @brief   按键控制三色 RGB 灯.
 * @param   none.
 * @return  none.
 * @note    三个按键控制 RGB 灯的三种颜色亮灭.
 ********************************************************************************/
void lq_rgb_led_demo(void)
{
    // 初始化 GPIO 引脚为输出模式, 用以控制 RGB 灯
    ls_gpio R(PIN_2, GPIO_MODE_OUT);
    ls_gpio G(PIN_1, GPIO_MODE_OUT);
    ls_gpio B(PIN_3, GPIO_MODE_OUT);

    // 初始化 GPIO 引脚为输入模式, 用以控制按键
    ls_gpio K1(PIN_44, GPIO_MODE_IN);
    ls_gpio K2(PIN_45, GPIO_MODE_IN);
    ls_gpio K3(PIN_80, GPIO_MODE_IN);


    while (ls_system_running.load())
    {
        // 按键一控制红色灯的亮灭
        if (K1.gpio_level_get() == GPIO_HIGH) {
            R.gpio_level_set(GPIO_HIGH);
        } else {
            R.gpio_level_set(GPIO_LOW);
        }
        // 按键二控制绿色灯的亮灭
        if (K2.gpio_level_get() == GPIO_HIGH) {
            G.gpio_level_set(GPIO_HIGH);
        } else {
            G.gpio_level_set(GPIO_LOW);
        }
        // 按键三控制蓝色灯的亮灭
        if (K3.gpio_level_get() == GPIO_HIGH) {
            B.gpio_level_set(GPIO_HIGH);
        } else {
            B.gpio_level_set(GPIO_LOW);
        }
        usleep(5000);
    }
}
