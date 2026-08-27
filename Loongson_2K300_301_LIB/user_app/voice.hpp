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
 *                              函数声明
 *============================================================================*/
// 发送语音指令
void Voice_Play_ID(uint16_t id);                    // 播放指定 ID 的语音
void Voice_Send_String(const char* str);             // 向语音模块发送字符串
void Voice_Play_Current_Node(int map_id, int current_id);  // 播放到站语音
void Voice_Play_Arrived(void);                       // 到达终点：发送END指令

// 接收语音指令
void Voice_Rx_Process(const char* cmd);              // 处理语音指令

#endif /* __VOICE_HPP */
