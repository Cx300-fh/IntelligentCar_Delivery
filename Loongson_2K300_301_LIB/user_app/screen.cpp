/**
 * @file screen.cpp
 * @brief 陶晶驰串口屏模块实现
 * @details 实现与陶晶驰串口屏的通信：发送数据、接收指令
 *          第4部分：发送数据给陶晶驰串口屏
 *          第5部分：接收陶晶驰串口屏指令
 */

#include "screen.hpp"
#include "screen_tts.hpp"

#include <atomic>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/*============================================================================
 *                              接收回调前向声明
 *============================================================================*/
static void screen_rx_callback(const uint8_t data);

/*============================================================================
 *                              全局对象定义
 *============================================================================*/
// 陶晶驰串口屏 UART 对象（UART5, PIN64/PIN65, 230400, 线程接收模式）
ls_uart screen_uart(SCREEN_UART_PIN, SCREEN_UART_BAUD,
                            LS_UART_DATA8, LS_UART_STOP1, LS_UART_PARITY_NONE,
                            UART_MODE_THREAD, screen_rx_callback);

/*============================================================================
 *                              屏幕选择目标（AIM指令设定）
 *============================================================================*/
int screen_selected_map = MAP_NONE;      // 屏幕选择的地图ID（0=未选择, 1=THU, 2=SUTD）
int screen_selected_target = 0;          // 屏幕选择的目标点ID（0=未选择）
bool screen_target_ready = false;        // 是否已选择有效目标（用于判断能否启动）

// 配送页显示状态：HMI的START按钮事件自带page DConfirm，退出按钮发"END"并page Main。
// 车端据这两条指令跟踪当前页面：DConfirm页控件（b1~b5/d_s_*）与其他页同名控件冲突
// （地图页b1=back/b2=clear），裸发指令会命中当前页同名控件，必须先确认页面再发。
bool screen_delivery_page_active = false;

/*============================================================================
 *                              接收缓冲区
 *============================================================================*/
static uint8_t screen_rx_state = 0;       // 换行符检测状态（\r\n）
static uint8_t screen_rx_ff_state = 0;    // 0xFF帧尾检测状态
static uint8_t screen_rx_cmd[512];        // 串口屏指令缓冲区（兼容TTS UTF-8文本）
static size_t screen_rx_cmd_len = 0;      // 串口屏指令长度

// twfile透明传输与普通HMI指令必须严格隔离。普通发送只短暂持锁；上传期间直接
// 丢弃周期刷新和心跳，避免阻塞主控制循环，也避免任何字节插入二进制包。
static std::mutex g_screen_tx_mutex;
static std::atomic<bool> g_screen_transfer_active(false);
static std::mutex g_screen_ack_mutex;
static std::condition_variable g_screen_ack_cv;
static std::deque<uint8_t> g_screen_ack_bytes;

/*============================================================================
 *                第4部分：发送数据给陶晶驰串口屏
 *============================================================================*/

/**
 * @brief   发送帧尾（0xFF 0xFF 0xFF）
 */
// 串口续写：uart_write 超时会短写，必须从断点继续发，
// 不能从包头重发（否则屏幕会收到重复前缀）。
static bool screen_uart_write_all(const uint8_t* data, size_t len)
{
    size_t sent = 0;
    for (int retry = 0; retry < 8 && sent < len; retry++) {
        ssize_t w = screen_uart.uart_write(data + sent, (ssize_t)(len - sent), 100);
        if (w <= 0) break;
        sent += (size_t)w;
    }
    return sent == len;
}

char g_screen_last_cmd[128] = {0};   // 【诊断】最近一条发给屏幕的指令

static bool screen_write_command(const char* cmd)
{
    if (!cmd) return false;
    snprintf(g_screen_last_cmd, sizeof(g_screen_last_cmd), "%s", cmd);
    std::lock_guard<std::mutex> lock(g_screen_tx_mutex);
    if (g_screen_transfer_active.load()) {
        // 【临时诊断】传输占用时所有写入被静默丢弃，而调用方全都忽略返回值
        static int busy_dbg = 0;
        if ((++busy_dbg % 50) == 1)
            printf("[Screen] TX被跳过：文件传输占用中 cmd=%s\n", cmd);
        return false;
    }
    const uint8_t tail[] = {0xFF, 0xFF, 0xFF};
    size_t len = strlen(cmd);
    // ls_uart::uart_write() 超时时会只发出一部分并返回已发字节数。
    // 原实现一次写不完就直接放弃，屏幕收到的是半条指令，
    // 变量名被截断于是回 0x1A（变量名无效）——2026-09-03 实测
    // “DConfirm.d_s_1.txt=...” 期望 33 字节只发出 11 个，帧尾根本没发，
    // 单次运行累计收到 4406 个 0x1A；active_phase/active_slot/viewing_slot
    // 三个变量发不下去，屏幕只能拿旧值算，装货按钮就卡在“等待中”。
    // 同文件的 screen_write_raw_locked() 早就有续写逻辑，这里补上。
    if (!screen_uart_write_all((const uint8_t*)cmd, len)) {
        static int fail_dbg = 0;
        if ((++fail_dbg % 50) == 1)
            printf("[Screen] TX正文续写仍失败 cmd=%s 期望%zu\n", cmd, len);
        return false;
    }
    if (!screen_uart_write_all(tail, 3)) {
        static int tail_dbg = 0;
        if ((++tail_dbg % 50) == 1)
            printf("[Screen] TX帧尾续写仍失败 cmd=%s\n", cmd);
        return false;
    }
    return true;
}

// 仅供已取得传输所有权的twfile实现调用。
static bool screen_write_raw_locked(const uint8_t* data, size_t len)
{
    std::lock_guard<std::mutex> lock(g_screen_tx_mutex);
    if (!data || len == 0) return false;

    // ls_uart::uart_write()允许在超时时返回已经发送的字节数。二进制透传
    // 不能从包头重发，必须从短写位置继续；小块发送也可避免长时间占用驱动。
    const size_t chunk_limit = 128;
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    size_t offset = 0;
    unsigned short_writes = 0;
    while (offset < len) {
        if (std::chrono::steady_clock::now() >= deadline) {
            printf("[ScreenTTS] UART5续写超时：期望%zu字节，已发送%zu字节\n",
                   len, offset);
            return false;
        }
        const size_t request = std::min(chunk_limit, len - offset);
        const ssize_t written = screen_uart.uart_write(
            data + offset, (ssize_t)request, 1000);
        if (written <= 0) {
            printf("[ScreenTTS] UART5写入失败：总计%zu字节，已发送%zu字节，返回%zd\n",
                   len, offset, written);
            return false;
        }
        offset += (size_t)written;
        if ((size_t)written < request) ++short_writes;
    }
    if (short_writes)
        printf("[ScreenTTS] UART5短写已自动续传：%u次，总计%zu字节\n",
               short_writes, len);
    return true;
}

static bool screen_write_transfer_command(const std::string& cmd)
{
    const uint8_t tail[] = {0xFF, 0xFF, 0xFF};
    return screen_write_raw_locked((const uint8_t*)cmd.data(), cmd.size()) &&
           screen_write_raw_locked(tail, sizeof(tail));
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
    snprintf(cmd, sizeof(cmd), "%s.val=%d", name, value);
    screen_write_command(cmd);
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
    snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", name, text);
    screen_write_command(cmd);
}

/**
 * @brief   切换屏幕页面
 * @param   page_id 页面编号
 */
void Screen_Send_Page(uint8_t page_id)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "page %d", page_id);
    screen_write_command(cmd);
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
    snprintf(cmd1, sizeof(cmd1), "n_online.val=1");
    screen_write_command(cmd1);

    // 发送超时清零
    char cmd2[48];
    snprintf(cmd2, sizeof(cmd2), "page_sys.v_hb_timeout.val=0");
    screen_write_command(cmd2);
}

bool Screen_Transfer_Active(void)
{
    return g_screen_transfer_active.load();
}

static void screen_ack_clear(void)
{
    std::lock_guard<std::mutex> lock(g_screen_ack_mutex);
    g_screen_ack_bytes.clear();
}

static bool screen_wait_sequence(const uint8_t* expected, size_t expected_len,
                                 uint32_t timeout_ms)
{
    size_t matched = 0;
    std::vector<uint8_t> seen;
    std::unique_lock<std::mutex> lock(g_screen_ack_mutex);
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (g_screen_ack_bytes.empty()) {
            if (g_screen_ack_cv.wait_until(lock, deadline) == std::cv_status::timeout)
                break;
            continue;
        }
        uint8_t value = g_screen_ack_bytes.front();
        g_screen_ack_bytes.pop_front();
        if (seen.size() < 32) seen.push_back(value);
        if (value == expected[matched]) {
            if (++matched == expected_len) return true;
        } else {
            matched = (value == expected[0]) ? 1 : 0;
        }
    }
    printf("[ScreenTTS] 等待屏幕应答超时，期望:");
    for (size_t i = 0; i < expected_len; ++i) printf(" %02X", expected[i]);
    printf("；已收到:");
    if (seen.empty()) printf(" <无>");
    for (size_t i = 0; i < seen.size(); ++i) printf(" %02X", seen[i]);
    printf("\n");
    return false;
}

static uint16_t screen_crc16_modbus(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
    return crc;
}

static bool screen_valid_file_name(const char* value)
{
    if (!value || !*value) return false;
    for (const unsigned char* p = (const unsigned char*)value; *p; ++p) {
        if (!(isalnum(*p) || *p == '/' || *p == '_' || *p == '-' || *p == '.'))
            return false;
    }
    return true;
}

static void screen_abort_transfer(void)
{
    // 官方协议：停顿20ms后发送ID=65535、无校验、0数据长度的包可退出透传。
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const uint8_t abort_packet[] = {
        0x3A, 0xA1, 0xBB, 0x44, 0x7F, 0xFF, 0xFE, 0x00,
        0xFF, 0xFF, 0x00, 0x00
    };
    screen_write_raw_locked(abort_packet, sizeof(abort_packet));
    const uint8_t done[] = {0xFD, 0xFF, 0xFF, 0xFF};
    screen_wait_sequence(done, sizeof(done), 1000);
}

bool Screen_Upload_Wav_And_Play(const uint8_t* wav_data, size_t wav_size,
                                const char* remote_path, const char* component)
{
    if (!wav_data || wav_size < 44 || wav_size > 1024 * 1024 ||
        !screen_valid_file_name(remote_path) || !screen_valid_file_name(component)) {
        printf("[ScreenTTS] 非法WAV数据、路径或控件名\n");
        return false;
    }

    // 取得独占传输权；若另一个TTS正在上传则立即返回，绝不阻塞控制主循环。
    {
        std::lock_guard<std::mutex> lock(g_screen_tx_mutex);
        if (g_screen_transfer_active.load()) {
            printf("[ScreenTTS] 上传拒绝：已有文件传输正在进行\n");
            return false;
        }
        g_screen_transfer_active.store(true);
    }
    screen_rx_state = 0;
    screen_rx_ff_state = 0;
    screen_rx_cmd_len = 0;
    screen_ack_clear();

    bool ok = false;
    const char* stage = "准备";
    do {
        printf("[ScreenTTS] 准备上传 %zu 字节到 %s（控件 %s）\n",
               wav_size, remote_path, component);
        // HMI定时器据此暂停心跳超时判断。该变量必须在屏幕工程中设为全局。
        stage = "设置上传标志";
        if (!screen_write_transfer_command("page_sys.v_tts_upload.val=1")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stage = "停用音频控件";
        if (!screen_write_transfer_command(std::string(component) + ".en=0")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        screen_ack_clear();

        char twfile[128];
        snprintf(twfile, sizeof(twfile), "twfile \"%s\",%zu", remote_path, wav_size);
        stage = "发送twfile指令";
        if (!screen_write_transfer_command(twfile)) break;
        const uint8_t ready[] = {0xFE, 0xFF, 0xFF, 0xFF};
        stage = "等待twfile就绪";
        if (!screen_wait_sequence(ready, sizeof(ready), 3000)) {
            printf("[ScreenTTS] twfile未就绪，请检查RAM文件区和HMI配置\n");
            break;
        }

        stage = "传输WAV数据包";
        // X3允许dataSize最大4096，但板端轮询UART在大包时会多次短写，
        // 导致屏幕等待整包超时。510字节音频+2字节CRC可稳定落在512字节内。
        const size_t max_payload = 510;
        size_t offset = 0;
        uint16_t packet_id = 0;
        while (offset < wav_size) {
            const size_t payload_len = std::min(max_payload, wav_size - offset);
            const bool last_packet = (offset + payload_len == wav_size);
            const uint16_t data_size = (uint16_t)(payload_len + 2);
            std::vector<uint8_t> packet(12 + payload_len + 2);
            const uint8_t prefix[] = {0x3A, 0xA1, 0xBB, 0x44, 0x7F, 0xFF, 0xFE};
            memcpy(&packet[0], prefix, sizeof(prefix));
            packet[7] = 0x01;  // CRC16 MODBUS
            packet[8] = (uint8_t)(packet_id & 0xFF);
            packet[9] = (uint8_t)(packet_id >> 8);
            packet[10] = (uint8_t)(data_size & 0xFF);
            packet[11] = (uint8_t)(data_size >> 8);
            memcpy(&packet[12], wav_data + offset, payload_len);
            uint16_t crc = screen_crc16_modbus(wav_data + offset, payload_len);
            packet[12 + payload_len] = (uint8_t)(crc & 0xFF);
            packet[13 + payload_len] = (uint8_t)(crc >> 8);

            bool packet_ok = false;
            // 屏幕对中间包回复05；最后一包直接回复FD FF FF FF，不再先发05。
            // 最后一包若盲目重发会得到04（包序号/传输状态错误），因此只发送一次。
            const int retry_limit = last_packet ? 1 : 3;
            for (int retry = 0; retry < retry_limit && !packet_ok; ++retry) {
                screen_ack_clear();
                if (!screen_write_raw_locked(&packet[0], packet.size())) continue;
                if (last_packet) {
                    const uint8_t done[] = {0xFD, 0xFF, 0xFF, 0xFF};
                    packet_ok = screen_wait_sequence(done, sizeof(done), 2000);
                } else {
                    const uint8_t ack = 0x05;
                    packet_ok = screen_wait_sequence(&ack, 1, 700);
                }
            }
            if (!packet_ok) {
                printf("[ScreenTTS] 数据包%u传输失败\n", (unsigned)packet_id);
                screen_abort_transfer();
                offset = 0;
                break;
            }
            offset += payload_len;
            ++packet_id;
        }
        if (offset != wav_size) break;

        // page_sys中的全局外部音频组件：from=1、loop=0、path=ram/tts.wav。
        stage = "设置音频文件路径";
        if (!screen_write_transfer_command(std::string(component) + ".path=\"" + remote_path + "\"")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stage = "设置非循环播放";
        if (!screen_write_transfer_command(std::string(component) + ".loop=0")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stage = "设置屏幕音量";
        if (!screen_write_transfer_command("volume=80")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stage = "启用屏幕音频控件";
        if (!screen_write_transfer_command(std::string(component) + ".en=1")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // 用户HMI中的开始/暂停按钮分别使用play 0,1,0和play 0,0,0。
        // wav_tts绑定到默认音频通道0，en=1后显式启动/恢复该通道。
        stage = "启动音频通道0";
        if (!screen_write_transfer_command("play 0,1,0")) break;
        printf("[ScreenTTS] 已发送播放命令：volume=80, play 0,1,0\n");
        ok = true;
    } while (false);

    if (!ok) printf("[ScreenTTS] 上传阶段失败：%s\n", stage);

    // 无论成功失败都恢复心跳状态；传输失败时先尝试协议退出包。
    screen_write_transfer_command("page_sys.v_hb_timeout.val=0");
    screen_write_transfer_command("page_sys.v_tts_upload.val=0");
    screen_rx_state = 0;
    screen_rx_ff_state = 0;
    screen_rx_cmd_len = 0;
    g_screen_transfer_active.store(false);
    screen_ack_clear();
    return ok;
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
    if (g_screen_transfer_active.load()) {
        {
            std::lock_guard<std::mutex> lock(g_screen_ack_mutex);
            g_screen_ack_bytes.push_back(data);
            if (g_screen_ack_bytes.size() > 64) g_screen_ack_bytes.pop_front();
        }
        g_screen_ack_cv.notify_one();
        return;
    }

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
        return;  // 帧尾候选字节不写入ASCII命令缓冲区
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
    char cmd[512];
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

    // 【临时诊断】打开原始接收打印：区分“屏幕没发”与“发了但指令名对不上”
    // 【诊断】屏幕回 0x1A = 控件名无效。按控件名去重，一个名字只报一次。
    if (len == 1 && (unsigned char)cmd[0] == 0x1A) {
        static char seen[24][64];
        static int  seen_n = 0;
        char key[64];
        snprintf(key, sizeof(key), "%s", g_screen_last_cmd);
        for (int i = 0; key[i]; i++) { if (key[i] == '=') { key[i] = 0; break; } }
        bool dup = false;
        for (int i = 0; i < seen_n; i++) if (strcmp(seen[i], key) == 0) { dup = true; break; }
        if (!dup && seen_n < 24) {
            snprintf(seen[seen_n], sizeof(seen[0]), "%s", key);
            seen_n++;
            printf("[Screen] 屏幕拒收(0x1A): %s\n", key);
        }
        return;
    }
    if (len <= 2) return;   // 其余短应答(0x01成功等)不刷屏
    printf("[Screen] RX[%zd]: %s\n", len, cmd);

    int map_id = 0;
    int target_id = 0;

    // 在线TTS只进入屏幕喇叭队列，不调用voice.cpp、不触发车载喇叭。
    // HMI可发送UTF-8文本：TTS,请取走您的物品
    if (strncmp(cmd, "TTS,", 4) == 0)
    {
        if (!ScreenTTS_Speak(std::string(cmd + 4)))
            printf("[ScreenTTS] 请求被拒绝（文本为空、队列满或模块未启动）\n");
        return;
    }

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
    // 含义：屏幕首页启动按钮发来启动命令。HMI端事件自带page DConfirm切换。
    if (strcmp(cmd, "START") == 0 || strcmp(cmd, "start") == 0)
    {
        printf("[Screen] RX: %s\n", cmd);  // 打印接收到的正确指令
        if (delivery.Enabled())
        {
            // 配送模式：START=用户进入配送页（页面切换HMI已自行完成），导航仍由服务器驱动
            screen_delivery_page_active = true;
            printf("[Screen] 配送模式：进入配送页，等待服务器命令\n");
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

    // 3.5 END
    // 含义：DConfirm页退出按钮（HMI端事件自带page Main回主页），车端只记录页面状态
    if (strcmp(cmd, "END") == 0 || strcmp(cmd, "end") == 0)
    {
        printf("[Screen] RX: %s（退出配送页）\n", cmd);
        screen_delivery_page_active = false;
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

// 屏幕只有 b1~b5 五个槽位，而服务器快照会把全部历史订单一起下发。
// 原实现直接取 snap.orders[0..4]，订单一多，正在执行的那条就会被已完成的
// 挤出前五条，delivery_active_slot 返回 0，屏幕收到 active_slot=0 / viewing_slot=1，
// HMI 按 "viewing_slot > active_slot" 判成“等待前序订单完成”——文案变“等待中”、
// tsw d_s_4,0 禁用确认按钮，用户点了也不会发 LOAD_CONFIRMED。
// 2026-09-03 实测：积累到 15 条订单后活跃订单排在第 15 位，装货确认彻底点不动。
// 因此槽位只映射未完成的订单，已完成的不占位。
static int delivery_visible_orders(const StateSync& snap, int* idx, int max_n)
{
    int n = 0;
    for (size_t i = 0; i < snap.orders.size() && n < max_n; i++) {
        if (snap.orders[i].status != ORDER_COMPLETED) idx[n++] = (int)i;
    }
    if (n == 0) {   // 全部已完成：退化为显示最近几条，避免屏幕整片空白
        size_t total = snap.orders.size();
        size_t start = total > (size_t)max_n ? total - (size_t)max_n : 0;
        for (size_t i = start; i < total && n < max_n; i++) idx[n++] = (int)i;
    }
    return n;
}

// 订单槽位映射：当前订单（current_order_id优先，否则首个status2/3/4订单）的槽(1~5)
// 无活跃订单返回0
static int delivery_active_slot(const StateSync& snap)
{
    int idx[5];
    int n = delivery_visible_orders(snap, idx, 5);
    for (int s = 0; s < n; s++) {
        if (snap.has_current_order && snap.orders[idx[s]].order_id == snap.current_order_id)
            return s + 1;
    }
    for (int s = 0; s < n; s++) {
        int st = snap.orders[idx[s]].status;
        if (st == ORDER_WAIT_PICKUP_CONFIRM || st == ORDER_DELIVERING ||
            st == ORDER_WAIT_DROPOFF_CONFIRM)
            return s + 1;
    }
    return 0;
}

// 车端只下发"数据"，不再下发任何成句文案。
// d_s_1~d_s_4 的措辞全部交给 HMI 的定时事件按 active_phase 自己拼，
// 这样文字在设计器里就是对的编码、对的字库，也不存在两边抢同一个控件的问题。
//
// 车端负责的四个动态文本控件（HMI里必须建，且 vscope 设成【全局】，
// 跟已经能正常工作的 b1~b5 一样）：
// 没有活跃订单时统一写空串。
// 车端不再下发任何文案，一个字都不发。
// 屏幕上显示什么，完全由 HMI 的定时事件按 active_phase / active_slot / viewing_slot 自己决定。
// 车端只负责三件事：下发这三个变量、下发 b1~b5 的订单号、用 tsw 控制确认按钮能不能点。
// 这样做的好处是文字在设计器里就是对的编码和字库，串口上不再出现中文，
// 也彻底没有"车端和HMI抢同一个控件"的问题。
static void render_delivery_texts(const StateSync& snap, int slot)
{
    (void)snap;
    (void)slot;
}

void Screen_Render_Delivery(void)
{
    // 页面门控：屏幕不在DConfirm页时一个字节都不发——b1~b5/d_s_*与其他页同名控件冲突
    // （地图页b1=back/b2=clear会被裸发指令覆盖成"--"）。tsw指令不支持页面前缀，全靠此门控。
    if (!screen_delivery_page_active) return;

    // 快照版本变化立即刷新；未变化每1秒兜底刷新（覆盖屏幕按钮点击扰动的文案）
    static uint64_t last_ver = (uint64_t)-1;
    static uint32_t last_ms = 0;
    const StateSync& snap = delivery.Snapshot();
    uint32_t now = lq_get_tick_ms();
    if (snap.snapshot_version == last_ver && now - last_ms < 1000) return;
    last_ver = snap.snapshot_version;
    last_ms = now;

    int slot = delivery_active_slot(snap);

    // 订单号：b1~b5显示display_no（无订单的槽清空）。
    // 带DConfirm页面前缀：即使门控失效（END在途竞态）也不会污染其他页同名控件
    char txt[16];
    int vis[5];
    int vis_n = delivery_visible_orders(snap, vis, 5);
    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "DConfirm.b%d", i + 1);
        // 槽位号从1开始按顺序排，不用服务器的display_no
        // （display_no是全局流水号，会是"015"这种，跟屏幕上5个槽对不上）。
        // 空槽写空串：开机没订单时整个页面就是干净的。
        if (i < vis_n) {
            snprintf(txt, sizeof(txt), "%d", i + 1);
        } else {
            txt[0] = 0;
        }
        Screen_Send_Text(name, txt);
    }

    // 状态机变量：屏幕据此切换按钮文案（b1~b5内部逻辑）与确认按钮使能
    Screen_Send_Var("active_phase", snap.screen_phase);
    Screen_Send_Var("active_slot", slot);
    // viewing_slot 不再由车端下发：这是"用户正在看第几个订单"，属于屏幕本地状态。
    // 车端每秒回写一次的话，用户点 b1~b5 翻看别的订单会在1秒内被弹回当前订单，
    // HMI 里那两个 viewing_slot</>active_slot 分支就永远走不到。
    // 现在 HMI 的 b1~b5 点击事件自己设 viewing_slot.val，车端不碰。

    // 文案与确认按钮（车端权威写入，覆盖屏幕本地文案）
    render_delivery_texts(snap, slot);

    // 确认按钮使能：仅在等待确认(phase 2/4)且该订单未在待确认队列时允许点击
    bool enable_btn = (snap.screen_phase == SCREEN_PHASE_WAIT_PICKUP ||
                       snap.screen_phase == SCREEN_PHASE_WAIT_DROPOFF);
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "tsw d_s_4,%d", enable_btn ? 1 : 0);
    screen_write_command(cmd);
}

void Screen_Enter_Delivery_Page(void)
{
    // DConfirm页为配送模式常驻页（按页面名跳转，页面ID无关）
    char cmd[24];
    snprintf(cmd, sizeof(cmd), "page DConfirm");
    screen_write_command(cmd);
    screen_delivery_page_active = true;
    printf("[Screen] 配送模式：切换到DConfirm页\n");
}
