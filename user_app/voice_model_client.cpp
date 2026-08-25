#include "voice_model_client.hpp"
#include "include.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

static int voice_model_fd = -1;
static struct sockaddr_in voice_model_addr;
static bool voice_model_ready = false;
static bool voice_model_pending = false;
static uint32_t voice_model_last_send_ms = 0;

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

static void json_escape(const char* in, char* out, int out_len)
{
    if (!out || out_len <= 0) return;
    if (!in) {
        out[0] = '\0';
        return;
    }

    int n = 0;
    for (int i = 0; in[i] && n < out_len - 1; i++) {
        if ((in[i] == '"' || in[i] == '\\') && n < out_len - 2) {
            out[n++] = '\\';
        }
        out[n++] = in[i];
    }
    out[n] = '\0';
}

bool Voice_Model_Init(void)
{
    if (voice_model_ready) return true;

    voice_model_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (voice_model_fd < 0) {
        printf("[VoiceModel] socket create failed\n");
        return false;
    }

    int flags = fcntl(voice_model_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(voice_model_fd, F_SETFL, flags | O_NONBLOCK);
    }

    memset(&voice_model_addr, 0, sizeof(voice_model_addr));
    voice_model_addr.sin_family = AF_INET;
    voice_model_addr.sin_port = htons(VOICE_MODEL_PORT);
    if (inet_pton(AF_INET, VOICE_MODEL_IP, &voice_model_addr.sin_addr) < 1) {
        printf("[VoiceModel] invalid model ip: %s\n", VOICE_MODEL_IP);
        close(voice_model_fd);
        voice_model_fd = -1;
        return false;
    }

    voice_model_ready = true;
    printf("[VoiceModel] UDP ready: %s:%d\n", VOICE_MODEL_IP, VOICE_MODEL_PORT);
    return true;
}

bool Voice_Model_Send_Command(const char* text)
{
    if (!text || text[0] == '\0') return false;
    if (!Voice_Model_Init()) return false;

    const NavStatus& status = nav_fsm.get_status();
    const NavTask& task = nav_fsm.get_task();
    char escaped[160];
    char msg[VOICE_MODEL_BUF_LEN];
    json_escape(text, escaped, sizeof(escaped));

    int len = snprintf(msg, sizeof(msg),
        "{\"type\":\"voice_command\",\"text\":\"%s\",\"map_id\":%d,"
        "\"current_node\":%d,\"order_active\":%s}",
        escaped,
        task.map_id,
        status.current_id > 0 ? status.current_id : 0,
        (order_scheduler.has_active_route() || order_scheduler.has_pending_station_action()) ? "true" : "false");

    if (len <= 0 || len >= (int)sizeof(msg)) {
        printf("[VoiceModel] command json too long\n");
        return false;
    }

    ssize_t sent = sendto(voice_model_fd, msg, len, 0,
                          (struct sockaddr*)&voice_model_addr,
                          sizeof(voice_model_addr));
    if (sent != len) {
        printf("[VoiceModel] send failed\n");
        return false;
    }

    printf("[VoiceModel] TX %s\n", msg);
    voice_model_pending = true;
    voice_model_last_send_ms = lq_get_tick_ms();
    return true;
}

bool Voice_Model_Handle_Local_Command(const char* action)
{
    if (!action) return false;

    if (strcmp(action, "confirm") == 0) {
        if (!order_scheduler.confirm_station_action()) {
            Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        }
        return true;
    }

    if (strcmp(action, "stop") == 0) {
        nav_fsm.cancel_task();
        Voice_Play_Event(VOICE_EVENT_TASK_CANCEL);
        return true;
    }

    if (strcmp(action, "query_status") == 0) {
        const NavTask& task = nav_fsm.get_task();
        const NavStatus& status = nav_fsm.get_status();
        printf("[VoiceModel] status: map=%d target=%d current=%d route_len=%d pending=%d\n",
               task.map_id, task.target_id, status.current_id,
               order_scheduler.get_current_route_len(),
               order_scheduler.has_pending_station_action() ? 1 : 0);
        return true;
    }

    return false;
}

static void handle_model_response(const char* json)
{
    char action[48] = {0};
    if (!json_get_string(json, "action", action, sizeof(action))) {
        printf("[VoiceModel] invalid response: missing action\n");
        Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        return;
    }

    printf("[VoiceModel] action=%s\n", action);

    if (Voice_Model_Handle_Local_Command(action)) return;

    if (strcmp(action, "submit_order") == 0) {
        int order_id = 0, map_id = 0, pickup_node = 0, dropoff_node = 0;
        if (json_get_int(json, "order_id", &order_id) &&
            json_get_int(json, "map_id", &map_id) &&
            json_get_int(json, "pickup_node", &pickup_node) &&
            json_get_int(json, "dropoff_node", &dropoff_node)) {
            order_scheduler.submit_order(order_id, map_id, pickup_node, dropoff_node);
        } else {
            Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        }
        return;
    }

    if (strcmp(action, "speak") == 0) {
        char event_name[64] = {0};
        if (json_get_string(json, "speak_event", event_name, sizeof(event_name))) {
            Voice_Play_Event(Voice_Event_From_Name(event_name));
        } else {
            Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        }
        return;
    }

    if (strcmp(action, "start") == 0) {
        if (screen_target_ready && nav_fsm.start_task(screen_selected_map, screen_selected_target)) {
            Voice_Play_Event(VOICE_EVENT_TASK_START);
        } else {
            Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
        }
        return;
    }

    printf("[VoiceModel] unknown action: %s\n", action);
    Voice_Play_Event(VOICE_EVENT_INVALID_COMMAND);
}

void Voice_Model_Poll(void)
{
    if (!voice_model_ready || voice_model_fd < 0) return;

    char buf[VOICE_MODEL_BUF_LEN];
    while (true) {
        ssize_t n = recvfrom(voice_model_fd, buf, sizeof(buf) - 1, 0, nullptr, nullptr);
        if (n <= 0) break;
        buf[n] = '\0';
        voice_model_pending = false;
        printf("[VoiceModel] RX %s\n", buf);
        handle_model_response(buf);
    }

    if (voice_model_pending && lq_get_tick_ms() - voice_model_last_send_ms > 2000) {
        voice_model_pending = false;
        printf("[VoiceModel] response timeout\n");
        Voice_Play_Event(VOICE_EVENT_MODEL_TIMEOUT);
    }
}
