/**
 * @file delivery_store.cpp
 * @brief 中央配送——本地可靠存储 实现（临时文件+fsync+rename原子写）
 */

#include "delivery_store.hpp"
#include "delivery_protocol.hpp"

#include <cstdio>
#include <unistd.h>

using nlohmann::json;

static json Store_To_Json(const DeliveryStoreData& d)
{
    json ev = json::array();
    for (size_t i = 0; i < d.pending_events.size(); i++) {
        ev.push_back(json{
            {"message_id", d.pending_events[i].message_id},
            {"event_type", d.pending_events[i].event_type},
            {"payload_json", d.pending_events[i].payload_json},
            {"created_at", d.pending_events[i].created_at},
        });
    }
    return json{
        {"store_version", 1},
        {"server_epoch", d.server_epoch},
        {"snapshot_version", d.snapshot_version},
        {"command_version", d.command_version},
        {"command_hash", d.command_hash},
        {"command_json", d.command_json},
        {"emergency_latched", d.emergency_latched},
        {"last_current_node", d.last_current_node},
        {"map_id", d.map_id},
        {"map_version", d.map_version},
        {"map_checksum", d.map_checksum},
        {"pending_events", ev},
    };
}

bool Delivery_Store_Save(const std::string& path, const DeliveryStoreData& d)
{
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (f == NULL) return false;

    std::string data = Store_To_Json(d).dump();
    size_t written = fwrite(data.data(), 1, data.size(), f);
    if (written != data.size()) { fclose(f); unlink(tmp.c_str()); return false; }

    fflush(f);
    fsync(fileno(f));      // 落盘后才允许rename
    fclose(f);

    if (rename(tmp.c_str(), path.c_str()) != 0) {
        unlink(tmp.c_str());
        return false;
    }
    return true;
}

bool Delivery_Store_Load(const std::string& path, DeliveryStoreData* d)
{
    *d = DeliveryStoreData();   // 任何失败路径都保持默认值

    FILE* f = fopen(path.c_str(), "rb");
    if (f == NULL) return false;

    std::string data;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
    fclose(f);

    json j;
    try {
        j = json::parse(data);
    } catch (...) {
        return false;      // 损坏文件：保持默认（由上层上报FAULT并重新同步）
    }
    if (!j.is_object()) return false;

    auto get_str = [&](const char* k, std::string* out) {
        auto it = j.find(k);
        if (it != j.end() && it->is_string()) *out = it->get<std::string>();
    };
    auto get_u64 = [&](const char* k, uint64_t* out) {
        auto it = j.find(k);
        if (it != j.end() && it->is_number_unsigned()) *out = it->get<uint64_t>();
    };
    auto get_int = [&](const char* k, int* out) {
        auto it = j.find(k);
        if (it != j.end() && it->is_number_integer()) *out = it->get<int>();
    };
    auto get_bool = [&](const char* k, bool* out) {
        auto it = j.find(k);
        if (it != j.end() && it->is_boolean()) *out = it->get<bool>();
    };

    get_str("server_epoch", &d->server_epoch);
    get_u64("snapshot_version", &d->snapshot_version);
    get_u64("command_version", &d->command_version);
    get_u64("command_hash", &d->command_hash);
    get_str("command_json", &d->command_json);
    get_bool("emergency_latched", &d->emergency_latched);
    get_int("last_current_node", &d->last_current_node);
    get_int("map_id", &d->map_id);
    get_int("map_version", &d->map_version);
    get_str("map_checksum", &d->map_checksum);

    auto it = j.find("pending_events");
    if (it != j.end() && it->is_array()) {
        for (size_t i = 0; i < it->size(); i++) {
            const json& e = (*it)[i];
            if (!e.is_object()) continue;
            PendingEvent pe;
            auto s = e.find("message_id");
            if (s != e.end() && s->is_string()) pe.message_id = s->get<std::string>();
            s = e.find("event_type");
            if (s != e.end() && s->is_string()) pe.event_type = s->get<std::string>();
            s = e.find("payload_json");
            if (s != e.end() && s->is_string()) pe.payload_json = s->get<std::string>();
            s = e.find("created_at");
            if (s != e.end() && s->is_string()) pe.created_at = s->get<std::string>();
            if (!pe.message_id.empty() && !pe.payload_json.empty())
                d->pending_events.push_back(pe);
        }
    }
    return true;
}
