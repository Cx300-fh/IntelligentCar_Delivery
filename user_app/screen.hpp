/**
 * @file screen.hpp
 * @brief 陶晶驰串口屏模块头文件
 * @details 实现与陶晶驰串口屏的通信：发送数据、接收指令
 */

#ifndef __SCREEN_HPP
#define __SCREEN_HPP

#include "include.hpp"

/*============================================================================
 *                              串口配置
 *============================================================================*/
// 陶晶驰串口屏: UART5, PIN64/PIN65, 波特率 115200
#define SCREEN_UART_PIN     UART5_PIN64
#define SCREEN_UART_BAUD    230400

/*============================================================================
 *                              屏幕设定目标变量
 *============================================================================*/
// 屏幕通过AIM指令设定的地图和目标点（等待START指令后启动）
// 这些变量定义在 screen.cpp 中
extern int screen_selected_map;      // 屏幕选择的地图ID（0=未选择, 1=THU, 2=SUTD）
extern int screen_selected_target;   // 屏幕选择的目标点ID（0=未选择）
extern bool screen_target_ready;     // 是否已选择目标（用于判断能否启动）

/*============================================================================
 *                              函数声明
 *============================================================================*/
// 发送数据到屏幕
void Screen_Send_Var(const char* name, int value);           // 发送整型变量到屏幕
void Screen_Send_Text(const char* name, const char* text);   // 发送文本到屏幕
void Screen_Send_Page(uint8_t page_id);                      // 切换屏幕页面
void Screen_Send_All(void);                                  // 发送所有导航数据到屏幕
void Screen_Send_Heartbeat(void);                            // 发送心跳包（保持屏幕在线状态）

// 接收屏幕指令
void Screen_Rx_Process(const uint8_t* data, ssize_t len);    // 处理屏幕接收数据

#endif /* __SCREEN_HPP */
