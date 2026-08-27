/**
 * @file delivery_protocol.cpp
 * @brief 中央配送协议——JSON解析/序列化/NDJSON分帧/去重 实现
 */

#include "delivery_protocol.hpp"

#include <cstdio>
#include <ctime>
#include <chrono>

using nlohmann::json;

//================================================================================
// 错误码 <-> 名称
//================================================================================
const char* Delivery_Error_Name(DeliveryErrorCode code)
{
    switch (code) {
        case ERR_NONE:                   return "NONE";
        case ERR_INVALID_PROTOCOL:       return "INVALID_PROTOCOL";
        case ERR_AUTH_FAILED:            return "AUTH_FAILED";
        case ERR_WRONG_VEHICLE:          return "WRONG_VEHICLE";
        case ERR_NOT_SYNCHRONIZED:       return "NOT_SYNCHRONIZED";
        case ERR_STALE_COMMAND:          return "STALE_COMMAND";
        case ERR_VERSION_CONFLICT:       return "VERSION_CONFLICT";
        case ERR_BUSY_TARGET_FROZEN:     return "BUSY_TARGET_FROZEN";
        case ERR_INVALID_MAP:            return "INVALID_MAP";
        case ERR_MAP_VERSION_MISMATCH:   return "MAP_VERSION_MISMATCH";
        case ERR_INVALID_TARGET:         return "INVALID_TARGET";
        case ERR_POSITION_UNKNOWN:       return "POSITION_UNKNOWN";
        case ERR_UNREACHABLE_TARGET:     return "UNREACHABLE_TARGET";
        case ERR_INVALID_CAPACITY_STATE: return "INVALID_CAPACITY_STATE";
        case ERR_CAMERA_INIT_FAILED:     return "CAMERA_INIT_FAILED";
        case ERR_NAVIGATION_FAILED:      return "NAVIGATION_FAILED";
        case ERR_STOP_TIMEOUT:           return "STOP_TIMEOUT";
        case ERR_SCREEN_DATA_INVALID:    return "SCREEN_DATA_INVALID";
        case ERR_INTERNAL_ERROR:         return "INTERNAL_ERROR";
    }
    return "INTERNAL_ERROR";
}

static DeliveryErrorCode Error_From_Name(const std::string& name)
{
    static const struct { const char* name; DeliveryErrorCode code; } kMap[] = {
        {"NONE", ERR_NONE}, {"INVALID_PROTOCOL", ERR_INVALID_PROTOCOL},
        {"AUTH_FAILED", ERR_AUTH_FAILED}, {"WRONG_VEHICLE", ERR_WRONG_VEHICLE},
        {"NOT_SYNCHRONIZED", ERR_NOT_SYNCHRONIZED}, {"STALE_COMMAND", ERR_STALE_COMMAND},
        {"VERSION_CONFLICT", ERR_VERSION_CONFLICT}, {"BUSY_TARGET_FROZEN", ERR_BUSY_TARGET_FROZEN},
        {"INVALID_MAP", ERR_INVALID_MAP}, {"MAP_VERSION_MISMATCH", ERR_MAP_VERSION_MISMATCH},
        {"INVALID_TARGET", ERR_INVALID_TARGET}, {"POSITION_UNKNOWN", ERR_POSITION_UNKNOWN},
        {"UNREACHABLE_TARGET", ERR_UNREACHABLE_TARGET},
        {"INVALID_CAPACITY_STATE", ERR_INVALID_CAPACITY_STATE},
        {"CAMERA_INIT_FAILED", ERR_CAMERA_INIT_FAILED},
        {"NAVIGATION_FAILED", ERR_NAVIGATION_FAILED},
        {"STOP_TIMEOUT", ERR_STOP_TIMEOUT}, {"SCREEN_DATA_INVALID", ERR_SCREEN_DATA_INVALID},
        {"INTERNAL_ERROR", ERR_INTERNAL_ERROR},
    };
    for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); i++) {
        if (name == kMap[i].name) return kMap[i].code;
    }
    return ERR_INTERNAL_ERROR;
}

//================================================================================
// NDJSON 分帧
//================================================================================
bool NdjsonDecoder::Feed(const char* data, size_t len, std::vector<std::string>* out_lines)
{
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            if (!buf_.empty() && buf_.back() == '\r') buf_.pop_back();
            if (!buf_.empty()) out_lines->push_back(buf_);
            buf_.clear();
        } else {
            buf_.push_back(data[i]);
            if (buf_.size() > kMaxLineBytes) return false;  // 单行超限，调用方断开重连
        }
    }
    return true;
}

//================================================================================
// 工具函数
//================================================================================
std::string Delivery_Format_Utc_Now()
{
    using namespace std::chrono;
    int64_t ms_total = duration_cast<milliseconds>(
                           system_clock::now().time_since_epoch()).count();
    time_t sec = (time_t)(ms_total / 1000);
    int msec = (int)(ms_total % 1000);
    if (msec < 0) { msec += 1000; sec -= 1; }

    struct tm utc;
    gmtime_r(&sec, &utc);

    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
             utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
             utc.tm_hour, utc.tm_min, utc.tm_sec, msec);
    return std::string(buf);
}

std::string Delivery_Make_Message_Id(const std::string& prefix, uint64_t seq)
{
    char buf[kMaxMessageIdLen + 1];
    snprintf(buf, sizeof(buf), "%s-%llu", prefix.c_str(), (unsigned long long)seq);
    return std::string(buf);
}

uint64_t Delivery_Fnv1a64(const std::string& s)
{
    uint64_t hash = 1469598103934665603ULL;  // FNV offset basis
    for (size_t i = 0; i < s.size(); i++) {
        hash ^= (uint8_t)s[i];
        hash *= 1099511628211ULL;            // FNV prime
    }
    return hash;
}

uint64_t Delivery_Canonical_Command_Hash(const GotoStopCommand& cmd)
{
    // 只取业务字段；nlohmann json对象dump按key字典序输出，天然规范化
    json ops = json::array();
    for (size_t i = 0; i < cmd.operations.size(); i++) {
        ops.push_back(json{
            {"order_id", cmd.operations[i].order_id},
            {"action", cmd.operations[i].action},
            {"order_version", cmd.operations[i].order_version},
        });
    }
    json canonical = json{
        {"type", "goto_stop"},
        {"command_version", cmd.command_version},
        {"trip_id", cmd.trip_id},
        {"stop_id", cmd.stop_id},
        {"map_id", cmd.map_id},
        {"required_map_version", cmd.required_map_version},
        {"required_map_checksum", cmd.required_map_checksum},
        {"target_node", cmd.target_node},
        {"location_id", cmd.location_id},
        {"stop_type", cmd.stop_type == STOP_TYPE_PICKUP ? "PICKUP" :
                      cmd.stop_type == STOP_TYPE_DROPOFF ? "DROPOFF" : "UNKNOWN"},
        {"operations", ops},
    };
    return Delivery_Fnv1a64(canonical.dump());
}

//================================================================================
// 命令版本守卫
//================================================================================
CommandVersionGuard::Verdict CommandVersionGuard::Check(uint64_t version, uint64_t content_hash) const
{
    if (!has_accepted_) return ACCEPT_NEW;
    if (version < last_version_) return STALE;
    if (version == last_version_)
        return (content_hash == last_hash_) ? REPLAY_SAME : CONFLICT;
    return ACCEPT_NEW;
}

void CommandVersionGuard::Record(uint64_t version, uint64_t content_hash,
                                 const std::string& cached_ack_json)
{
    has_accepted_ = true;
    last_version_ = version;
    last_hash_ = content_hash;
    cached_ack_ = cached_ack_json;
}

//================================================================================
// 解析辅助
//================================================================================
static bool P_Str(const json& j, const char* key, std::string* out,
                  size_t max_len, bool required, ParseResult* r)
{
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        if (required) { *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
                        std::string("missing field: ") + key); return false; }
        return true;
    }
    if (!it->is_string()) {
        *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
              std::string("field not string: ") + key);
        return false;
    }
    *out = it->get<std::string>();
    if (out->size() > max_len) {
        *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
              std::string("field too long: ") + key);
        return false;
    }
    return true;
}

static bool P_Int(const json& j, const char* key, int* out,
                  bool required, ParseResult* r)
{
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        if (required) { *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
                        std::string("missing field: ") + key); return false; }
        return true;
    }
    if (!it->is_number_integer()) {
        *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
              std::string("field not integer: ") + key);
        return false;
    }
    *out = it->get<int>();
    return true;
}

static bool P_U64(const json& j, const char* key, uint64_t* out,
                  bool required, ParseResult* r)
{
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        if (required) { *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
                        std::string("missing field: ") + key); return false; }
        return true;
    }
    if (!it->is_number_unsigned()) {
        *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
              std::string("field not unsigned: ") + key);
        return false;
    }
    *out = it->get<uint64_t>();
    return true;
}

static bool P_Bool(const json& j, const char* key, bool* out,
                   bool required, ParseResult* r)
{
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        if (required) { *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
                        std::string("missing field: ") + key); return false; }
        return true;
    }
    if (!it->is_boolean()) {
        *r = ParseResult::Fail(ERR_INVALID_PROTOCOL,
              std::string("field not boolean: ") + key);
        return false;
    }
    *out = it->get<bool>();
    return true;
}

static ParseResult Parse_Common(const json& j, CommonHeader* h)
{
    ParseResult r;
    int pv = 0, vid = -1;
    if (!P_Int(j, "protocol_version", &pv, true, &r)) return r;
    if (pv != kProtocolVersion)
        return ParseResult::Fail(ERR_INVALID_PROTOCOL, "protocol_version must be 1");
    if (!P_Int(j, "vehicle_id", &vid, true, &r)) return r;
    if (vid != kVehicleId)
        return ParseResult::Fail(ERR_WRONG_VEHICLE, "vehicle_id must be 0");
    if (!P_Str(j, "type", &h->type, 32, true, &r)) return r;
    if (!P_Str(j, "message_id", &h->message_id, kMaxMessageIdLen, true, &r)) return r;
    P_Str(j, "sent_at", &h->sent_at, 40, false, &r);
    h->protocol_version = pv;
    h->vehicle_id = vid;
    return r;
}

static ParseResult Parse_Error_Obj(const json& j, ErrorInfo* e)
{
    ParseResult r;
    auto it = j.find("error");
    if (it == j.end() || it->is_null()) return r;
    if (!it->is_object())
        return ParseResult::Fail(ERR_INVALID_PROTOCOL, "error must be object");
    const json& ej = *it;
    std::string code_name;
    e->has_error = true;
    if (!P_Str(ej, "code", &code_name, 48, false, &r)) return r;
    e->code = Error_From_Name(code_name);
    P_Str(ej, "message", &e->message, kMaxFaultMessageLen, false, &r);
    return r;
}

//================================================================================
// 服务器消息解析
//================================================================================
ParseResult Delivery_Parse_Server_Message(const std::string& line, ServerMessage* out)
{
    *out = ServerMessage();   // 复用变量时必须清零，避免上次解析残留污染本次结果

    json j;
    try {
        j = json::parse(line);
    } catch (...) {
        return ParseResult::Fail(ERR_INVALID_PROTOCOL, "invalid JSON syntax");
    }
    if (!j.is_object())
        return ParseResult::Fail(ERR_INVALID_PROTOCOL, "message must be object");

    ParseResult r = Parse_Common(j, &out->header);
    if (!r.ok) return r;

    const std::string& t = out->header.type;
    if (t == "hello_ack") {
        out->type = SRV_MSG_HELLO_ACK;
        HelloAck& m = out->hello_ack;
        if (!P_Bool(j, "accepted", &m.accepted, true, &r)) return r;
        if (!P_Str(j, "session_id", &m.session_id, kMaxSessionIdLen, false, &r)) return r;
        if (!P_Str(j, "server_epoch", &m.server_epoch, kMaxServerEpochLen, false, &r)) return r;
        P_U64(j, "heartbeat_interval_ms", &m.heartbeat_interval_ms, false, &r);
        P_U64(j, "connection_timeout_ms", &m.connection_timeout_ms, false, &r);
        P_U64(j, "latest_snapshot_version", &m.latest_snapshot_version, false, &r);
        P_U64(j, "latest_command_version", &m.latest_command_version, false, &r);
        P_Str(j, "server_time", &m.server_time, 40, false, &r);
        return Parse_Error_Obj(j, &m.error);
    }
    if (t == "heartbeat_ack") {
        out->type = SRV_MSG_HEARTBEAT_ACK;
        HeartbeatAck& m = out->heartbeat_ack;
        if (!P_Str(j, "server_epoch", &m.server_epoch, kMaxServerEpochLen, true, &r)) return r;
        if (!P_U64(j, "snapshot_version", &m.snapshot_version, true, &r)) return r;
        if (!P_U64(j, "latest_command_version", &m.latest_command_version, true, &r)) return r;
        return r;
    }
    if (t == "state_sync") {
        out->type = SRV_MSG_STATE_SYNC;
        StateSync& m = out->state_sync;
        if (!P_Str(j, "server_epoch", &m.server_epoch, kMaxServerEpochLen, true, &r)) return r;
        if (!P_U64(j, "snapshot_version", &m.snapshot_version, true, &r)) return r;
        P_U64(j, "latest_command_version", &m.latest_command_version, false, &r);
        int phase = 0;
        if (!P_Int(j, "screen_phase", &phase, true, &r)) return r;
        if (phase < 0 || phase > 5)
            return ParseResult::Fail(ERR_INVALID_PROTOCOL, "screen_phase out of range");
        m.screen_phase = phase;
        {
            auto it = j.find("current_order_id");
            m.has_current_order = (it != j.end() && !it->is_null());
            if (m.has_current_order) {
                if (!it->is_string())
                    return ParseResult::Fail(ERR_INVALID_PROTOCOL, "current_order_id not string/null");
                m.current_order_id = it->get<std::string>();
            }
        }
        P_Int(j, "orders_total", &m.orders_total, false, &r);
        P_Int(j, "orders_included", &m.orders_included, false, &r);
        P_Bool(j, "orders_truncated", &m.orders_truncated, false, &r);

        auto oit = j.find("orders");
        if (oit == j.end() || !oit->is_array())
            return ParseResult::Fail(ERR_INVALID_PROTOCOL, "orders missing/not array");
        if (oit->size() > kMaxOrdersCached)
            return ParseResult::Fail(ERR_INVALID_CAPACITY_STATE, "too many orders in snapshot");
        for (size_t i = 0; i < oit->size(); i++) {
            const json& oj = (*oit)[i];
            if (!oj.is_object())
                return ParseResult::Fail(ERR_INVALID_PROTOCOL, "order item not object");
            OrderInfo o;
            if (!P_Str(oj, "order_id", &o.order_id, kMaxOrderIdLen, true, &r)) return r;
            P_Str(oj, "display_no", &o.display_no, 16, false, &r);
            P_Str(oj, "nickname", &o.nickname, kMaxNicknameLen, false, &r);
            if (!P_Int(oj, "status", &o.status, true, &r)) return r;
            if (o.status < 1 || o.status > 5)
                return ParseResult::Fail(ERR_INVALID_PROTOCOL,
                      "order status must be 1..5 (fake status-0 order rejected): " + o.order_id);
            P_Str(oj, "status_code", &o.status_code, 32, false, &r);
            P_Str(oj, "status_text", &o.status_text, 32, false, &r);
            {
                std::string ds;
                P_Str(oj, "dispatch_state", &ds, 24, false, &r);
                if (ds == "TO_PICKUP") o.dispatch_state = DISPATCH_TO_PICKUP;
                else if (ds == "WAIT_PICKUP") o.dispatch_state = DISPATCH_WAIT_PICKUP;
                else if (ds == "TO_DROPOFF") o.dispatch_state = DISPATCH_TO_DROPOFF;
                else if (ds == "WAIT_DROPOFF") o.dispatch_state = DISPATCH_WAIT_DROPOFF;
                else if (ds == "DONE") o.dispatch_state = DISPATCH_DONE;
            }
            P_U64(oj, "order_version", &o.order_version, false, &r);
            P_Str(oj, "pickup_location_id", &o.pickup_location_id, kMaxLocationIdLen, false, &r);
            P_Str(oj, "pickup_name", &o.pickup_name, kMaxLocationNameLen, false, &r);
            P_Str(oj, "dropoff_location_id", &o.dropoff_location_id, kMaxLocationIdLen, false, &r);
            P_Str(oj, "dropoff_name", &o.dropoff_name, kMaxLocationNameLen, false, &r);
            P_Str(oj, "item_summary", &o.item_summary, kMaxItemSummaryLen, false, &r);
            {
                auto bit = oj.find("button_label");
                if (bit != oj.end() && !bit->is_null()) {
                    if (!bit->is_string())
                        return ParseResult::Fail(ERR_INVALID_PROTOCOL, "button_label not string/null");
                    o.has_button = true;
                    o.button_label = bit->get<std::string>();
                }
            }
            P_Str(oj, "button_action", &o.button_action, 48, false, &r);
            P_Bool(oj, "button_enabled", &o.button_enabled, false, &r);
            m.orders.push_back(o);
        }

        {   // active_trip（可null）
            auto it = j.find("active_trip");
            if (it != j.end() && !it->is_null()) {
                if (!it->is_object())
                    return ParseResult::Fail(ERR_INVALID_PROTOCOL, "active_trip not object/null");
                m.active_trip.present = true;
                const json& tj = *it;
                if (!P_Str(tj, "trip_id", &m.active_trip.trip_id, kMaxTripIdLen, true, &r)) return r;
                P_Str(tj, "current_stop_id", &m.active_trip.current_stop_id, kMaxStopIdLen, false, &r);
                P_Int(tj, "loaded_count", &m.active_trip.loaded_count, false, &r);
            }
        }
        {   // authoritative_target（可null）
            auto it = j.find("authoritative_target");
            if (it != j.end() && !it->is_null()) {
                if (!it->is_object())
                    return ParseResult::Fail(ERR_INVALID_PROTOCOL, "authoritative_target not object/null");
                m.authoritative_target.present = true;
                const json& tj = *it;
                if (!P_Str(tj, "trip_id", &m.authoritative_target.trip_id, kMaxTripIdLen, true, &r)) return r;
                P_Str(tj, "stop_id", &m.authoritative_target.stop_id, kMaxStopIdLen, false, &r);
                P_Int(tj, "map_id", &m.authoritative_target.map_id, false, &r);
                P_Int(tj, "target_node", &m.authoritative_target.target_node, false, &r);
                P_U64(tj, "command_version", &m.authoritative_target.command_version, false, &r);
            }
        }
        return r;
    }
    if (t == "goto_stop") {
        out->type = SRV_MSG_GOTO_STOP;
        GotoStopCommand& m = out->goto_stop;
        if (!P_U64(j, "command_version", &m.command_version, true, &r)) return r;
        if (!P_Str(j, "trip_id", &m.trip_id, kMaxTripIdLen, true, &r)) return r;
        if (!P_Str(j, "stop_id", &m.stop_id, kMaxStopIdLen, true, &r)) return r;
        if (!P_Int(j, "map_id", &m.map_id, true, &r)) return r;
        P_Int(j, "required_map_version", &m.required_map_version, false, &r);
        P_Str(j, "required_map_checksum", &m.required_map_checksum, kMaxChecksumLen, false, &r);
        if (!P_Int(j, "target_node", &m.target_node, true, &r)) return r;
        P_Str(j, "location_id", &m.location_id, kMaxLocationIdLen, false, &r);
        P_Str(j, "location_name", &m.location_name, kMaxLocationNameLen, false, &r);
        {
            std::string st;
            P_Str(j, "stop_type", &st, 16, false, &r);
            if (st == "PICKUP") m.stop_type = STOP_TYPE_PICKUP;
            else if (st == "DROPOFF") m.stop_type = STOP_TYPE_DROPOFF;
        }
        {
            auto it = j.find("operations");
            if (it == j.end() || !it->is_array())
                return ParseResult::Fail(ERR_INVALID_PROTOCOL, "operations missing/not array");
            for (size_t i = 0; i < it->size(); i++) {
                const json& oj = (*it)[i];
                if (!oj.is_object())
                    return ParseResult::Fail(ERR_INVALID_PROTOCOL, "operation not object");
                GotoStopOp op;
                if (!P_Str(oj, "order_id", &op.order_id, kMaxOrderIdLen, true, &r)) return r;
                if (!P_Str(oj, "action", &op.action, 16, true, &r)) return r;
                P_U64(oj, "order_version", &op.order_version, false, &r);
                m.operations.push_back(op);
            }
        }
        return r;
    }
    if (t == "hold") {
        out->type = SRV_MSG_HOLD;
        HoldCommand& m = out->hold;
        if (!P_U64(j, "command_version", &m.command_version, true, &r)) return r;
        P_Str(j, "reason", &m.reason, kMaxFaultMessageLen, false, &r);
        P_Str(j, "trip_id", &m.trip_id, kMaxTripIdLen, false, &r);
        P_Str(j, "stop_id", &m.stop_id, kMaxStopIdLen, false, &r);
        return r;
    }
    if (t == "emergency_stop") {
        out->type = SRV_MSG_EMERGENCY_STOP;
        EmergencyStopCommand& m = out->emergency_stop;
        if (!P_U64(j, "command_version", &m.command_version, true, &r)) return r;
        P_Str(j, "reason", &m.reason, kMaxFaultMessageLen, false, &r);
        P_Str(j, "trip_id", &m.trip_id, kMaxTripIdLen, false, &r);
        P_Str(j, "stop_id", &m.stop_id, kMaxStopIdLen, false, &r);
        return r;
    }
    if (t == "resume") {
        out->type = SRV_MSG_RESUME;
        ResumeCommand& m = out->resume;
        if (!P_U64(j, "command_version", &m.command_version, true, &r)) return r;
        if (!P_Str(j, "trip_id", &m.trip_id, kMaxTripIdLen, true, &r)) return r;
        if (!P_Str(j, "stop_id", &m.stop_id, kMaxStopIdLen, true, &r)) return r;
        P_U64(j, "resume_target_command_version", &m.resume_target_command_version, false, &r);
        P_Str(j, "reason", &m.reason, kMaxFaultMessageLen, false, &r);
        return r;
    }
    if (t == "event_ack") {
        out->type = SRV_MSG_EVENT_ACK;
        EventAck& m = out->event_ack;
        if (!P_Bool(j, "accepted", &m.accepted, true, &r)) return r;
        if (!P_Str(j, "reply_to", &m.reply_to, kMaxMessageIdLen, true, &r)) return r;
        P_Str(j, "event_type", &m.event_type, 24, false, &r);
        {
            auto it = j.find("order_id");
            m.has_order = (it != j.end() && !it->is_null());
            if (m.has_order) m.order_id = it->get<std::string>();
        }
        {
            auto it = j.find("new_status");
            m.has_new_status = (it != j.end() && !it->is_null());
            if (m.has_new_status) m.new_status = it->get<int>();
        }
        {
            auto it = j.find("new_order_version");
            m.has_new_order_version = (it != j.end() && !it->is_null());
            if (m.has_new_order_version) m.new_order_version = it->get<uint64_t>();
        }
        P_U64(j, "snapshot_version", &m.snapshot_version, false, &r);
        return Parse_Error_Obj(j, &m.error);
    }

    return ParseResult::Fail(ERR_INVALID_PROTOCOL, "unknown message type: " + t);
}

//================================================================================
// 小车消息序列化
//================================================================================
static json Common_To_Json(const CommonHeader& h)
{
    return json{
        {"protocol_version", h.protocol_version},
        {"type", h.type},
        {"message_id", h.message_id},
        {"vehicle_id", h.vehicle_id},
        {"sent_at", h.sent_at},
    };
}

static json Error_To_Json(const ErrorInfo& e)
{
    if (!e.has_error) return json(nullptr);
    return json{{"code", Delivery_Error_Name((DeliveryErrorCode)e.code)},
                {"message", e.message}};
}

std::string Delivery_Serialize_Hello(const Hello& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["device_token"] = m.device_token;
    j["boot_id"] = m.boot_id;
    j["software_version"] = m.software_version;
    j["last_server_epoch"] = m.last_server_epoch;
    j["last_snapshot_version"] = m.last_snapshot_version;
    j["last_command_version"] = m.last_command_version;
    j["map_id"] = m.map_id;
    j["map_version"] = m.map_version;
    j["map_checksum"] = m.map_checksum;
    j["current_node"] = m.current_node;
    j["position_valid"] = m.position_valid;
    j["motion_state"] = m.motion_state == MOTION_MOVING ? "MOVING" :
                        m.motion_state == MOTION_HOLDING ? "HOLDING" : "STOPPED";
    j["emergency_latched"] = m.emergency_latched;
    json ids = json::array();
    for (size_t i = 0; i < m.pending_event_ids.size(); i++)
        ids.push_back(m.pending_event_ids[i]);
    j["pending_event_ids"] = ids;
    return j.dump();
}

std::string Delivery_Serialize_Heartbeat(const Heartbeat& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["boot_id"] = m.boot_id;
    j["sequence"] = m.sequence;
    j["uptime_ms"] = m.uptime_ms;
    j["map_id"] = m.map_id;
    j["current_node"] = m.current_node;
    j["last_tag_id"] = m.last_tag_id;
    j["last_tag_at"] = m.last_tag_at;
    j["mile_since_node"] = m.mile_since_node;
    j["position_source"] = m.position_source == POSITION_SOURCE_APRILTAG ? "APRILTAG" :
                           m.position_source == POSITION_SOURCE_ODOMETRY ? "ODOMETRY" : "NONE";
    j["motion_state"] = m.motion_state == MOTION_MOVING ? "MOVING" :
                        m.motion_state == MOTION_HOLDING ? "HOLDING" : "STOPPED";
    j["navigation_state"] = m.navigation_state;
    j["current_action"] = m.current_action;
    j["trip_id"] = m.has_trip ? json(m.trip_id) : json(nullptr);
    j["stop_id"] = m.has_stop ? json(m.stop_id) : json(nullptr);
    j["command_version"] = m.command_version;
    j["battery_percent"] = m.has_battery ? json(m.battery_percent) : json(nullptr);
    j["fault_code"] = m.has_fault_code ? json(m.fault_code) : json(nullptr);
    return j.dump();
}

std::string Delivery_Serialize_SyncAck(const SyncAck& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["reply_to"] = m.reply_to;
    j["server_epoch"] = m.server_epoch;
    j["snapshot_version"] = m.snapshot_version;
    j["accepted"] = m.accepted;
    j["error"] = Error_To_Json(m.error);
    return j.dump();
}

std::string Delivery_Serialize_CommandAck(const CommandAck& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["reply_to"] = m.reply_to;
    j["accepted"] = m.accepted;
    j["command_version"] = m.command_version;
    j["execution_state"] = m.execution_state;
    j["path"] = m.path;
    j["error"] = Error_To_Json(m.error);
    return j.dump();
}

std::string Delivery_Serialize_NavigationStatus(const NavigationStatusReport& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["trip_id"] = m.has_trip ? json(m.trip_id) : json(nullptr);
    j["stop_id"] = m.has_stop ? json(m.stop_id) : json(nullptr);
    j["command_version"] = m.command_version;
    j["navigation_state"] = m.navigation_state;
    j["motion_state"] = m.motion_state == MOTION_MOVING ? "MOVING" :
                        m.motion_state == MOTION_HOLDING ? "HOLDING" : "STOPPED";
    j["map_id"] = m.map_id;
    j["current_node"] = m.current_node;
    j["prev_node"] = m.prev_node;
    j["next_node"] = m.next_node;
    j["actual_path"] = m.actual_path;
    j["path_index"] = m.path_index;
    j["target_node"] = m.target_node;
    j["position_valid"] = m.position_valid;
    j["fault_code"] = m.has_fault_code ? json(m.fault_code) : json(nullptr);
    return j.dump();
}

std::string Delivery_Serialize_Arrived(const ArrivedEvent& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["trip_id"] = m.trip_id;
    j["stop_id"] = m.stop_id;
    j["command_version"] = m.command_version;
    j["map_id"] = m.map_id;
    j["node_id"] = m.node_id;
    j["tag_id"] = m.tag_id;
    j["motion_state"] = m.motion_state == MOTION_MOVING ? "MOVING" :
                        m.motion_state == MOTION_HOLDING ? "HOLDING" : "STOPPED";
    j["position_source"] = m.position_source == POSITION_SOURCE_APRILTAG ? "APRILTAG" :
                           m.position_source == POSITION_SOURCE_ODOMETRY ? "ODOMETRY" : "NONE";
    return j.dump();
}

std::string Delivery_Serialize_UserAction(const UserActionEvent& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["trip_id"] = m.trip_id;
    j["stop_id"] = m.stop_id;
    j["order_id"] = m.order_id;
    j["action"] = m.action == USER_ACTION_CONFIRM_PICKUP_LOADED ?
                    "CONFIRM_PICKUP_LOADED" : "CONFIRM_DROPOFF_TAKEN";
    j["expected_status"] = m.expected_status;
    j["order_version"] = m.order_version;
    j["display_revision"] = m.display_revision;
    return j.dump();
}

std::string Delivery_Serialize_Fault(const FaultReport& m, const CommonHeader& h)
{
    json j = Common_To_Json(h);
    j["fault_code"] = Delivery_Error_Name((DeliveryErrorCode)m.fault_code);
    j["fault_level"] = m.fault_level == FAULT_WARNING ? "WARNING" :
                       m.fault_level == FAULT_STOP_REQUIRED ? "STOP_REQUIRED" : "FATAL";
    j["fault_message"] = m.fault_message;
    j["motion_state"] = m.motion_state == MOTION_MOVING ? "MOVING" :
                        m.motion_state == MOTION_HOLDING ? "HOLDING" : "STOPPED";
    j["trip_id"] = m.has_trip ? json(m.trip_id) : json(nullptr);
    j["stop_id"] = m.has_stop ? json(m.stop_id) : json(nullptr);
    j["command_version"] = m.command_version;
    j["map_id"] = m.map_id;
    j["current_node"] = m.current_node;
    return j.dump();
}

//================================================================================
// 业务校验
//================================================================================
ParseResult Delivery_Check_Capacity(const std::vector<OrderInfo>& orders)
{
    int loaded = 0;
    for (size_t i = 0; i < orders.size(); i++) {
        if (orders[i].status == ORDER_DELIVERING ||
            orders[i].status == ORDER_WAIT_DROPOFF_CONFIRM) loaded++;
    }
    if (loaded > kMaxLoadedOrders)
        return ParseResult::Fail(ERR_INVALID_CAPACITY_STATE,
              "loaded orders (status 3/4) exceed capacity 5");
    ParseResult r;
    return r;
}

ParseResult Delivery_Check_Order_Valid(const OrderInfo& o)
{
    if (o.status < 1 || o.status > 5)
        return ParseResult::Fail(ERR_INVALID_PROTOCOL, "order status out of range");
    if (o.order_id.empty() || o.order_id.size() > kMaxOrderIdLen)
        return ParseResult::Fail(ERR_INVALID_PROTOCOL, "order_id length invalid");
    ParseResult r;
    return r;
}
