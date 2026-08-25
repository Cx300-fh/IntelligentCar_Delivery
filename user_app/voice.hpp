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
// ASRPRO 语音模块: UART3, PIN46/PIN47, 波特率 115200
#define VOICE_UART_PIN      UART3_PIN46
#define VOICE_UART_BAUD     115200

/*============================================================================
 *                              播报事件定义
 *============================================================================*/
enum VoiceLanguage {
    VOICE_LANG_ZH = 0,
    VOICE_LANG_EN = 1
};

enum VoiceEvent {
    VOICE_EVENT_NONE = 0,
    VOICE_EVENT_PICKUP_ARRIVED,
    VOICE_EVENT_DROPOFF_ARRIVED,
    VOICE_EVENT_TASK_START,
    VOICE_EVENT_TASK_CANCEL,
    VOICE_EVENT_TASK_COMPLETE,
    VOICE_EVENT_ROUTE_UPDATED,
    VOICE_EVENT_ORDER_DELAYED,
    VOICE_EVENT_INVALID_COMMAND,
    VOICE_EVENT_MODEL_TIMEOUT
};

/*============================================================================
 *                              屏幕目标选择状态（外部引用）
 *============================================================================*/
// 定义在 screen.cpp，voice 模块需要检查是否已选择目标
extern bool screen_target_ready;

/*============================================================================
 *                              函数声明
 *============================================================================*/
// 发送语音指令
void Voice_Play_ID(uint16_t id);           // 播放指定 ID 的语音
void Voice_Play_Event(VoiceEvent event, VoiceLanguage lang = VOICE_LANG_ZH);
VoiceEvent Voice_Event_From_Name(const char* name);
const char* Voice_Get_Event_Name(VoiceEvent event);

// 接收语音指令
void Voice_Rx_Process(const char* cmd);    // 处理语音指令

#endif /* __VOICE_HPP */
