/**
 * @file delivery_store.hpp
 * @brief 中央配送——本地可靠存储（命令版本/急停锁/未ACK事件持久化）
 * @details 写入时机：接受新命令前、新快照、生成可靠事件、事件ACK、急停变化
 *          禁止：每帧视觉循环、每5ms控制循环、每次心跳
 *          原子性：写临时文件 -> fflush+fsync -> rename
 */

#ifndef __DELIVERY_STORE_HPP
#define __DELIVERY_STORE_HPP

#include "delivery_types.hpp"
#include <string>

// 待确认可靠事件（arrived/user_action；重启后按原message_id重发）
struct PendingEvent {
    std::string message_id;
    std::string event_type;     // "arrived" / "user_action"
    std::string payload_json;   // 完整消息JSON（重发时原样使用）
    std::string created_at;
};

// 持久化数据
struct DeliveryStoreData {
    std::string server_epoch;
    uint64_t    snapshot_version;
    uint64_t    command_version;
    uint64_t    command_hash;         // 已接受命令的规范化内容哈希
    std::string command_json;         // 已接受命令原文（重启后校验目标一致性）
    bool        emergency_latched;
    int         last_current_node;    // 最后确认的当前位置
    int         map_id;
    int         map_version;
    std::string map_checksum;
    std::vector<PendingEvent> pending_events;

    DeliveryStoreData() : snapshot_version(0), command_version(0), command_hash(0),
                          emergency_latched(false), last_current_node(-1),
                          map_id(0), map_version(0) {}
};

// 保存：path.tmp -> fsync -> rename 到 path；成功返回true
bool Delivery_Store_Save(const std::string& path, const DeliveryStoreData& d);

// 加载：文件缺失/损坏/解析失败 -> 置默认值并返回false（不终止程序）
bool Delivery_Store_Load(const std::string& path, DeliveryStoreData* d);

#endif /* __DELIVERY_STORE_HPP */
