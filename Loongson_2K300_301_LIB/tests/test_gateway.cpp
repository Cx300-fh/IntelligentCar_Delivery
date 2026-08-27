/**
 * @file test_gateway.cpp
 * @brief 阶段4网关测试（车板运行，不碰电机）
 * @details 进程内嵌mock服务器线程，覆盖：建连/hello流/心跳/静默超时掉线/
 *          重连/64KiB超限断开/有界队列/Stop安全退出
 */

#include "car_gateway.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static int g_total = 0, g_fail = 0;
#define CHECK(cond) do { \
    ++g_total; \
    if (!(cond)) { ++g_fail; printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

//================================================================================
// mock 服务器（同进程线程）
//================================================================================
static std::atomic<bool> g_mock_silent{false};       // true=不回任何消息(模拟超时)
static std::atomic<bool> g_mock_send_sync{false};    // hello后附加state_sync
static std::atomic<bool> g_mock_send_goto{false};    // hello后附加goto_stop
static std::atomic<bool> g_mock_send_oversize{false};// 发送超限行
static std::atomic<bool> g_mock_running{false};

static std::mutex g_recv_mtx;
static std::deque<std::string> g_mock_received;      // mock收到的所有行

static void Mock_Send_Line(int fd, const std::string& line)
{
    std::string data = line + "\n";
    ssize_t n = send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    (void)n;
}

static void Mock_Thread(uint16_t port)
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) return;
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (bind(lfd, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(lfd, 4) != 0) {
        close(lfd);
        return;
    }
    fcntl(lfd, F_SETFL, fcntl(lfd, F_GETFL, 0) | O_NONBLOCK);

    while (g_mock_running.load()) {
        // 非阻塞accept（支持网关多次重连）
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            struct timespec ts = {0, 20 * 1000 * 1000};
            nanosleep(&ts, NULL);
            continue;
        }
        printf("  [mock] 接受连接\n");

        NdjsonDecoder dec;
        bool conn_open = true;
        while (conn_open && g_mock_running.load()) {
            struct pollfd pfd;
            pfd.fd = cfd;
            pfd.events = POLLIN;
            int rc = poll(&pfd, 1, 50);
            if (rc <= 0) continue;
            char buf[2048];
            ssize_t n = recv(cfd, buf, sizeof(buf), 0);
            if (n <= 0) { conn_open = false; break; }

            std::vector<std::string> lines;
            dec.Feed(buf, (size_t)n, &lines);
            for (size_t i = 0; i < lines.size(); i++) {
                {
                    std::lock_guard<std::mutex> lk(g_recv_mtx);
                    g_mock_received.push_back(lines[i]);
                }
                bool is_hello = lines[i].find("\"type\":\"hello\"") != std::string::npos;
                bool is_hb    = lines[i].find("\"type\":\"heartbeat\"") != std::string::npos;
                if (g_mock_silent.load()) continue;   // 静默模式：不回
                if (is_hello) {
                    Mock_Send_Line(cfd,
                        "{\"protocol_version\":1,\"type\":\"hello_ack\","
                        "\"message_id\":\"srv-h1\",\"vehicle_id\":0,\"accepted\":true,"
                        "\"session_id\":\"s1\",\"server_epoch\":\"e1\","
                        "\"heartbeat_interval_ms\":2000,\"connection_timeout_ms\":6000,"
                        "\"latest_snapshot_version\":1,\"latest_command_version\":0,"
                        "\"error\":null}");
                    if (g_mock_send_sync.load()) {
                        Mock_Send_Line(cfd,
                            "{\"protocol_version\":1,\"type\":\"state_sync\","
                            "\"message_id\":\"sync-1\",\"vehicle_id\":0,"
                            "\"server_epoch\":\"e1\",\"snapshot_version\":1,"
                            "\"screen_phase\":0,\"current_order_id\":null,"
                            "\"orders\":[],\"active_trip\":null,"
                            "\"authoritative_target\":null}");
                    }
                    if (g_mock_send_goto.load()) {
                        Mock_Send_Line(cfd,
                            "{\"protocol_version\":1,\"type\":\"goto_stop\","
                            "\"message_id\":\"cmd-1\",\"vehicle_id\":0,"
                            "\"command_version\":1,\"trip_id\":\"trip-1\","
                            "\"stop_id\":\"stop-1\",\"map_id\":1,"
                            "\"required_map_version\":3,"
                            "\"required_map_checksum\":\"sha256:x\","
                            "\"target_node\":13,\"location_id\":\"1:13\","
                            "\"location_name\":\"ZLY\",\"stop_type\":\"DROPOFF\","
                            "\"operations\":[{\"order_id\":\"ORD-1\","
                            "\"action\":\"DROPOFF\",\"order_version\":1}]}");
                    }
                } else if (is_hb) {
                    Mock_Send_Line(cfd,
                        "{\"protocol_version\":1,\"type\":\"heartbeat_ack\","
                        "\"message_id\":\"srv-hb\",\"vehicle_id\":0,"
                        "\"server_epoch\":\"e1\",\"snapshot_version\":1,"
                        "\"latest_command_version\":1}");
                }
            }
            if (g_mock_send_oversize.load()) {
                std::string big(65 * 1024, 'x');     // 超限无换行
                ssize_t m = send(cfd, big.data(), big.size(), MSG_NOSIGNAL);
                (void)m;
                g_mock_send_oversize.store(false);
            }
        }
        close(cfd);
        printf("  [mock] 连接结束\n");
    }
    close(lfd);
}

//================================================================================
// 辅助
//================================================================================
static size_t Mock_Received_Count()
{
    std::lock_guard<std::mutex> lk(g_recv_mtx);
    return g_mock_received.size();
}

static bool Mock_Wait_Count(size_t want, int timeout_ms,
                            bool (*pred)(size_t, size_t) = NULL)
{
    for (int t = 0; t < timeout_ms; t += 50) {
        size_t c = Mock_Received_Count();
        bool ok = pred ? pred(c, want) : (c >= want);
        if (ok) return true;
        struct timespec ts = {0, 50 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    return false;
}

static bool Wait_Link(CarGateway& gw, bool want, int timeout_ms)
{
    for (int t = 0; t < timeout_ms; t += 50) {
        if (gw.Is_Link_Up() == want) return true;
        struct timespec ts = {0, 50 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    return false;
}

// 等待收到指定类型的服务器消息
static bool Wait_Message(CarGateway& gw, ServerMsgType type, int timeout_ms,
                         ServerMessage* out = NULL)
{
    ServerMessage m;
    for (int t = 0; t < timeout_ms; t += 50) {
        while (gw.Poll_Server_Message(&m)) {
            if (m.type == type) { if (out) *out = m; return true; }
        }
        struct timespec ts = {0, 50 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    return false;
}

static std::string Make_Hello_Line(const char* msg_id)
{
    return std::string("{\"protocol_version\":1,\"type\":\"hello\",") +
        "\"message_id\":\"" + msg_id + "\",\"vehicle_id\":0," +
        "\"device_token\":\"test-token\",\"boot_id\":\"boot-1\"," +
        "\"software_version\":\"1.0.0\",\"map_id\":1,\"map_version\":3," +
        "\"current_node\":3,\"motion_state\":\"STOPPED\"}";
}

static std::string Make_Heartbeat_Line(int seq)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"protocol_version\":1,\"type\":\"heartbeat\","
        "\"message_id\":\"car-hb-%d\",\"vehicle_id\":0,\"sequence\":%d,"
        "\"current_node\":3,\"motion_state\":\"MOVING\"}", seq, seq);
    return std::string(buf);
}

//================================================================================
// 测试主体
//================================================================================
int main()
{
    printf("===== 阶段4网关测试（车板） =====\n");
    const uint16_t kPort = 18898;

    std::atomic<bool> loss_flag{false};

    // 启动mock
    g_mock_running.store(true);
    std::thread mock(Mock_Thread, kPort);
    struct timespec ts100 = {0, 100 * 1000 * 1000};
    nanosleep(&ts100, NULL);

    // 网关配置：快速超时便于测试
    CarGateway::Config cfg;
    cfg.host = "127.0.0.1";
    cfg.port = kPort;
    cfg.connect_timeout_ms = 1000;
    cfg.connection_timeout_ms = 1200;   // 测试加速（正式6000）
    cfg.poll_tick_ms = 50;
    CarGateway gw(cfg);
    gw.Set_Link_Loss_Callback([&loss_flag]() { loss_flag.store(true); });
    CHECK(gw.Start());

    // ---- 1. 建连 + hello流程 ----
    printf("-- 1.建连+hello --\n");
    CHECK(Wait_Link(gw, true, 3000));
    size_t before = Mock_Received_Count();
    CHECK(gw.Send_Line(Make_Hello_Line("car-hello-1")));
    CHECK(Mock_Wait_Count(before + 1, 3000));

    g_mock_send_sync.store(true);
    g_mock_send_goto.store(true);
    CHECK(gw.Send_Line(Make_Hello_Line("car-hello-2")));   // 触发mock回sync+goto
    ServerMessage m;
    CHECK(Wait_Message(gw, SRV_MSG_HELLO_ACK, 3000));
    CHECK(Wait_Message(gw, SRV_MSG_STATE_SYNC, 3000));
    CHECK(Wait_Message(gw, SRV_MSG_GOTO_STOP, 3000, &m));
    CHECK(m.goto_stop.command_version == 1);
    CHECK(m.goto_stop.target_node == 13);
    gw.Notify_Auth_Result(true);
    g_mock_send_sync.store(false);
    g_mock_send_goto.store(false);

    // ---- 2. 心跳往返 ----
    printf("-- 2.心跳往返 --\n");
    CHECK(gw.Send_Line(Make_Heartbeat_Line(1)));
    CHECK(gw.Send_Line(Make_Heartbeat_Line(2)));
    CHECK(Mock_Wait_Count(Mock_Received_Count() + 0, 2000));  // 已收到
    CHECK(Wait_Message(gw, SRV_MSG_HEARTBEAT_ACK, 3000));

    // ---- 3. 服务器静默 -> 超时掉线 -> link loss回调 ----
    printf("-- 3.静默超时掉线 --\n");
    loss_flag.store(false);
    g_mock_silent.store(true);
    CHECK(Wait_Link(gw, false, 3500));          // 1.2s超时+判定余量
    CHECK(loss_flag.load());                    // 掉线回调必须触发
    CHECK(gw.Reconnect_Count() >= 1);

    // ---- 4. 恢复 -> 重连成功 -> 重新hello ----
    printf("-- 4.重连 --\n");
    g_mock_silent.store(false);
    CHECK(Wait_Link(gw, true, 6000));           // 含1-2s退避
    size_t before2 = Mock_Received_Count();
    CHECK(gw.Send_Line(Make_Hello_Line("car-hello-3")));
    CHECK(Mock_Wait_Count(before2 + 1, 3000));
    CHECK(Wait_Message(gw, SRV_MSG_HELLO_ACK, 3000));

    // ---- 5. 64KiB超限行 -> 协议异常断开 ----
    printf("-- 5.超限断开 --\n");
    uint32_t rc_before = gw.Reconnect_Count();
    g_mock_send_oversize.store(true);
    CHECK(gw.Send_Line(Make_Heartbeat_Line(99)));   // 触发mock回包路径
    for (int t = 0; t < 3000; t += 50) {
        if (gw.Reconnect_Count() > rc_before) break;
        nanosleep(&ts100, NULL);
    }
    CHECK(gw.Reconnect_Count() > rc_before);

    // ---- 6. Stop安全退出 ----
    printf("-- 6.安全退出 --\n");
    gw.Stop();                                       // join返回即无死锁
    CHECK(!gw.Is_Link_Up());

    // ---- 7. 有界发送队列 ----
    printf("-- 7.有界队列 --\n");
    size_t ok_count = 0;
    for (int i = 0; i < 80; i++) {
        if (gw.Send_Line(Make_Heartbeat_Line(1000 + i))) ok_count++;
    }
    CHECK(ok_count == 64);                           // 上限64，之后必须拒绝

    g_mock_running.store(false);
    mock.join();

    printf("===== 结果: %d/%d 通过 =====\n", g_total - g_fail, g_total);
    return g_fail == 0 ? 0 : 1;
}
