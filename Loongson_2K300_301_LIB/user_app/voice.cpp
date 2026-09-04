/**
 * @file voice.cpp
 * @brief ASRPRO 语音模块实现
 * @details 实现与语音模块的通信：发送指令、接收指令
 *          第6部分：给语音模块发送指令
 *          第7部分：接收语音模块指令
 */

#include "voice.hpp"

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

/**
 * @brief   根据地图和当前节点 ID，获取语音模块需要接收的地点字符串
 * @param   map_id 当前地图 ID：1=THU，2=SUTD
 * @param   node_id 当前节点 ID / AprilTag ID
 * @return  对应的字符串；如果无效则返回 nullptr
 */
static const char* Voice_Get_Node_Cmd(int map_id, int node_id)
{
    // THU 地图：ID 1~14
    // 1 紫荆操场 ZJCC
    // 2 理科楼 LKL
    // 3 图书馆 TSG
    // 4 苏世民书院 SSM
    // 5 东大操场 DDCC
    // 6 校医院 XYY
    // 7 学生宿舍 XSSS
    // 8 东门 DM
    // 9 大礼堂 DLT
    // 10 新清华学堂 XQH
    // 11 中央主楼 ZYZL
    // 12 A点 A
    // 13 照澜院 ZLY
    // 14 科技大楼 KJDL
    static const char* thu_cmd[] = {
        nullptr,
        "ZJCC",
        "LKL",
        "TSG",
        "SSM",
        "DDCC",
        "XYY",
        "XSSS",
        "DM",
        "DLT",
        "XQH",
        "ZYZL",
        "A",
        "ZLY",
        "KJDL"
    };

    // SUTD 地图：ID 1~12
    // 1 A
    // 2 B
    // 3 C
    // 4 D
    // 5 E
    // 6 F
    // 7 LIB
    // 8 AUD
    // 9 SSH
    // 10 CC
    // 11 POOL
    // 12 SRC
    static const char* sutd_cmd[] = {
        nullptr,
        "A",
        "B",
        "C",
        "D",
        "E",
        "F",
        "LIB",
        "AUD",
        "SSH",
        "CC",
        "POOL",
        "SRC"
    };

    if (map_id == 1)   // MAP_THU
    {
        if (node_id >= 1 && node_id <= 14)
        {
            return thu_cmd[node_id];
        }
    }
    else if (map_id == 2)  // MAP_SUTD
    {
        if (node_id >= 1 && node_id <= 12)
        {
            return sutd_cmd[node_id];
        }
    }

    return nullptr;
}

/**
 * @brief   向 ASRPRO 语音模块发送字符串
 * @param   str 要发送的字符串
 * @note    与语音模块图形化程序中的“接收到字符串 XXX”对应
 */
void Voice_Send_String(const char* str)
{
    if (str == nullptr)
    {
        return;
    }

    voice_uart.uart_write((uint8_t*)str, strlen(str));

    // 如果语音模块那边需要换行作为结束符，可以打开下面这一行
    // voice_uart.uart_write((uint8_t*)"\r\n", 2);
}

/**
 * @brief   根据当前地图和当前节点 ID 播放到站语音
 * @param   map_id 当前地图 ID：1=THU，2=SUTD
 * @param   current_id 当前节点 ID / AprilTag ID
 */
void Voice_Play_Current_Node(int map_id, int current_id)
{
    const char* cmd = Voice_Get_Node_Cmd(map_id, current_id);

    if (cmd == nullptr)
    {
        printf("[Voice] invalid map_id=%d, current_id=%d\n", map_id, current_id);
        return;
    }

    Voice_Send_String(cmd);
    printf("[Voice] TX: %s, map_id=%d, current_id=%d\n", cmd, map_id, current_id);
}

/**
 * @brief   到达终点：发送END（裸字符串，不带换行）
 * @note    ASRPRO收到后播“请取走物品”
 */
void Voice_Play_Arrived(void)
{
    Voice_Send_String("END");
    printf("[Voice] TX: END (arrived)\n");
}

/**
 * @brief   到达取件点：发送PUT（裸字符串，不带换行）
 * @note    ASRPRO收到后播“请放入物品”；天问Block侧需加“接收字符串等于PUT”分支
 */
void Voice_Play_Pickup(void)
{
    Voice_Send_String("PUT");
    printf("[Voice] TX: PUT (pickup)\n");
}

/*============================================================================
 *                          配送停靠类型（供导航查询）
 *============================================================================*/
static int s_voice_stop_type = VOICE_STOP_UNKNOWN;
static bool s_voice_suppress_arrival = false;

/**
 * @brief   配送协调器在goto_stop接受时通知本次停靠类型
 */
void Voice_Notify_Stop_Type(int type)
{
    if (type != VOICE_STOP_PICKUP && type != VOICE_STOP_DROPOFF) {
        type = VOICE_STOP_UNKNOWN;
    }
    s_voice_stop_type = type;
    s_voice_suppress_arrival = false;   // 新stop正常播报
}

/**
 * @brief   导航到站时查询本次停靠类型，决定播PUT（放入）还是END（取走）
 */
int Voice_Get_Stop_Type(void)
{
    return s_voice_stop_type;
}

/**
 * @brief   抑制下一次到站播报：同一stop的goto_stop补发（后端resend）场景，
 *          车端已到站播报过，直接到站流程不应再播一遍
 */
void Voice_Suppress_Next_Arrival(void)
{
    s_voice_suppress_arrival = true;
}

/**
 * @brief   查询并清除抑制标志（一次性消费）
 */
bool Voice_Consume_Suppress_Flag(void)
{
    bool s = s_voice_suppress_arrival;
    s_voice_suppress_arrival = false;
    return s;
}

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
    // uint8_t cmd[4] = {0xFF, (uint8_t)(id & 0xFF), (uint8_t)((id >> 8) & 0xFF), 0xFF};
    // voice_uart.uart_write(cmd, 4);
}

/*============================================================================
 *                第7部分：接收语音模块指令
 *============================================================================*/

/**
 * @brief   语音模块接收回调函数
 * @param   data 接收到的单字节数据
 * @note    ASRPRO 输出格式: 命令字符串 + "\r\n"；语音确认为裸单字节
 */
static void voice_rx_callback(const uint8_t data)
{
    // ASRPRO语音确认（单字节，无换行）：0x01=物品已装好 0x02=物品已取走。
    // 与屏幕确认按钮完全等价：这里只入队，主线程配送协调器统一校验订单
    // 状态后生成user_action；订单状态不符则忽略，后端另有幂等，双通道
    // 不冲突不覆盖（先到者生效，后到者被状态机拒绝）。
    if (data == 0x01)
    {
        printf("[Voice] RX: 0x01（语音装载确认，入队）\n");
        Screen_Push_Event(SCREEN_EV_LOAD_CONFIRMED);
        return;
    }
    if (data == 0x02)
    {
        printf("[Voice] RX: 0x02（语音取件确认，入队）\n");
        Screen_Push_Event(SCREEN_EV_UNLOAD_CONFIRMED);
        return;
    }

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
    printf("[Voice] RX: %s\n", cmd);

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
