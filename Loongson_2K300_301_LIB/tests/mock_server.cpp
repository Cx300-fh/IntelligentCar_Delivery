/**
 * @file mock_server.cpp
 * @brief 板端模拟服务器（回环联调：与main同机运行，main连127.0.0.1:8898）
 * @details 三种模式：
 *   交互模式（默认）：stdin命令 sync/goto/hold/estop/resume/silent/drop/quit
 *   冒烟模式 --smoke [node]：自动 hello_ack→state_sync→心跳出现有效位置2s后
 *          goto(默认13)→收arrived→event_ack→结束
 *   观察模式 --observe：只回hello_ack/sync/心跳ack，不发命令，
 *          打印车端报告的current_node（验证摄像头+Tag识别），30s结束
 */

#include "delivery_protocol.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#include <string>
#include <vector>
#include <deque>

static bool g_observe = false;
static int  g_goto_node = 13;
static int  g_silent_after_s = -1;   // >0: 连接N秒后停止回话(连接保持,模拟WiFi断)
static bool g_auto_ack = true;       // false: 不回event_ack(验证可靠重发)

static uint64_t Now_Ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static std::string Make_Id(const char* p, uint64_t& seq) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%s-%llu", p, (unsigned long long)++seq);
    return buf;
}

static bool Send_Line(int fd, const std::string& line) {
    std::string d = line + "\n";
    size_t sent = 0;
    while (sent < d.size()) {
        ssize_t n = send(fd, d.data() + sent, d.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) { if (n < 0 && errno == EINTR) continue; return false; }
        sent += (size_t)n;
    }
    return true;
}

// 提取JSON字段字符串值（仅用于打印摘要，不用作解析）
static std::string J_Type(const std::string& line) {
    size_t p = line.find("\"type\":\"");
    if (p == std::string::npos) return "?";
    p += 8;
    return line.substr(p, line.find('"', p) - p);
}
static std::string J_Field(const std::string& line, const char* key) {
    std::string k = std::string("\"") + key + "\":";
    size_t p = line.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    while (p < line.size() && (line[p] == ' ' || line[p] == '"')) p++;
    size_t e = p;
    while (e < line.size() && line[e] != ',' && line[e] != '}' && line[e] != '"') e++;
    return line.substr(p, e - p);
}

static std::string Hello_Ack_Json(uint64_t& seq) {
    return std::string("{\"protocol_version\":1,\"type\":\"hello_ack\",") +
        "\"message_id\":\"" + Make_Id("srv-ha", seq) + "\",\"vehicle_id\":0," +
        "\"accepted\":true,\"session_id\":\"mock-s1\","
        "\"server_epoch\":\"mock-epoch-1\","
        "\"heartbeat_interval_ms\":2000,\"connection_timeout_ms\":6000,"
        "\"latest_snapshot_version\":1,\"latest_command_version\":0,"
        "\"error\":null}";
}

static std::string Sync_Json(uint64_t& seq, int phase = 0) {
    // 按phase生成带订单的快照（联调：模拟订单状态推进）
    std::string orders = "[]";
    std::string current = "null";
    if (phase >= 2 && phase <= 5) {
        char buf[768];
        snprintf(buf, sizeof(buf),
            "[{\"order_id\":\"ORD-MOCK-1\",\"display_no\":\"42\","
            "\"nickname\":\"MockUser\",\"status\":%d,"
            "\"status_code\":\"MOCK\",\"status_text\":\"mock\","
            "\"dispatch_state\":\"MOCK\",\"order_version\":%d,"
            "\"pickup_location_id\":\"1:5\",\"pickup_name\":\"Pickup\","
            "\"dropoff_location_id\":\"1:13\",\"dropoff_name\":\"Dropoff\","
            "\"item_summary\":\"TestItem\",\"button_label\":null,"
            "\"button_action\":null,\"button_enabled\":false}]",
            phase, phase);
        orders = buf;
        current = "\"ORD-MOCK-1\"";
    }
    std::string trip = (phase >= 2 && phase <= 4)
        ? "{\"trip_id\":\"mock-trip-1\",\"current_stop_id\":\"mock-stop-1\",\"loaded_count\":1}"
        : "null";
    return std::string("{\"protocol_version\":1,\"type\":\"state_sync\",") +
        "\"message_id\":\"" + Make_Id("srv-sync", seq) + "\",\"vehicle_id\":0," +
        "\"server_epoch\":\"mock-epoch-1\",\"snapshot_version\":" + std::to_string(phase + 10) + "," +
        "\"latest_command_version\":0,\"screen_phase\":" + std::to_string(phase) + "," +
        "\"current_order_id\":" + current + "," +
        "\"orders_total\":1,\"orders_included\":1,\"orders_truncated\":false," +
        "\"orders\":" + orders + "," +
        "\"active_trip\":" + trip + "," +
        "\"authoritative_target\":null}";
}

static std::string Goto_Json(uint64_t& seq, uint64_t ver, int node) {
    return std::string("{\"protocol_version\":1,\"type\":\"goto_stop\",") +
        "\"message_id\":\"" + Make_Id("srv-cmd", seq) + "\",\"vehicle_id\":0," +
        "\"command_version\":" + std::to_string(ver) + "," +
        "\"trip_id\":\"mock-trip-1\",\"stop_id\":\"mock-stop-1\",\"map_id\":1,"
        "\"required_map_version\":3,\"required_map_checksum\":\"sha256:mock\","
        "\"target_node\":" + std::to_string(node) + ","
        "\"location_id\":\"1:" + std::to_string(node) + "\","
        "\"location_name\":\"MOCK\",\"stop_type\":\"DROPOFF\","
        "\"operations\":[{\"order_id\":\"ORD-MOCK-1\",\"action\":\"DROPOFF\","
        "\"order_version\":1}]}";
}

static std::string Event_Ack_Json(uint64_t& seq, const std::string& reply_to) {
    return std::string("{\"protocol_version\":1,\"type\":\"event_ack\",") +
        "\"message_id\":\"" + Make_Id("srv-ea", seq) + "\",\"vehicle_id\":0," +
        "\"accepted\":true,\"reply_to\":\"" + reply_to + "\"," +
        "\"event_type\":\"arrived\",\"error\":null}";
}

// hold/emergency_stop通用命令JSON
static std::string Simple_Cmd_Json(uint64_t& seq, const char* type, uint64_t ver) {
    return std::string("{\"protocol_version\":1,\"type\":\"") + type + "\"," +
        "\"message_id\":\"" + Make_Id("srv-cmd", seq) + "\",\"vehicle_id\":0," +
        "\"command_version\":" + std::to_string(ver) + "," +
        "\"trip_id\":\"mock-trip-1\",\"stop_id\":\"mock-stop-1\"," +
        "\"reason\":\"mock-test\"}";
}

static std::string Resume_Json(uint64_t& seq, uint64_t ver) {
    return std::string("{\"protocol_version\":1,\"type\":\"resume\",") +
        "\"message_id\":\"" + Make_Id("srv-cmd", seq) + "\",\"vehicle_id\":0," +
        "\"command_version\":" + std::to_string(ver) + "," +
        "\"trip_id\":\"mock-trip-1\",\"stop_id\":\"mock-stop-1\"," +
        "\"resume_target_command_version\":" + std::to_string(ver - 1) + "," +
        "\"reason\":\"mock-test\"}";
}

static std::string Hb_Ack_Json(uint64_t& seq) {
    return std::string("{\"protocol_version\":1,\"type\":\"heartbeat_ack\",") +
        "\"message_id\":\"" + Make_Id("srv-hb", seq) + "\",\"vehicle_id\":0," +
        "\"server_epoch\":\"mock-epoch-1\",\"snapshot_version\":1,"
        "\"latest_command_version\":0}";
}

int main(int argc, char** argv)
{
    bool smoke = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--smoke") == 0) {
            smoke = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                g_goto_node = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--observe") == 0) {
            g_observe = true;
        } else if (strcmp(argv[i], "--silent-after") == 0 && i + 1 < argc) {
            g_silent_after_s = atoi(argv[++i]);
        }
    }

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(8898);
    if (bind(lfd, (struct sockaddr*)&a, sizeof(a)) != 0 || listen(lfd, 2) != 0) {
        printf("[mock] bind/listen失败: %s\n", strerror(errno));
        return 1;
    }
    printf("[mock] 监听8898（%s）...\n",
           g_observe ? "观察模式" : smoke ? "冒烟模式" : "交互模式");

    int cfd = accept(lfd, NULL, NULL);
    if (cfd < 0) { printf("[mock] accept失败\n"); return 1; }
    close(lfd);
    printf("[mock] 车端已连接\n");
    uint64_t t_conn = Now_Ms();

    uint64_t seq = 0, cmd_ver = 0;
    uint64_t t0 = Now_Ms();
    uint64_t t_hello_ack = 0, t_sync = 0, t_goto = 0, t_event_ack = 0;
    bool got_hello = false, got_arrived = false, got_ack = false;
    std::string arrived_reply_to;
    bool smoke_done = false;
    bool interactive_sent_sync = false;

    NdjsonDecoder dec;
    char buf[4096];

    while (true) {
        struct pollfd pfds[2];
        int nfd = 1;
        pfds[0].fd = cfd; pfds[0].events = POLLIN;
        if (!smoke) {
            pfds[1].fd = 0; pfds[1].events = POLLIN;   // stdin
            nfd = 2;
        }
        int rc = poll(pfds, nfd, 100);
        if (rc < 0) continue;

        // ---- 网络输入 ----
        if (pfds[0].revents & (POLLIN | POLLHUP)) {
            ssize_t n = recv(cfd, buf, sizeof(buf), 0);
            if (n <= 0) { printf("[mock] 连接断开\n"); break; }
            std::vector<std::string> lines;
            dec.Feed(buf, (size_t)n, &lines);
            for (size_t i = 0; i < lines.size(); i++) {
                const std::string& L = lines[i];
                std::string t = J_Type(L);
                printf("<< %-14s %s\n", t.c_str(),
                       L.size() > 110 ? (L.substr(0, 110) + "...").c_str() : L.c_str());

                if (t == "hello") {
                    got_hello = true;
                    bool silent_now = (g_silent_after_s > 0 &&
                                       Now_Ms() - t_conn >= (uint64_t)g_silent_after_s * 1000);
                    if (!silent_now) {
                        if (Send_Line(cfd, Hello_Ack_Json(seq))) t_hello_ack = Now_Ms();
                        printf(">> hello_ack\n");
                        if (smoke || g_observe) {
                            Send_Line(cfd, Sync_Json(seq));
                            t_sync = Now_Ms();
                            printf(">> state_sync\n");
                        }
                    } else {
                        printf("[mock] 已进入静默（连接保持，模拟WiFi断）\n");
                    }
                } else if (t == "heartbeat") {
                    printf("   [hb] node=%s action=%s nav=%s\n",
                           J_Field(L, "current_node").c_str(),
                           J_Field(L, "current_action").c_str(),
                           J_Field(L, "navigation_state").c_str());
                    bool silent_now = (g_silent_after_s > 0 &&
                                       Now_Ms() - t_conn >= (uint64_t)g_silent_after_s * 1000);
                    if (!silent_now &&
                        (!smoke || Now_Ms() - t_hello_ack > 500)) {
                        Send_Line(cfd, Hb_Ack_Json(seq));
                    }
                } else if (t == "sync_ack") {
                    printf("   sync结果: accepted=%s\n", J_Field(L, "accepted").c_str());
                    if (smoke) {
                        // sync_ack后2秒下发goto
                        t_goto = Now_Ms() + 2000;
                    }
                } else if (t == "command_ack") {
                    got_ack = true;
                    printf("   命令结果: accepted=%s %s\n",
                           J_Field(L, "accepted").c_str(),
                           J_Field(L, "code").c_str());
                } else if (t == "arrived") {
                    got_arrived = true;
                    arrived_reply_to = J_Field(L, "message_id");
                    if (g_auto_ack) {
                        // 自动回event_ack
                        Send_Line(cfd, Event_Ack_Json(seq, arrived_reply_to));
                        t_event_ack = Now_Ms();
                        printf(">> event_ack（arrived已确认：%s）\n", arrived_reply_to.c_str());
                    } else {
                        printf("   [noack模式] arrived收到但不回应：%s（车应每秒重发）\n",
                               arrived_reply_to.c_str());
                    }
                    if (smoke) smoke_done = true;
                } else if (t == "user_action") {
                    // 阶段6联调：自动回event_ack并推进订单快照（2→3装载 / 4→5取件）
                    std::string reply = J_Field(L, "message_id");
                    std::string action = J_Field(L, "action");
                    int new_status = (action.find("PICKUP") != std::string::npos) ? 3 : 5;
                    if (g_auto_ack) {
                        char eaj[320];
                        snprintf(eaj, sizeof(eaj),
                            "{\"protocol_version\":1,\"type\":\"event_ack\","
                            "\"message_id\":\"%s\",\"vehicle_id\":0,"
                            "\"accepted\":true,\"reply_to\":\"%s\","
                            "\"event_type\":\"user_action\",\"order_id\":\"ORD-MOCK-1\","
                            "\"new_status\":%d,\"new_order_version\":%d,"
                            "\"snapshot_version\":%d,\"error\":null}",
                            Make_Id("srv-ea", seq).c_str(), reply.c_str(),
                            new_status, new_status, new_status + 10);
                        Send_Line(cfd, eaj);
                        printf(">> event_ack（user_action已确认：%s 新状态%d）\n",
                               reply.c_str(), new_status);
                        // 下发新快照：装载后phase=3配送中；取件后phase=5完成
                        Send_Line(cfd, Sync_Json(seq, new_status));
                        printf(">> state_sync phase=%d\n", new_status);
                    } else {
                        printf("   [noack模式] user_action收到但不回应：%s（车应重发）\n",
                               reply.c_str());
                    }
                }
            }
        }

        // ---- 冒烟/观察时序推进 ----
        if (g_observe) {
            if (Now_Ms() - t0 > 30000) {
                printf("\n===== 观察结束 =====\n  连接与心跳正常，位置见上方日志\n");
                break;
            }
            continue;
        }
        if (smoke) {
            if (t_goto && Now_Ms() >= t_goto) {
                t_goto = 0;
                cmd_ver = 1;
                Send_Line(cfd, Goto_Json(seq, cmd_ver, g_goto_node));
                printf(">> goto_stop v1 -> node%d\n", g_goto_node);
            }
            if (smoke_done || Now_Ms() - t0 > 60000) {
                printf("\n===== 冒烟结果 =====\n");
                printf("  hello/hello_ack      : %s\n", got_hello ? "PASS" : "FAIL");
                printf("  state_sync/sync_ack  : %s\n", t_sync ? "PASS" : "FAIL");
                printf("  goto_stop/ack        : %s\n", got_ack ? "PASS" : "FAIL");
                printf("  arrived/event_ack    : %s\n", got_arrived ? "PASS" : "FAIL");
                break;
            }
            continue;
        }

        // ---- 交互命令 ----
        if (pfds[1].revents & POLLIN) {
            char cl[128];
            if (!fgets(cl, sizeof(cl), stdin)) break;
            std::string c = cl;
            while (!c.empty() && (c.back() == '\n' || c.back() == '\r')) c.pop_back();
            if (c == "sync") { Send_Line(cfd, Sync_Json(seq)); printf(">> state_sync\n"); }
            else if (c.rfind("phase", 0) == 0) {
                int ph = c.size() > 6 ? atoi(c.substr(6).c_str()) : 2;
                Send_Line(cfd, Sync_Json(seq, ph));
                printf(">> state_sync phase=%d（带订单）\n", ph);
            }
            else if (c.rfind("goto", 0) == 0) {
                int node = c.size() > 5 ? atoi(c.substr(5).c_str()) : 13;
                Send_Line(cfd, Goto_Json(seq, ++cmd_ver, node));
                printf(">> goto_stop v%llu -> node%d\n", (unsigned long long)cmd_ver, node);
            }
            // ===== 阶段7验收测试命令 =====
            else if (c.rfind("goto2", 0) == 0) {
                // 幂等测试：同一version同一内容连发两次（第二次应返回缓存ACK不重启导航）
                int node = c.size() > 6 ? atoi(c.substr(6).c_str()) : 13;
                uint64_t v = ++cmd_ver;
                Send_Line(cfd, Goto_Json(seq, v, node));
                usleep(1500 * 1000);
                Send_Line(cfd, Goto_Json(seq, v, node));
                printf(">> goto_stop v%llu -> node%d 发送两次（幂等测试）\n",
                       (unsigned long long)v, node);
            }
            else if (c == "stale") {
                // 旧版本测试：发比当前低的version（应STALE_COMMAND拒绝）
                if (cmd_ver > 1) {
                    Send_Line(cfd, Goto_Json(seq, cmd_ver - 1, 13));
                    printf(">> goto_stop v%llu（旧版本，应STALE拒绝）\n",
                           (unsigned long long)(cmd_ver - 1));
                } else printf("（先goto一次产生版本号）\n");
            }
            else if (c.rfind("conflict", 0) == 0) {
                // 版本冲突测试：同version不同内容（第一次接受/第二次应VERSION_CONFLICT停车）
                uint64_t v = ++cmd_ver;
                Send_Line(cfd, Goto_Json(seq, v, 5));
                usleep(1500 * 1000);
                Send_Line(cfd, Goto_Json(seq, v, 9));   // 同版本不同目标
                printf(">> goto_stop v%llu node5 + v%llu node9（冲突测试）\n",
                       (unsigned long long)v, (unsigned long long)v);
            }
            else if (c == "hold") {
                Send_Line(cfd, Simple_Cmd_Json(seq, "hold", ++cmd_ver));
                printf(">> hold v%llu\n", (unsigned long long)cmd_ver);
            }
            else if (c == "estop") {
                Send_Line(cfd, Simple_Cmd_Json(seq, "emergency_stop", ++cmd_ver));
                printf(">> emergency_stop v%llu\n", (unsigned long long)cmd_ver);
            }
            else if (c == "resume") {
                Send_Line(cfd, Resume_Json(seq, ++cmd_ver));
                printf(">> resume v%llu\n", (unsigned long long)cmd_ver);
            }
            else if (c == "noack") {
                g_auto_ack = false;
                printf("（停止回应arrived/user_action——验证可靠重发）\n");
            }
            else if (c == "ack") {
                g_auto_ack = true;
                printf("（恢复自动回应）\n");
            }
            else if (c == "quit") break;
            else if (!c.empty()) {
                printf("命令: sync | phase N | goto N | goto2 N | stale | conflict\n");
                printf("      hold | estop | resume | noack | ack | quit\n");
            }
        }
    }

    close(cfd);
    printf("[mock] 结束\n");
    return 0;
}
