/**
 * @file voice.cpp
 * @brief ASRPRO 语音模块实现
 * @details 实现与语音模块的通信：发送指令、接收指令
 *          第6部分：给语音模块发送指令
 *          第7部分：接收语音模块指令
 */

#include "voice.hpp"
#include "voice_model_client.hpp"

/*============================================================================
 *                              接收回调前向声明
 *============================================================================*/
static void voice_rx_callback(const uint8_t data);

/*============================================================================
 *                              全局对象定义
 *============================================================================*/
// ASRPRO 语音模块 UART 对象（UART3, PIN46/PIN47, 115200, 线程接收模式）
ls_uart voice_uart(VOICE_UART_PIN, VOICE_UART_BAUD,
                           LS_UART_DATA8, LS_UART_STOP1, LS_UART_PARITY_NONE,
                           UART_MODE_THREAD, voice_rx_callback);

/*============================================================================
 *                              接收缓冲区
 *============================================================================*/
static uint8_t voice_rx_buf[64];     // 语音模块接收缓冲区
static uint8_t voice_rx_idx = 0;     // 语音接收索引

struct VoiceEventMap {
    VoiceEvent event;
    uint16_t id_zh;
    uint16_t id_en;
    const char* name;
};

static const VoiceEventMap voice_event_map[] = {
    {VOICE_EVENT_PICKUP_ARRIVED,  101, 201, "pickup_arrived"},
    {VOICE_EVENT_DROPOFF_ARRIVED, 102, 202, "dropoff_arrived"},
    {VOICE_EVENT_TASK_START,      103, 203, "task_start"},
    {VOICE_EVENT_TASK_CANCEL,     104, 204, "task_cancel"},
    {VOICE_EVENT_TASK_COMPLETE,   105, 205, "task_complete"},
    {VOICE_EVENT_ROUTE_UPDATED,   106, 206, "route_updated"},
    {VOICE_EVENT_ORDER_DELAYED,   107, 207, "order_delayed"},
    {VOICE_EVENT_INVALID_COMMAND, 108, 208, "invalid_command"},
    {VOICE_EVENT_MODEL_TIMEOUT,   109, 209, "model_timeout"},
    {VOICE_EVENT_NONE,              0,   0, "none"}
};

/*============================================================================
 *                第6部分：给语音模块发送指令
 *============================================================================*/

/**
 * @brief   播放指定 ID 的语音
 * @param   id 语音 ID
 * @note    ASRPRO 协议: 0xFF <id_low> <id_high> 0xFF（需根据实际协议调整）
 */
void Voice_Play_ID(uint16_t id)
{
    if (id == 0) {
        printf("[Voice] warning: empty voice id\n");
        return;
    }

    uint8_t cmd[4] = {0xFF, (uint8_t)(id & 0xFF), (uint8_t)((id >> 8) & 0xFF), 0xFF};
    voice_uart.uart_write(cmd, 4);
    printf("[Voice] TX play id=%u\n", id);
}

void Voice_Play_Event(VoiceEvent event, VoiceLanguage lang)
{
    for (int i = 0; voice_event_map[i].event != VOICE_EVENT_NONE; i++) {
        if (voice_event_map[i].event == event) {
            uint16_t id = (lang == VOICE_LANG_EN) ? voice_event_map[i].id_en : voice_event_map[i].id_zh;
            Voice_Play_ID(id);
            return;
        }
    }

    printf("[Voice] warning: no voice id for event=%d\n", event);
}

VoiceEvent Voice_Event_From_Name(const char* name)
{
    if (!name) return VOICE_EVENT_NONE;
    for (int i = 0; voice_event_map[i].event != VOICE_EVENT_NONE; i++) {
        if (strcmp(name, voice_event_map[i].name) == 0) {
            return voice_event_map[i].event;
        }
    }
    return VOICE_EVENT_NONE;
}

const char* Voice_Get_Event_Name(VoiceEvent event)
{
    for (int i = 0; voice_event_map[i].event != VOICE_EVENT_NONE; i++) {
        if (voice_event_map[i].event == event) {
            return voice_event_map[i].name;
        }
    }
    return "none";
}

/*============================================================================
 *                第7部分：接收语音模块指令
 *============================================================================*/

/**
 * @brief   语音模块接收回调函数
 * @param   data 接收到的单字节数据
 * @note    ASRPRO 输出格式: 命令字符串 + "\r\n"
 */
static void voice_rx_callback(const uint8_t data)
{
    if (data == '\n' || data == '\r')
    {
        if (voice_rx_idx > 0)
        {
            voice_rx_buf[voice_rx_idx] = '\0';
            Voice_Rx_Process((char*)voice_rx_buf);
            voice_rx_idx = 0;
        }
    }
    else
    {
        if (voice_rx_idx < sizeof(voice_rx_buf) - 1)
        {
            voice_rx_buf[voice_rx_idx++] = data;
        }
    }
}

/**
 * @brief   处理语音指令
 * @param   cmd 语音指令字符串
 */
void Voice_Rx_Process(const char* cmd)
{
    if (!cmd || cmd[0] == '\0') return;
    printf("[Voice] RX: %s\n", cmd);

    if (strcmp(cmd, "confirm") == 0 || strcmp(cmd, "queren") == 0 ||
        strcmp(cmd, "ok") == 0) {
        Voice_Model_Handle_Local_Command("confirm");
        return;
    }

    if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "tingzhi") == 0 ||
        strcmp(cmd, "pause") == 0) {
        Voice_Model_Handle_Local_Command("stop");
        return;
    }

    if (!Voice_Model_Send_Command(cmd)) {
        printf("[Voice] model send failed, keep local fallback only\n");
        Voice_Play_Event(VOICE_EVENT_MODEL_TIMEOUT);
    }

    // if (strcmp(cmd, "start") == 0 || strcmp(cmd, "qdong") == 0)
    // {
    //     if (screen_target_ready)
    //     {
    //         run = 1;
    //         printf("[Voice] 启动\n");
    //     }
    //     else
    //     {
    //         printf("[Voice] 未选择目标，不能启动\n");
    //     }
    // }
    // else if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "tingzhi") == 0)
    // {
    //     run = 0;
    //     printf("[Voice] 停止\n");
    // }
}
