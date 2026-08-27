/**
 * @file mock_server.cpp
 * @brief 板端模拟服务器（回环联调：与main同机运行，main连127.0.0.1:8898）
 * @details 两种模式：
 *   交互模式（默认）：stdin命令 sync/goto/hold/estop/resume/silent/drop/quit
 *   冒烟模式 --smoke：自动 hello_ack→state_sync→2s后goto 13→收arrived→event_ack→结束
 *          用于无场地自动验证协议闭环（无Tag时车应回 POSITION_UNKNOWN 拒绝，亦算通过路径）
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

static std::string Sync_Json(uint64_t& seq) {
    return std::string("{\"protocol_version\":1,\"type\":\"state_sync\",") +
        "\"message_id\":\"" + Make_Id("srv-sync", seq) + "\",\"vehicle_id\":0," +
        "\"server_epoch\":\"mock-epoch-1\",\"snapshot_version\":1,"
        "\"latest_command_version\":0,\"screen_phase\":0,"
        "\"current_order_id\":null,\"orders\":[],"
        "\"active_trip\":null,\"authoritative_target\":null}";
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
        "\"accepted\":true,\"reply_to\":\"" + reply_to + "\","
        "\"event_type\":\"arrived\",\"error\":null}";
}

static std::string Hb_Ack_Json(uint64_t& seq) {
    return std::string("{\"protocol_version\":1,\"type\":\"heartbeat_ack\",") +
        "\"message_id\":\"" + Make_Id("srv-hb", seq) + "\",\"vehicle_id\":0," +
        "\"server_epoch\":\"mock-epoch-1\",\"snapshot_version\":1,"
        "\"latest_command_version\":0}";
}

int main(int argc, char** argv)
{
    bool smoke = (argc > 1 && strcmp(argv[1], "--smoke") == 0);

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
    printf("[mock] 监听8898%s...\n", smoke ? "（冒烟模式）" : "（交互模式）");

    int cfd = accept(lfd, NULL, NULL);
    if (cfd < 0) { printf("[mock] accept失败\n"); return 1; }
    close(lfd);
    printf("[mock] 车端已连接\n");

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
                    if (Send_Line(cfd, Hello_Ack_Json(seq))) t_hello_ack = Now_Ms();
                    printf(">> hello_ack\n");
                    if (smoke) {
                        Send_Line(cfd, Sync_Json(seq));
                        t_sync = Now_Ms();
                        printf(">> state_sync\n");
                    }
                } else if (t == "heartbeat") {
                    if (!smoke || Now_Ms() - t_hello_ack > 500) {
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
                    // 自动回event_ack
                    Send_Line(cfd, Event_Ack_Json(seq, arrived_reply_to));
                    t_event_ack = Now_Ms();
                    printf(">> event_ack（arrived已确认：%s）\n", arrived_reply_to.c_str());
                    if (smoke) smoke_done = true;
                }
            }
        }

        // ---- 冒烟时序推进 ----
        if (smoke) {
            if (t_goto && Now_Ms() >= t_goto) {
                t_goto = 0;
                cmd_ver = 1;
                Send_Line(cfd, Goto_Json(seq, cmd_ver, 13));
                printf(">> goto_stop v1 -> node13\n");
            }
            if (smoke_done || Now_Ms() - t0 > 60000) {
                printf("\n===== 冒烟结果 =====\n");
                printf("  hello/hello_ack      : %s\n", got_hello ? "PASS" : "FAIL");
                printf("  state_sync/sync_ack  : %s\n", t_sync ? "PASS" : "FAIL");
                printf("  goto_stop/ack        : %s\n", got_ack ? "PASS(无Tag场地预期POSITION_UNKNOWN拒绝)" : "FAIL");
                printf("  arrived/event_ack    : %s\n", got_arrived ? "PASS" : "SKIP(无场地)");
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
            else if (c.rfind("goto", 0) == 0) {
                int node = c.size() > 5 ? atoi(c.substr(5).c_str()) : 13;
                Send_Line(cfd, Goto_Json(seq, ++cmd_ver, node));
                printf(">> goto_stop v%llu -> node%d\n", (unsigned long long)cmd_ver, node);
            }
            else if (c == "quit") break;
            else if (!c.empty()) printf("（未知命令: %s | sync/goto [n]/quit）\n", c.c_str());
        }
    }

    close(cfd);
    printf("[mock] 结束\n");
    return 0;
}
