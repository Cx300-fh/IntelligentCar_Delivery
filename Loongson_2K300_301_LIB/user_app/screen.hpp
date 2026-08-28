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

/*============================================================================
 *                       配送模式：屏幕事件队列（阶段6）
 *============================================================================*/
// 串口屏线程产生（点击DConfirm页按钮），主线程消费（配送协调器转为user_action）
// 线程规则：串口屏线程只入队，不直接操作导航和订单
enum ScreenEvent {
    SCREEN_EV_NONE = 0,
    SCREEN_EV_LOAD_CONFIRMED,      // "物品已装好"：订单 2→3 确认
    SCREEN_EV_UNLOAD_CONFIRMED,    // "物品已取走"：订单 4→5 确认
    SCREEN_EV_STOP,                // 急停（inhibit已在串口线程置位，此事件供协调器记录）
};

// 主线程：非阻塞取一条屏幕事件（无事件返回false）
bool Screen_Poll_Event(ScreenEvent* out);
// 串口屏线程：入队一条屏幕事件（队列满丢弃）
void Screen_Push_Event(ScreenEvent ev);

/*============================================================================
 *                       配送模式：快照渲染（阶段6）
 *============================================================================*/
// 主线程调用：把服务器订单快照写到DConfirm页（active_slot/active_phase/订单号/文案/按钮使能）
// 内部按snapshot_version变化触发+兜底周期刷新，串口发送已节流
void Screen_Render_Delivery(void);

// 配送模式启动时调用一次：切到DConfirm页
void Screen_Enter_Delivery_Page(void);

#endif /* __SCREEN_HPP */
