/**
 * @file voice.hpp
 * @brief ASRPRO 语音模块头文件
 * @details 实现与语音模块的通信：发送指令、接收指令
 */

#ifndef __VOICE_HPP
#define __VOICE_HPP

#include "include.hpp"

/*============================================================================
 *                              串口配置
 *============================================================================*/
// ASRPRO 语音模块: UART1, PIN42/PIN43, 波特率 115200
#define VOICE_UART_PIN      UART1_PIN42
#define VOICE_UART_BAUD     115200

/*============================================================================
 *                              屏幕目标选择状态（外部引用）
 *============================================================================*/
// 定义在 screen.cpp，voice 模块需要检查是否已选择目标
extern bool screen_target_ready;

/*============================================================================
 *                          配送停靠类型（与 StopType 数值一致）
 *============================================================================*/
#define VOICE_STOP_UNKNOWN  0
#define VOICE_STOP_PICKUP   1
#define VOICE_STOP_DROPOFF  2

/*============================================================================
 *                              函数声明
 *============================================================================*/
// 发送语音指令
void Voice_Play_ID(uint16_t id);                    // 播放指定 ID 的语音
void Voice_Send_String(const char* str);             // 向语音模块发送字符串
void Voice_Play_Current_Node(int map_id, int current_id);  // 播放到站语音
void Voice_Play_Arrived(void);                       // 到达终点：发送END指令
void Voice_Play_Pickup(void);                        // 到达取件点：发送PUT指令

// 配送协调器在goto_stop接受时通知当前stop类型；导航到站时查询决定播PUT还是END
void Voice_Notify_Stop_Type(int type);
int  Voice_Get_Stop_Type(void);

// 同一stop补发goto_stop（后端resend误判）时抑制下一次到站播报，防双播；
// 新stop的Notify会自动清除抑制
void Voice_Suppress_Next_Arrival(void);
bool Voice_Consume_Suppress_Flag(void);

// 接收语音指令
void Voice_Rx_Process(const char* cmd);              // 处理语音指令

#endif /* __VOICE_HPP */
