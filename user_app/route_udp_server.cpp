#include "route_udp_server.hpp"
#include "include.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

static int route_server_fd = -1;
static bool route_server_ready = false;

static bool json_get_string(const char* json, const char* key, char* out, int out_len)
{
    if (!json || !key || !out || out_len <= 0) return false;

    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(json, pattern);
    if (!p) return false;

    p = strchr(p + strlen(pattern), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;

    int n = 0;
    while (*p && *p != '"' && n < out_len - 1) {
        out[n++] = *p++;
    }
    out[n] = '\0';
    return n > 0;
}

static bool json_get_int(const char* json, const char* key, int* out)
{
    if (!json || !key || !out) return false;

    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(json, pattern);
    if (!p) return false;

    p = strchr(p + strlen(pattern), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return sscanf(p, "%d", out) == 1;
}

static bool json_get_bool(const char* json, const char* key, bool* out)
{
    if (!json || !key || !out) return false;

    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(json, pattern);
    if (!p) return false;

    p = strchr(p + strlen(pattern), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    if (strncmp(p, "true", 4) == 0 || strncmp(p, "1", 1) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0 || strncmp(p, "0", 1) == 0) {
        *out = false;
        return true;
    }
    return false;
}

static void send_status_response(const struct sockaddr_in& src, bool ok, const char* action, const char* message)
{
    if (route_server_fd < 0) return;

    const NavStatus& status = nav_fsm.get_status();
    const NavTask& task = nav_fsm.get_task();
    const SchedulerEvent& event = order_scheduler.get_last_event();
    char response[ROUTE_SERVER_BUF_LEN];
    char path_buf[80];
    int path_pos = 0;

    path_pos += snprintf(path_buf + path_pos, sizeof(path_buf) - path_pos, "[");
    for (int i = 0; i < status.path_len && i < NAV_MAX_PATH_LEN && path_pos < (int)sizeof(path_buf) - 4; i++) {
        path_pos += snprintf(path_buf + path_pos, sizeof(path_buf) - path_pos,
                             "%s%d", i == 0 ? "" : ",", status.path[i]);
        if (path_pos >= (int)sizeof(path_buf) - 4) {
            path_pos = (int)sizeof(path_buf) - 4;
            break;
        }
    }
    snprintf(path_buf + path_pos, sizeof(path_buf) - path_pos, "]");

    int len = snprintf(response, sizeof(response),
        "{\"type\":\"status_response\",\"ok\":%s,\"action\":\"%s\","
        "\"message\":\"%s\",\"map_id\":%d,\"target_id\":%d,\"current_node\":%d,"
        "\"route_len\":%d,\"pending_station\":%s,\"state\":\"%s\","
        "\"nav_action\":\"%s\",\"prev_node\":%d,\"next_node\":%d,"
        "\"expected_next_node\":%d,\"path\":%s,\"path_index\":%d,"
        "\"task_active\":%s,\"pending_text_zh\":\"%s\",\"pending_text_en\":\"%s\","
        "\"scheduler_event\":\"%s\",\"order_id\":%d}",
        ok ? "true" : "false",
        action ? action : "",
        message ? message : "",
        task.map_id,
        task.target_id,
        status.current_id > 0 ? status.current_id : 0,
        order_scheduler.get_current_route_len(),
        order_scheduler.has_pending_station_action() ? "true" : "false",
        get_nav_state_name(status.state),
        get_action_name(status.current_action),
        status.prev_id > 0 ? status.prev_id : 0,
        status.next_id > 0 ? status.next_id : 0,
        status.expected_next_id > 0 ? status.expected_next_id : 0,
        path_buf,
        status.path_index,
        task.active ? "true" : "false",
        order_scheduler.get_pending_action_text_zh(),
        order_scheduler.get_pending_action_text_en(),
        get_scheduler_event_name(event.type),
        event.order_id);

    if (len > 0 && len < (int)sizeof(response)) {
        sendto(route_server_fd, response, len, 0,
               (const struct sockaddr*)&src, sizeof(src));
    } else {
        int fallback_len = snprintf(response, sizeof(response),
            "{\"type\":\"status_response\",\"ok\":%s,\"action\":\"%s\","
            "\"message\":\"status too long\",\"map_id\":%d,\"target_id\":%d,"
            "\"current_node\":%d,\"route_len\":%d,\"pending_station\":%s}",
            ok ? "true" : "false",
            action ? action : "",
            task.map_id,
            task.target_id,
            status.current_id > 0 ? status.current_id : 0,
            order_scheduler.get_current_route_len(),
            order_scheduler.has_pending_station_action() ? "true" : "false");
        if (fallback_len > 0 && fallback_len < (int)sizeof(response)) {
            sendto(route_server_fd, response, fallback_len, 0,
                   (const struct sockaddr*)&src, sizeof(src));
        }
    }
}

bool Route_Server_Init(void)
{
    if (route_server_ready) return true;

    route_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (route_server_fd < 0) {
        printf("[RouteServer] socket create failed\n");
        return false;
    }

    int reuse = 1;
    setsockopt(route_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ROUTE_SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(route_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[RouteServer] bind UDP %d failed\n", ROUTE_SERVER_PORT);
        close(route_server_fd);
        route_server_fd = -1;
        return false;
    }

    int flags = fcntl(route_server_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(route_server_fd, F_SETFL, flags | O_NONBLOCK);
    }

    route_server_ready = true;
    printf("[RouteServer] UDP listening on 0.0.0.0:%d\n", ROUTE_SERVER_PORT);
    return true;
}

static bool handle_route_message(const char* json, const struct sockaddr_in& src)
{
    char type[48] = {0};
    if (!json_get_string(json, "type", type, sizeof(type))) {
        printf("[RouteServer] invalid message: missing type\n");
        send_status_response(src, false, "invalid", "missing type");
        return false;
    }

    printf("[RouteServer] type=%s payload=%s\n", type, json);

    if (strcmp(type, "submit_order") == 0) {
        int order_id = 0, map_id = 0, pickup_node = 0, dropoff_node = 0;
        if (json_get_int(json, "order_id", &order_id) &&
            json_get_int(json, "map_id", &map_id) &&
            json_get_int(json, "pickup_node", &pickup_node) &&
            json_get_int(json, "dropoff_node", &dropoff_node)) {
            bool ok = order_scheduler.submit_order(order_id, map_id, pickup_node, dropoff_node);
            send_status_response(src, ok, type, ok ? "order accepted" : "order rejected");
            return ok;
        }
        send_status_response(src, false, type, "missing order fields");
        Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        return false;
    }

    if (strcmp(type, "confirm") == 0) {
        bool ok = order_scheduler.confirm_station_action();
        send_status_response(src, ok, type, ok ? "station confirmed" : "no station action");
        if (!ok) Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        return ok;
    }

    if (strcmp(type, "stop") == 0) {
        nav_fsm.cancel_task();
        Voice_Play_Event(VOICE_EVENT_TASK_CANCEL);
        send_status_response(src, true, type, "task stopped");
        return true;
    }

    if (strcmp(type, "edge_update") == 0) {
        int from = 0, to = 0, manual_penalty = 0, dynamic_penalty = 0;
        bool blocked = false;
        if (json_get_int(json, "from", &from) &&
            json_get_int(json, "to", &to)) {
            json_get_int(json, "manual_penalty", &manual_penalty);
            json_get_int(json, "dynamic_penalty", &dynamic_penalty);
            json_get_bool(json, "blocked", &blocked);

            bool ok = order_scheduler.update_edge_condition(
                from, to, manual_penalty, dynamic_penalty, blocked);
            send_status_response(src, ok, type, ok ? "edge updated" : "edge update failed");
            return ok;
        }
        send_status_response(src, false, type, "missing edge fields");
        Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        return false;
    }

    if (strcmp(type, "query_status") == 0) {
        send_status_response(src, true, type, "status ok");
        return true;
    }

    printf("[RouteServer] unknown type: %s\n", type);
    send_status_response(src, false, type, "unknown type");
    Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
    return false;
}

void Route_Server_Poll(void)
{
    if (!route_server_ready && !Route_Server_Init()) return;
    if (route_server_fd < 0) return;

    while (true) {
        char buf[ROUTE_SERVER_BUF_LEN];
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        ssize_t n = recvfrom(route_server_fd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr*)&src, &src_len);
        if (n <= 0) break;

        buf[n] = '\0';
        handle_route_message(buf, src);
    }
}
