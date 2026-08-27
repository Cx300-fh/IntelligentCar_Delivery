/**
 * @file delivery_types.hpp
 * @brief 中央配送协议——类型定义（纯数据层，不依赖工程任何头文件）
 * @details 依据 protocol-v1.md + 任务书V2 + Kevin 222.md 修订
 *          阶段3交付：枚举、结构体、长度限制、版本去重规则
 */

#ifndef __DELIVERY_TYPES_HPP
#define __DELIVERY_TYPES_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

//================================================================================
// 协议常量
//================================================================================
const int    kProtocolVersion = 1;          // protocol_version 固定值
const int    kVehicleId       = 0;          // 单车系统，vehicle_id 固定0
const size_t kMaxLineBytes    = 64 * 1024;  // NDJSON 单行最大长度（UTF-8字节）
const size_t kMaxOrdersCached = 20;         // 小车订单缓存上限
const int    kMaxLoadedOrders = 5;          // status=3/4 订单合计上限（容量）

// 字符串字段最大UTF-8字节数（任务书第六节建议长度）
const size_t kMaxMessageIdLen     = 64;
const size_t kMaxBootIdLen        = 64;
const size_t kMaxOrderIdLen       = 48;
const size_t kMaxTripIdLen        = 64;
const size_t kMaxStopIdLen        = 64;
const size_t kMaxLocationIdLen    = 64;
const size_t kMaxNicknameLen      = 48;
const size_t kMaxLocationNameLen  = 48;
const size_t kMaxItemSummaryLen   = 96;
const size_t kMaxFaultMessageLen  = 256;
const size_t kMaxSessionIdLen     = 64;
const size_t kMaxServerEpochLen   = 64;
const size_t kMaxChecksumLen      = 80;
const size_t kMaxSoftwareVerLen   = 32;

//================================================================================
// 枚举
//================================================================================

// 订单真实状态 1..5（服务器权威，小车只显示）
enum OrderStatus {
    ORDER_QUEUED               = 1,  // 等待处理或正在去取件点
    ORDER_WAIT_PICKUP_CONFIRM  = 2,  // 到达取件点，等待装载确认
    ORDER_DELIVERING           = 3,  // 物品已在车内，配送中
    ORDER_WAIT_DROPOFF_CONFIRM = 4,  // 到达目的地，等待取件确认
    ORDER_COMPLETED            = 5,  // 订单完成
};

// 内部调度状态
enum DispatchState {
    DISPATCH_UNASSIGNED   = 0,
    DISPATCH_TO_PICKUP    = 1,
    DISPATCH_WAIT_PICKUP  = 2,
    DISPATCH_TO_DROPOFF   = 3,
    DISPATCH_WAIT_DROPOFF = 4,
    DISPATCH_DONE         = 5,
};

// 屏幕阶段 0..5（0=无订单可显示）
enum ScreenPhase {
    SCREEN_PHASE_NONE            = 0,  // 当前暂无订单
    SCREEN_PHASE_TO_PICKUP       = 1,  // 等待处理或前往取件点
    SCREEN_PHASE_WAIT_PICKUP     = 2,  // 等待装载确认
    SCREEN_PHASE_DELIVERING      = 3,  // 配送中
    SCREEN_PHASE_WAIT_DROPOFF    = 4,  // 等待取件确认
    SCREEN_PHASE_ALL_DONE        = 5,  // 已完成
};

// 运动状态
enum MotionState {
    MOTION_STOPPED = 0,
    MOTION_MOVING  = 1,
    MOTION_HOLDING = 2,
};

// 位置来源
enum PositionSource {
    POSITION_SOURCE_NONE      = 0,
    POSITION_SOURCE_APRILTAG  = 1,
    POSITION_SOURCE_ODOMETRY  = 2,
};

// 故障级别
enum FaultLevel {
    FAULT_WARNING      = 0,  // 可继续或降级运行
    FAULT_STOP_REQUIRED = 1, // 需要停车
    FAULT_FATAL        = 2,  // 致命
};

// 用户确认动作（一次只确认一个订单）
enum UserActionType {
    USER_ACTION_NONE                 = 0,
    USER_ACTION_CONFIRM_PICKUP_LOADED = 1,  // 2 -> 3
    USER_ACTION_CONFIRM_DROPOFF_TAKEN = 2,  // 4 -> 5
};

// 停靠类型
enum StopType {
    STOP_TYPE_UNKNOWN = 0,
    STOP_TYPE_PICKUP  = 1,
    STOP_TYPE_DROPOFF = 2,
};

// 错误码（任务书第十六节清单）
enum DeliveryErrorCode {
    ERR_NONE                   = 0,
    ERR_INVALID_PROTOCOL       = 1,   // 协议版本/字段/未知type非法
    ERR_AUTH_FAILED            = 2,
    ERR_WRONG_VEHICLE          = 3,
    ERR_NOT_SYNCHRONIZED       = 4,
    ERR_STALE_COMMAND          = 5,
    ERR_VERSION_CONFLICT       = 6,
    ERR_BUSY_TARGET_FROZEN     = 7,
    ERR_INVALID_MAP            = 8,
    ERR_MAP_VERSION_MISMATCH   = 9,
    ERR_INVALID_TARGET         = 10,
    ERR_POSITION_UNKNOWN       = 11,
    ERR_UNREACHABLE_TARGET     = 12,
    ERR_INVALID_CAPACITY_STATE = 13,
    ERR_CAMERA_INIT_FAILED     = 14,
    ERR_NAVIGATION_FAILED      = 15,
    ERR_STOP_TIMEOUT           = 16,
    ERR_SCREEN_DATA_INVALID    = 17,
    ERR_INTERNAL_ERROR         = 18,
};

// 错误码转字符串（用于JSON error.code字段）
const char* Delivery_Error_Name(DeliveryErrorCode code);

//================================================================================
// 公共头（所有消息必含）
//================================================================================
struct CommonHeader {
    int         protocol_version;
    std::string type;        // 小写下划线消息类型
    std::string message_id;  // 全局唯一，去重键
    int         vehicle_id;
    std::string sent_at;     // UTC ISO-8601，不用于本地超时判断

    CommonHeader() : protocol_version(kProtocolVersion), vehicle_id(kVehicleId) {}
};

//================================================================================
// 服务器 -> 小车 消息体
//================================================================================

struct ErrorInfo {
    bool        has_error;
    int         code;            // DeliveryErrorCode
    std::string message;

    ErrorInfo() : has_error(false), code(ERR_NONE) {}
};

struct HelloAck {
    bool        accepted;
    std::string session_id;
    std::string server_epoch;
    uint64_t    heartbeat_interval_ms;   // 默认2000
    uint64_t    connection_timeout_ms;   // 默认6000
    uint64_t    latest_snapshot_version;
    uint64_t    latest_command_version;
    std::string server_time;
    ErrorInfo   error;

    HelloAck() : accepted(false), heartbeat_interval_ms(2000),
                 connection_timeout_ms(6000), latest_snapshot_version(0),
                 latest_command_version(0) {}
};

struct HeartbeatAck {
    std::string server_epoch;
    uint64_t    snapshot_version;
    uint64_t    latest_command_version;

    HeartbeatAck() : snapshot_version(0), latest_command_version(0) {}
};

// 订单显示条目
struct OrderInfo {
    std::string order_id;
    std::string display_no;
    std::string nickname;
    int         status;            // 1..5
    std::string status_code;       // 如 "DELIVERING"
    std::string status_text;       // 如 "配送中"
    int         dispatch_state;    // DispatchState
    uint64_t    order_version;
    std::string pickup_location_id;
    std::string pickup_name;
    std::string dropoff_location_id;
    std::string dropoff_name;
    std::string item_summary;
    bool        has_button;
    std::string button_label;
    std::string button_action;
    bool        button_enabled;

    OrderInfo() : status(0), dispatch_state(DISPATCH_UNASSIGNED),
                  order_version(0), has_button(false), button_enabled(false) {}
};

struct ActiveTrip {
    bool        present;           // JSON null -> false
    std::string trip_id;
    std::string current_stop_id;
    int         loaded_count;

    ActiveTrip() : present(false), loaded_count(0) {}
};

struct AuthoritativeTarget {
    bool        present;
    std::string trip_id;
    std::string stop_id;
    int         map_id;
    int         target_node;
    uint64_t    command_version;

    AuthoritativeTarget() : present(false), map_id(0), target_node(0), command_version(0) {}
};

struct StateSync {
    std::string server_epoch;
    uint64_t    snapshot_version;
    uint64_t    latest_command_version;
    int         screen_phase;              // 0..5
    bool        has_current_order;
    std::string current_order_id;
    int         orders_total;
    int         orders_included;
    bool        orders_truncated;
    std::vector<OrderInfo> orders;
    ActiveTrip  active_trip;
    AuthoritativeTarget authoritative_target;

    StateSync() : snapshot_version(0), latest_command_version(0), screen_phase(0),
                  has_current_order(false), orders_total(0), orders_included(0),
                  orders_truncated(false) {}
};

struct GotoStopOp {
    std::string order_id;
    std::string action;            // "PICKUP"/"DROPOFF"
    uint64_t    order_version;
};

struct GotoStopCommand {
    uint64_t    command_version;
    std::string trip_id;
    std::string stop_id;
    int         map_id;
    int         required_map_version;
    std::string required_map_checksum;
    int         target_node;
    std::string location_id;
    std::string location_name;
    int         stop_type;         // StopType
    std::vector<GotoStopOp> operations;

    GotoStopCommand() : command_version(0), map_id(0), required_map_version(0),
                        target_node(0), stop_type(STOP_TYPE_UNKNOWN) {}
};

struct HoldCommand {
    uint64_t    command_version;
    std::string reason;
    std::string trip_id;
    std::string stop_id;

    HoldCommand() : command_version(0) {}
};

struct EmergencyStopCommand {
    uint64_t    command_version;
    std::string reason;
    std::string trip_id;
    std::string stop_id;

    EmergencyStopCommand() : command_version(0) {}
};

struct ResumeCommand {
    uint64_t    command_version;
    std::string trip_id;
    std::string stop_id;
    uint64_t    resume_target_command_version;
    std::string reason;

    ResumeCommand() : command_version(0), resume_target_command_version(0) {}
};

struct EventAck {
    bool        accepted;
    std::string reply_to;
    std::string event_type;        // "arrived"/"user_action"
    bool        has_order;
    std::string order_id;
    bool        has_new_status;
    int         new_status;        // 1..5
    bool        has_new_order_version;
    uint64_t    new_order_version;
    uint64_t    snapshot_version;
    ErrorInfo   error;

    EventAck() : accepted(false), has_order(false), has_new_status(false),
                 new_status(0), has_new_order_version(false),
                 new_order_version(0), snapshot_version(0) {}
};

// 服务器消息变体（解析后统一承载）
enum ServerMsgType {
    SRV_MSG_UNKNOWN = 0,
    SRV_MSG_HELLO_ACK,
    SRV_MSG_HEARTBEAT_ACK,
    SRV_MSG_STATE_SYNC,
    SRV_MSG_GOTO_STOP,
    SRV_MSG_HOLD,
    SRV_MSG_EMERGENCY_STOP,
    SRV_MSG_RESUME,
    SRV_MSG_EVENT_ACK,
};

struct ServerMessage {
    ServerMsgType type;
    CommonHeader  header;

    HelloAck             hello_ack;
    HeartbeatAck         heartbeat_ack;
    StateSync            state_sync;
    GotoStopCommand      goto_stop;
    HoldCommand          hold;
    EmergencyStopCommand emergency_stop;
    ResumeCommand        resume;
    EventAck             event_ack;

    ServerMessage() : type(SRV_MSG_UNKNOWN) {}
};

//================================================================================
// 小车 -> 服务器 消息体
//================================================================================

struct Hello {
    std::string device_token;          // 不写日志
    std::string boot_id;
    std::string software_version;
    std::string last_server_epoch;
    uint64_t    last_snapshot_version;
    uint64_t    last_command_version;
    int         map_id;
    int         map_version;
    std::string map_checksum;
    int         current_node;          // -1=未知
    bool        position_valid;
    int         motion_state;          // MotionState
    bool        emergency_latched;
    std::vector<std::string> pending_event_ids;

    Hello() : last_snapshot_version(0), last_command_version(0),
              map_id(0), map_version(0), current_node(-1),
              position_valid(false), motion_state(MOTION_STOPPED),
              emergency_latched(false) {}
};

struct Heartbeat {
    std::string boot_id;
    uint64_t    sequence;          // 同一次启动内递增
    uint64_t    uptime_ms;
    int         map_id;
    int         current_node;      // -1=未知
    int         last_tag_id;       // -1=无
    std::string last_tag_at;
    uint32_t    mile_since_node;
    int         position_source;   // PositionSource
    int         motion_state;
    std::string navigation_state;  // 导航状态名
    std::string current_action;    // 动作名
    bool        has_trip;
    std::string trip_id;
    bool        has_stop;
    std::string stop_id;
    uint64_t    command_version;
    bool        has_battery;
    int         battery_percent;
    bool        has_fault_code;
    int         fault_code;

    Heartbeat() : sequence(0), uptime_ms(0), map_id(0), current_node(-1),
                  last_tag_id(-1), mile_since_node(0),
                  position_source(POSITION_SOURCE_NONE),
                  motion_state(MOTION_STOPPED), has_trip(false), has_stop(false),
                  command_version(0), has_battery(false), battery_percent(0),
                  has_fault_code(false), fault_code(0) {}
};

struct SyncAck {
    std::string reply_to;
    std::string server_epoch;
    uint64_t    snapshot_version;
    bool        accepted;
    ErrorInfo   error;

    SyncAck() : snapshot_version(0), accepted(true) {}
};

struct CommandAck {
    std::string reply_to;
    bool        accepted;
    uint64_t    command_version;
    std::string execution_state;   // V1: "NAVIGATING"/"STOPPED"
    std::vector<int> path;         // 本地Dijkstra实际路径
    ErrorInfo   error;

    CommandAck() : accepted(false), command_version(0) {}
};

struct NavigationStatusReport {
    bool        has_trip;
    std::string trip_id;
    bool        has_stop;
    std::string stop_id;
    uint64_t    command_version;
    std::string navigation_state;
    int         motion_state;
    int         map_id;
    int         current_node;
    int         prev_node;
    int         next_node;
    std::vector<int> actual_path;
    int         path_index;
    int         target_node;
    bool        position_valid;
    bool        has_fault_code;
    int         fault_code;

    NavigationStatusReport() : command_version(0),
                               motion_state(MOTION_STOPPED), map_id(0),
                               current_node(-1), prev_node(-1), next_node(-1),
                               path_index(0), target_node(0), position_valid(false),
                               has_fault_code(false), fault_code(0) {}
};

struct ArrivedEvent {
    std::string trip_id;
    std::string stop_id;
    uint64_t    command_version;
    int         map_id;
    int         node_id;
    int         tag_id;
    int         motion_state;       // 发送前必须 STOPPED
    int         position_source;

    ArrivedEvent() : command_version(0), map_id(0), node_id(0), tag_id(0),
                     motion_state(MOTION_STOPPED),
                     position_source(POSITION_SOURCE_APRILTAG) {}
};

struct UserActionEvent {
    std::string trip_id;
    std::string stop_id;
    std::string order_id;
    int         action;             // UserActionType
    int         expected_status;    // 2或4
    uint64_t    order_version;
    uint64_t    display_revision;   // 屏幕点击时的快照版本

    UserActionEvent() : action(USER_ACTION_NONE), expected_status(0),
                        order_version(0), display_revision(0) {}
};

struct FaultReport {
    int         fault_code;         // DeliveryErrorCode
    int         fault_level;        // FaultLevel
    std::string fault_message;
    int         motion_state;
    bool        has_trip;
    std::string trip_id;
    bool        has_stop;
    std::string stop_id;
    uint64_t    command_version;
    int         map_id;
    int         current_node;

    FaultReport() : fault_code(ERR_NONE), fault_level(FAULT_WARNING),
                    motion_state(MOTION_STOPPED), has_trip(false), has_stop(false),
                    command_version(0), map_id(0), current_node(-1) {}
};

//================================================================================
// 命令版本守卫（两层去重：command_version + 内容哈希）
//================================================================================
class CommandVersionGuard {
public:
    enum Verdict {
        ACCEPT_NEW = 0,   // 版本更高，进入正常验证流程
        REPLAY_SAME,      // 版本相同且内容哈希相同：幂等重放，返回缓存ACK
        STALE,            // 版本更低：STALE_COMMAND
        CONFLICT,         // 版本相同但内容哈希不同：VERSION_CONFLICT，停车上报
    };

    // 比较但不记录；接受并持久化后再调用Record
    Verdict Check(uint64_t version, uint64_t content_hash) const;

    // 记录已接受的命令（同时由delivery_store持久化）
    void Record(uint64_t version, uint64_t content_hash, const std::string& cached_ack_json);

    uint64_t        last_version() const { return last_version_; }
    uint64_t        last_hash() const { return last_hash_; }
    const std::string& cached_ack() const { return cached_ack_; }
    bool            has_accepted() const { return has_accepted_; }

private:
    bool        has_accepted_ = false;
    uint64_t    last_version_ = 0;
    uint64_t    last_hash_ = 0;
    std::string cached_ack_;
};

#endif /* __DELIVERY_TYPES_HPP */
