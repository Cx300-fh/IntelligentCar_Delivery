/**
 * @file screen.cpp
 * @brief 陶晶驰串口屏模块实现
 * @details 实现与陶晶驰串口屏的通信：发送数据、接收指令
 *          第4部分：发送数据给陶晶驰串口屏
 *          第5部分：接收陶晶驰串口屏指令
 */

#include "screen.hpp"

/*============================================================================
 *                              接收回调前向声明
 *============================================================================*/
static void screen_rx_callback(const uint8_t data);

/*============================================================================
 *                              全局对象定义
 *============================================================================*/
// 陶晶驰串口屏 UART 对象（UART5, PIN64/PIN65, 115200, 线程接收模式）
ls_uart screen_uart(SCREEN_UART_PIN, SCREEN_UART_BAUD,
                            LS_UART_DATA8, LS_UART_STOP1, LS_UART_PARITY_NONE,
                            UART_MODE_THREAD, screen_rx_callback);

/*============================================================================
 *                              屏幕选择目标（AIM指令设定）
 *============================================================================*/
int screen_selected_map = MAP_NONE;      // 屏幕选择的地图ID（0=未选择, 1=THU, 2=SUTD）
int screen_selected_target = 0;          // 屏幕选择的目标点ID（0=未选择）
bool screen_target_ready = false;        // 是否已选择有效目标（用于判断能否启动）

/*============================================================================
 *                              接收缓冲区
 *============================================================================*/
static uint8_t screen_rx_state = 0;       // 换行符检测状态（\r\n）
static uint8_t screen_rx_ff_state = 0;    // 0xFF帧尾检测状态
static uint8_t screen_rx_cmd[64];         // 串口屏指令缓冲区
static uint8_t screen_rx_cmd_len = 0;     // 串口屏指令长度

/*============================================================================
 *                第4部分：发送数据给陶晶驰串口屏
 *============================================================================*/

/**
 * @brief   发送帧尾（0xFF 0xFF 0xFF）
 */
static void screen_send_tail(void)
{
    uint8_t tail[] = {0xFF, 0xFF, 0xFF};
    screen_uart.uart_write(tail, 3, 10);
}

/**
 * @brief   发送变量到屏幕（数值控件）
 * @param   name  组件名称（如 "n0"）
 * @param   value 数值
 * @note    格式: name.val=value + 0xFF 0xFF 0xFF
 */
void Screen_Send_Var(const char* name, int value)
{
    char cmd[64];
    int len = snprintf(cmd, sizeof(cmd), "%s.val=%d", name, value);
    screen_uart.uart_write((uint8_t*)cmd, len, 10);
    screen_send_tail();
}

/**
 * @brief   发送文本到屏幕
 * @param   name  组件名称（如 "t0"）
 * @param   text  文本内容
 * @note    格式: name.txt="text" + 0xFF 0xFF 0xFF
 */
void Screen_Send_Text(const char* name, const char* text)
{
    char cmd[128];
    int len = snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", name, text);
    screen_uart.uart_write((uint8_t*)cmd, len, 10);
    screen_send_tail();
}

/**
 * @brief   切换屏幕页面
 * @param   page_id 页面编号
 */
void Screen_Send_Page(uint8_t page_id)
{
    char cmd[32];
    int len = snprintf(cmd, sizeof(cmd), "page %d", page_id);
    screen_uart.uart_write((uint8_t*)cmd, len, 10);
    screen_send_tail();
}

/**
 * @brief   发送心跳包到陶晶驰屏幕
 * @details 定期发送心跳以保持屏幕在线状态，防止屏幕跳转到初始化界面
 *          发送内容：
 *          - n_online.val=1      : 表示龙芯在线
 *          - page_sys.v_hb_timeout.val=0 : 超时检测清零
 * @note    建议调用频率：每200ms~1秒一次
 * @warning 如果不及时发送，屏幕将认为龙芯未启动，跳转初始化界面并清空地图/目标点
 */
void Screen_Send_Heartbeat(void)
{
    // 发送在线状态
    char cmd1[32];
    int len1 = snprintf(cmd1, sizeof(cmd1), "n_online.val=1");
    screen_uart.uart_write((uint8_t*)cmd1, len1, 10);
    screen_send_tail();

    // 发送超时清零
    char cmd2[48];
    int len2 = snprintf(cmd2, sizeof(cmd2), "page_sys.v_hb_timeout.val=0");
    screen_uart.uart_write((uint8_t*)cmd2, len2, 10);
    screen_send_tail();
}

/**
 * @brief   发送导航状态信息到屏幕
 * @details 根据截图控件布局发送所有需要显示的数据
 *          数据控件(t): t_aim, t_state, t_cur, t_prev, t_next, t_action
 *                      t_route, t_cur_d, t_back_d, t_path
 *          数值控件(n): n_tag, n_angle, n_fps, n_enc_l, n_enc_r
 *                      n_cur_d, n_back_d, n_mile
 */
void Screen_Send_All(void)
{
    char text_buf[128];
    const NavStatus& nav_status = nav_fsm.get_status();
    const NavTask& nav_task = nav_fsm.get_task();

    // ===== 设备WiFi信息显示 =====
    // 根据WiFi连接状态显示不同信息
    switch (wifi_state) {
        case WIFI_STATE_CONNECTED:
            Screen_Send_Text("t_wifi_state", "Connected");
            Screen_Send_Text("t_wifi_name", wifi_ssid_display);
            Screen_Send_Text("t_wifi_ip", device_ip_str);
            break;
        case WIFI_STATE_CONNECTING:
            Screen_Send_Text("t_wifi_state", "Connecting...");
            Screen_Send_Text("t_wifi_name", wifi_ssid_display[0] ? wifi_ssid_display : "--");
            Screen_Send_Text("t_wifi_ip", "Waiting...");
            break;
        case WIFI_STATE_FAILED:
            Screen_Send_Text("t_wifi_state", "Failed");
            Screen_Send_Text("t_wifi_name", "No Network");
            Screen_Send_Text("t_wifi_ip", "No IP");
            break;
        default:
            Screen_Send_Text("t_wifi_state", "No Network");
            Screen_Send_Text("t_wifi_name", "--");
            Screen_Send_Text("t_wifi_ip", "No IP");
            break;
    }

    // ===== 基础传感器数据 =====
    // Tag检测信息
    Screen_Send_Var("n_tag", tag_id);

    // 角度信息
    int angle_int = (int)tag_angle;
    Screen_Send_Var("n_angle", angle_int);

    // FPS
    Screen_Send_Var("n_fps", (int)fps_observe);

    // 目标速度
    if (run==1) {
        Screen_Send_Var("n_speed", (int)current_speed);
    } else {
        Screen_Send_Var("n_speed", 0);
    }

    // 编码器数据
    Screen_Send_Var("n_enc_l", (int)encoder_l);
    Screen_Send_Var("n_enc_r", (int)encoder_r);

    // 里程计数
    Screen_Send_Var("n_mile", (uint32_t)mile);

    // ===== 导航状态信息 =====
    // 显示AIM设定的目标（无论是否已启动）
    if (screen_target_ready) {
        Screen_Send_Text("t_aim", nav_fsm.get_dijkstra()->get_node_name_en(screen_selected_target));
    } else {
        Screen_Send_Text("t_aim", "--");
    }

    if (nav_task.active) {
        // 目标位置已在上面显示，这里不需要重复

        // 当前状态 -> t_state
        Screen_Send_Text("t_state", get_nav_state_name(nav_status.state));

        // 当前动作 -> t_action
        Screen_Send_Text("t_action", get_action_name(nav_status.current_action));

        // Prev/Cur/Next 位置名称 -> t_prev, t_cur, t_next
        if (nav_status.prev_id > 0) {
            Screen_Send_Text("t_prev", nav_fsm.get_dijkstra()->get_node_name_en(nav_status.prev_id));
        } else {
            Screen_Send_Text("t_prev", "--");
        }

        if (nav_status.current_id > 0) {
            Screen_Send_Text("t_cur", nav_fsm.get_dijkstra()->get_node_name_en(nav_status.current_id));
        } else {
            Screen_Send_Text("t_cur", "--");
        }

        if (nav_status.next_id > 0) {
            Screen_Send_Text("t_next", nav_fsm.get_dijkstra()->get_node_name_en(nav_status.next_id));
        } else {
            Screen_Send_Text("t_next", "--");
        }

        // 路线状态 -> t_route
        if (nav_status.route_flag == 1) {
            Screen_Send_Text("t_route", "OK");
        } else if (nav_status.route_flag == 2) {
            Screen_Send_Text("t_route", "ERROR");
        } else {
            Screen_Send_Text("t_route", "--");
        }

        // 规划距离显示
        // n_cur_d: 当前位置到目标的距离(cm)，用于显示剩余距离
        if (nav_status.current_id > 0 && nav_task.target_id > 0) {
            // 当前位置到目标的距离
            Screen_Send_Var("n_cur_d", nav_status.dist > 0 ? nav_status.dist : 0);
        } else {
            // 没有当前位置或目标信息，显示默认值
            Screen_Send_Var("n_cur_d", 9999);
        }

        Screen_Send_Var("n_back_d", 9999);

        // 完整路径规划 -> t_path
        if (nav_status.path_len > 0) {
            text_buf[0] = '\0';
            int offset = 0;
            for (int i = 0; i < nav_status.path_len && offset < sizeof(text_buf) - 10; i++) {
                const char* name = nav_fsm.get_dijkstra()->get_node_name_en(nav_status.path[i]);
                int len = strlen(name);
                if (offset + len + 2 < sizeof(text_buf)) {
                    if (i > 0) {
                        strcpy(text_buf + offset, "->");
                        offset += 2;
                    }
                    strcpy(text_buf + offset, name);
                    offset += len;
                }
            }
            Screen_Send_Text("t_path", text_buf);
        } else {
            Screen_Send_Text("t_path", "--");
        }
    } else {
        // 任务未激活，显示默认值到数据控件
        Screen_Send_Text("t_state", "IDLE");
        Screen_Send_Text("t_prev", "--");
        Screen_Send_Text("t_cur", "--");
        Screen_Send_Text("t_next", "--");
        Screen_Send_Text("t_action", "STOP");
        Screen_Send_Text("t_route", "--");
        Screen_Send_Var("n_cur_d", 9999);
        Screen_Send_Var("n_back_d", 9999);
        Screen_Send_Text("t_path", "Wait...");
    }
}

/*============================================================================
 *                第5部分：接收陶晶驰串口屏指令
 *============================================================================*/

/**
 * @brief   串口屏接收回调函数
 * @param   data 接收到的单字节数据
 * @note    支持两种帧尾格式：
 *          1. 标准格式：0xFF 0xFF 0xFF（陶晶驰默认）
 *          2. 换行格式：0x0D 0x0A（\r\n，用户代码使用 printh 0D 0A）
 */
static void screen_rx_callback(const uint8_t data)
{
    // 检测换行符 \r\n（printh 0D 0A）
    if (data == 0x0D)  // \r
    {
        screen_rx_state = 1;
        return;
    }
    else if (data == 0x0A && screen_rx_state == 1)  // \n 紧跟 \r
    {
        // 收到完整指令（以\r\n结尾），处理
        if (screen_rx_cmd_len > 0)
        {
            Screen_Rx_Process(screen_rx_cmd, screen_rx_cmd_len);
        }
        screen_rx_state = 0;
        screen_rx_cmd_len = 0;
        return;
    }
    else if (data == 0x0A && screen_rx_state == 0)
    {
        // 单独的 \n，也当作结束符（兼容\n结尾）
        if (screen_rx_cmd_len > 0)
        {
            Screen_Rx_Process(screen_rx_cmd, screen_rx_cmd_len);
        }
        screen_rx_state = 0;
        screen_rx_cmd_len = 0;
        return;
    }
    else
    {
        screen_rx_state = 0;
    }

    // 检测标准帧尾 0xFF 0xFF 0xFF
    if (data == 0xFF)
    {
        screen_rx_ff_state++;
        if (screen_rx_ff_state >= 3)
        {
            // 收到完整指令，处理
            if (screen_rx_cmd_len > 0)
            {
                Screen_Rx_Process(screen_rx_cmd, screen_rx_cmd_len);
            }
            screen_rx_ff_state = 0;
            screen_rx_cmd_len = 0;
            return;
        }
    }
    else
    {
        screen_rx_ff_state = 0;
    }

    // 存储数据（排除结束符）
    if (screen_rx_cmd_len < sizeof(screen_rx_cmd) - 1)
    {
        screen_rx_cmd[screen_rx_cmd_len++] = data;
    }
}

/**
 * @brief 解析带两个整数参数的屏幕命令
 * @param cmd    命令字符串，例如 "TARGET,1,3"
 * @param prefix 命令前缀，例如 "TARGET" 或 "START"
 * @param a      输出第一个整数参数
 * @param b      输出第二个整数参数
 * @return true 解析成功；false 命令不匹配或参数不完整
 */
static bool parse_two_int_command(const char* cmd, const char* prefix, int* a, int* b)
{
    int prefix_len = strlen(prefix);
    if (strncmp(cmd, prefix, prefix_len) != 0) return false;
    if (cmd[prefix_len] != ',') return false;
    return sscanf(cmd + prefix_len + 1, "%d,%d", a, b) == 2;
}

/**
 * @brief   处理屏幕接收数据
 * @param   data 指令数据
 * @param   len  数据长度
 */
/**
 * @brief   处理屏幕接收数据
 * @param   data 指令数据
 * @param   len  数据长度
 * @details 解析陶晶驰串口屏发送的指令：
 *          - AIM,map_id,target_id: 设置地图和目标点并启动导航
 *          - START,map_id,target_id: 设置地图和目标点并启动导航
 *          - STOP: 急停/取消任务
 */
void Screen_Rx_Process(const uint8_t* data, ssize_t len)
{
    if (len <= 0) return;

    // 回调缓冲区不是 C 字符串，这里复制一份并手动补 '\0'，
    // 后面才能安全使用 strcmp/strncmp/sscanf。
    char cmd[64];
    int copy_len = (len < (ssize_t)(sizeof(cmd) - 1)) ? len : (ssize_t)(sizeof(cmd) - 1);
    memcpy(cmd, data, copy_len);
    cmd[copy_len] = '\0';

    // 过滤空指令或纯空白字符
    bool is_empty = true;
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] != ' ' && cmd[i] != '\t' && cmd[i] != '\r' && cmd[i] != '\n') {
            is_empty = false;
            break;
        }
    }
    if (is_empty) return;

    // 调试时可选：打印所有接收数据（默认关闭，避免刷屏）
    // printf("[Screen] RX[%zd]: %s\n", len, cmd);

    int map_id = 0;
    int target_id = 0;

    // 1. AIM,map,target
    // 含义：用户在 THU/SUTD 地图页确认了目标，仅保存设定，不启动。
    // AIM,0,0 表示取消目标选择。
    if (parse_two_int_command(cmd, "AIM", &map_id, &target_id))
    {
        printf("[Screen] RX: %s\n", cmd);  // 打印接收到的正确指令
        if (map_id == MAP_NONE || target_id == 0)
        {
            // 取消目标选择
            screen_selected_map = MAP_NONE;
            screen_selected_target = 0;
            screen_target_ready = false;
            printf("[Screen] AIM取消目标选择\n");
        }
        else
        {
            // 保存目标设定，等待START指令后启动
            screen_selected_map = map_id;
            screen_selected_target = target_id;
            screen_target_ready = true;

            // 立即切换地图，确保屏幕能正确显示目标名称
            nav_fsm.get_dijkstra()->set_map(map_id);

            printf("[Screen] AIM设定目标：地图=%d, 目标=%d，等待START启动\n",
                   map_id, target_id);
        }
        return;
    }

    // 2. START
    // 含义：屏幕首页启动按钮发来启动命令，使用AIM指令已设定的地图和目标。
    if (strcmp(cmd, "START") == 0 || strcmp(cmd, "start") == 0)
    {
        printf("[Screen] RX: %s\n", cmd);  // 打印接收到的正确指令
        if (delivery.Enabled())
        {
            // 配送模式：导航只能由服务器goto_stop驱动（服务器权威）
            printf("[Screen] 配送模式：本地START被忽略，等待服务器命令\n");
            Screen_Send_Text("t_state", "WAIT SERVER");
        }
        else if (!screen_target_ready)
        {
            printf("[Screen] START失败：未选择目标，请先AIM设定\n");
        }
        else if (nav_fsm.start_task(screen_selected_map, screen_selected_target))
        {
            printf("[Screen] 启动导航：地图=%d, 目标=%d\n",
                   screen_selected_map, screen_selected_target);
        }
        else
        {
            printf("[Screen] START失败：参数无效（地图=%d, 目标=%d）\n",
                   screen_selected_map, screen_selected_target);
        }
        return;
    }

    // 3. STOP / PAUSE
    // 含义：急停/暂停当前任务
    if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "stop") == 0 ||
        strcmp(cmd, "PAUSE") == 0 || strcmp(cmd, "pause") == 0)
    {
        printf("[Screen] RX: %s\n", cmd);  // 打印接收到的正确指令
        // 串口屏线程安全例外：只允许置位安全禁止（受控快停），清除由主线程负责
        Safety_Inhibit_Set(INHIBIT_REASON_MANUAL);
        Screen_Push_Event(SCREEN_EV_STOP);   // 主线程协调器可感知（记录/上报）
        if (delivery.Enabled()) {
            // 配送模式：只置安全禁止，不取消任务（保留定位与目标，等服务器命令/人工确认）
            printf("[Screen] 配送模式急停（安全禁止已置位）\n");
            return;
        }
        nav_fsm.cancel_task();
        printf("[Screen] 急停/取消任务（安全禁止已置位）\n");
        return;
    }

    // 4. WiFi控制指令
    // 格式: 
    //   WIFI,RECONNECT                    - 重连接当前保存的WiFi
    //   WIFI,DISCONNECT                   - 断开WiFi
    //   WIFI,SAVE_CONNECT,SSID,PASSWORD   - 保存并连接新WiFi
    //   WIFI,GET_SAVED                    - 获取已保存WiFi信息
    if (strncmp(cmd, "WIFI,", 5) == 0)
    {
        printf("[Screen] RX: %s\n", cmd);
        
        char wifi_param[256];
        strncpy(wifi_param, cmd + 5, sizeof(wifi_param) - 1);
        wifi_param[sizeof(wifi_param) - 1] = '\0';
        
        if (strcmp(wifi_param, "RECONNECT") == 0 || strcmp(wifi_param, "reconnect") == 0)
        {
            // 重连接（使用当前配置）
            printf("[Screen] WiFi重连接指令\n");
            bool success = WiFi_Connect();
            // 发送结果到屏幕
            if (success) {
                Screen_Send_Text("t_wifi_result", "Connected!");
                Screen_Send_Text("t_wifi_state", "Connected");
            } else {
                Screen_Send_Text("t_wifi_result", "Connect Failed");
                Screen_Send_Text("t_wifi_state", "Failed");
            }
        }
        else if (strcmp(wifi_param, "DISCONNECT") == 0 || strcmp(wifi_param, "disconnect") == 0)
        {
            // 断开WiFi
            WiFi_Disconnect();
            printf("[Screen] WiFi断开指令执行\n");
            Screen_Send_Text("t_wifi_state", "Disconnected");
        }
        else if (strcmp(wifi_param, "GET_SAVED") == 0 || strcmp(wifi_param, "get_saved") == 0)
        {
            // 发送已保存WiFi信息到屏幕
            const char* ssid = wifi_config.ssid[0] ? wifi_config.ssid : "Not Set";
            const char* pass = wifi_config.password[0] ? wifi_config.password : "Not Set";
            printf("[Screen] 发送已保存WiFi信息: SSID=%s, PASS=%s\n", ssid, pass);
            Screen_Send_Text("t_saved_ssid", ssid);
            Screen_Send_Text("t_saved_pass", pass);
            // 延迟一点时间确保屏幕页面已加载
            usleep(100000); // 100ms
            Screen_Send_Text("t_saved_ssid", ssid);
            Screen_Send_Text("t_saved_pass", pass);
        }
        else if (strncmp(wifi_param, "SAVE_CONNECT,", 13) == 0)
        {
            // 保存并连接: WIFI,SAVE_CONNECT,SSID,PASSWORD
            char* ssid = strtok(wifi_param + 13, ",");
            char* password = strtok(NULL, ",");
            if (ssid && password) {
                printf("[Screen] 保存并连接新WiFi: SSID=%s\n", ssid);
                
                // 1. 设置新WiFi
                WiFi_Set_Credentials(ssid, password);
                
                // 2. 立即保存到配置文件
                if (WiFi_Save_Config()) {
                    printf("[Screen] 新配置已保存\n");
                    // 3. 更新左侧显示的已保存WiFi信息
                    Screen_Send_Text("t_saved_ssid", ssid);
                    Screen_Send_Text("t_saved_password", password);
                }
                
                // 4. 尝试连接
                bool success = WiFi_Connect();
                
                // 5. 发送结果到屏幕
                if (success) {
                    Screen_Send_Text("t_wifi_result", "Connect Success!");
                    Screen_Send_Text("t_wifi_state", "Connected");
                    // 清空新WiFi输入框（屏幕端处理）
                    Screen_Send_Text("t_new_ssid", "");
                    Screen_Send_Text("t_new_password", "");
                    // 显示返回主界面按钮，隐藏返回配置界面按钮
                    // 屏幕端根据t_wifi_result内容自动切换
                } else {
                    Screen_Send_Text("t_wifi_result", "Connect Failed!");
                    Screen_Send_Text("t_wifi_state", "Failed");
                    // 保留新WiFi输入框数据（屏幕端保留）
                    // 显示返回配置界面按钮，隐藏返回主界面按钮
                }
            } else {
                printf("[Screen] WiFi参数错误，格式: WIFI,SAVE_CONNECT,SSID,PASSWORD\n");
                Screen_Send_Text("t_wifi_result", "Param Error");
            }
        }
        else
        {
            printf("[Screen] 未知WiFi指令: %s\n", wifi_param);
        }
        return;
    }

    // 4.5 配送模式确认按钮（DConfirm页d_s_4发出，阶段6）
    // 串口屏线程只入队；订单映射与user_action生成由主线程配送协调器完成
    if (strcmp(cmd, "LOAD_CONFIRMED") == 0)
    {
        printf("[Screen] RX: %s（装载确认，入队）\n", cmd);
        Screen_Push_Event(SCREEN_EV_LOAD_CONFIRMED);
        return;
    }
    if (strcmp(cmd, "UNLOAD_CONFIRMED") == 0)
    {
        printf("[Screen] RX: %s（取件确认，入队）\n", cmd);
        Screen_Push_Event(SCREEN_EV_UNLOAD_CONFIRMED);
        return;
    }
    // 模拟类消息（新固件保留按钮但车端忽略）：到站判定以AprilTag停稳为准
    if (strcmp(cmd, "ARRIVED_DROPOFF") == 0 || strcmp(cmd, "NEXT_ORDER") == 0 ||
        strcmp(cmd, "ALL_ORDERS_COMPLETE") == 0)
    {
        printf("[Screen] RX: %s（模拟消息，忽略）\n", cmd);
        return;
    }

    // 5. 未知指令 - 静默忽略，不打印（避免干扰数据刷屏）
}

/*============================================================================
 *                    配送模式：屏幕事件队列（阶段6）
 *============================================================================*/
#define SCREEN_EV_QUEUE_SIZE 8

static ScreenEvent g_screen_ev_queue[SCREEN_EV_QUEUE_SIZE];
static volatile int g_screen_ev_head = 0;   // 串口屏线程写
static volatile int g_screen_ev_tail = 0;   // 主线程读

void Screen_Push_Event(ScreenEvent ev)
{
    int next = (g_screen_ev_head + 1) % SCREEN_EV_QUEUE_SIZE;
    if (next == g_screen_ev_tail) return;   // 队满丢弃（确认按钮防抖兜底）
    g_screen_ev_queue[g_screen_ev_head] = ev;
    g_screen_ev_head = next;
}

bool Screen_Poll_Event(ScreenEvent* out)
{
    if (g_screen_ev_tail == g_screen_ev_head) return false;
    *out = g_screen_ev_queue[g_screen_ev_tail];
    g_screen_ev_tail = (g_screen_ev_tail + 1) % SCREEN_EV_QUEUE_SIZE;
    return true;
}

/*============================================================================
 *                    配送模式：快照渲染（阶段6）
 *============================================================================*/
// 文本安全：过滤引号/反斜杠/换行/控制字符（任务书十四.3，防服务器文本拼坏HMI指令）
static void screen_safe_text(char* dst, size_t dst_size, const std::string& src)
{
    size_t o = 0;
    for (size_t i = 0; i < src.size() && o + 1 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20 || c == '"' || c == '\\') continue;   // 控制字符/引号/反斜杠丢弃
        dst[o++] = (char)c;                                 // UTF-8多字节原样保留
    }
    dst[o] = '\0';
}

// 订单槽位映射：当前订单（current_order_id优先，否则首个status2/3/4订单）在缓存中的槽(1~5)
// 无活跃订单返回0
static int delivery_active_slot(const StateSync& snap)
{
    for (size_t i = 0; i < snap.orders.size() && i < 5; i++) {
        if (snap.has_current_order && snap.orders[i].order_id == snap.current_order_id)
            return (int)(i + 1);
    }
    for (size_t i = 0; i < snap.orders.size() && i < 5; i++) {
        int st = snap.orders[i].status;
        if (st == ORDER_WAIT_PICKUP_CONFIRM || st == ORDER_DELIVERING ||
            st == ORDER_WAIT_DROPOFF_CONFIRM)
            return (int)(i + 1);
    }
    return 0;
}

static void render_delivery_texts(const StateSync& snap, int slot)
{
    char s1[48], s4[32];
    const OrderInfo* cur = (slot > 0 && (size_t)(slot - 1) < snap.orders.size())
                           ? &snap.orders[slot - 1] : nullptr;

    switch (snap.screen_phase) {
        case SCREEN_PHASE_NONE: {
            Screen_Send_Text("d_s_1", "暂无订单");
            Screen_Send_Text("d_s_2", "等待服务器分配订单");
            Screen_Send_Text("d_s_3", " ");
            Screen_Send_Text("d_s_4", "待命");
            break;
        }
        case SCREEN_PHASE_TO_PICKUP:
            screen_safe_text(s1, sizeof(s1), cur ? cur->nickname + " - 前往取件点"
                                                 : std::string("前往取件点"));
            Screen_Send_Text("d_s_1", s1);
            Screen_Send_Text("d_s_2", "车辆正在前往取件点");
            Screen_Send_Text("d_s_3", "到点后等待装载确认");
            Screen_Send_Text("d_s_4", "配送中");
            break;
        case SCREEN_PHASE_WAIT_PICKUP:
            screen_safe_text(s1, sizeof(s1), cur ? "已到取件点 - " + cur->nickname : std::string("已到取件点"));
            Screen_Send_Text("d_s_1", s1);
            screen_safe_text(s1, sizeof(s1), cur ? "物品：" + cur->item_summary : std::string(" "));
            Screen_Send_Text("d_s_2", s1);
            Screen_Send_Text("d_s_3", "装好后点击下方按钮");
            Screen_Send_Text("d_s_4", cur && !cur->button_label.empty() ? cur->button_label.c_str() : "物品已装好");
            break;
        case SCREEN_PHASE_DELIVERING:
            screen_safe_text(s1, sizeof(s1), cur ? cur->nickname + " - 配送中" : std::string("配送中"));
            Screen_Send_Text("d_s_1", s1);
            screen_safe_text(s1, sizeof(s1), cur ? "送往：" + cur->dropoff_name : std::string(" "));
            Screen_Send_Text("d_s_2", s1);
            Screen_Send_Text("d_s_3", "到点后等待取件确认");
            Screen_Send_Text("d_s_4", "配送中");
            break;
        case SCREEN_PHASE_WAIT_DROPOFF:
            screen_safe_text(s1, sizeof(s1), cur ? "已到目的地 - " + cur->nickname : std::string("已到目的地"));
            Screen_Send_Text("d_s_1", s1);
            Screen_Send_Text("d_s_2", "物品已送达，等待取件");
            Screen_Send_Text("d_s_3", "取走后点击下方按钮");
            Screen_Send_Text("d_s_4", cur && !cur->button_label.empty() ? cur->button_label.c_str() : "物品已取走");
            break;
        case SCREEN_PHASE_ALL_DONE:
            Screen_Send_Text("d_s_1", "全部订单完成");
            Screen_Send_Text("d_s_2", "等待服务器新任务");
            Screen_Send_Text("d_s_3", " ");
            Screen_Send_Text("d_s_4", "已完成");
            break;
        default:
            break;
    }
    (void)s4;
}

void Screen_Render_Delivery(void)
{
    // 快照版本变化立即刷新；未变化每1秒兜底刷新（覆盖屏幕按钮点击扰动的文案）
    static uint64_t last_ver = (uint64_t)-1;
    static uint32_t last_ms = 0;
    const StateSync& snap = delivery.Snapshot();
    uint32_t now = lq_get_tick_ms();
    if (snap.snapshot_version == last_ver && now - last_ms < 1000) return;
    last_ver = snap.snapshot_version;
    last_ms = now;

    int slot = delivery_active_slot(snap);

    // 订单号：b1~b5显示display_no（无订单的槽清空）
    char txt[16];
    for (int i = 0; i < 5; i++) {
        char name[8];
        snprintf(name, sizeof(name), "b%d", i + 1);
        if ((size_t)i < snap.orders.size() && !snap.orders[i].display_no.empty()) {
            screen_safe_text(txt, sizeof(txt), snap.orders[i].display_no);
        } else {
            snprintf(txt, sizeof(txt), "--");
        }
        Screen_Send_Text(name, txt);
    }

    // 状态机变量：屏幕据此切换按钮文案（b1~b5内部逻辑）与确认按钮使能
    Screen_Send_Var("active_phase", snap.screen_phase);
    Screen_Send_Var("active_slot", slot);
    Screen_Send_Var("viewing_slot", slot > 0 ? slot : 1);

    // 文案与确认按钮（车端权威写入，覆盖屏幕本地文案）
    render_delivery_texts(snap, slot);

    // 确认按钮使能：仅在等待确认(phase 2/4)且该订单未在待确认队列时允许点击
    bool enable_btn = (snap.screen_phase == SCREEN_PHASE_WAIT_PICKUP ||
                       snap.screen_phase == SCREEN_PHASE_WAIT_DROPOFF);
    char cmd[32];
    int len = snprintf(cmd, sizeof(cmd), "tsw d_s_4,%d", enable_btn ? 1 : 0);
    screen_uart.uart_write((uint8_t*)cmd, len, 10);
    screen_send_tail();
}

void Screen_Enter_Delivery_Page(void)
{
    // DConfirm页为配送模式常驻页（按页面名跳转，页面ID无关）
    char cmd[24];
    int len = snprintf(cmd, sizeof(cmd), "page DConfirm");
    screen_uart.uart_write((uint8_t*)cmd, len, 10);
    screen_send_tail();
    printf("[Screen] 配送模式：切换到DConfirm页\n");
}
