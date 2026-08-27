/**
 * @file delivery_controller.hpp
 * @brief 中央配送——配送协调器（外层状态机：服务器命令与导航之间的唯一桥梁）
 * @details 职责（任务书十/222.md阶段5）：
 *   - 连接/认证/同步流程（hello→hello_ack→state_sync→sync_ack→READY）
 *   - goto_stop 十四步命令管道（验证→规划→持久化→ACK→最后才开运动许可）
 *   - hold/emergency_stop/resume 处理
 *   - 到站观察：锁存消费+停稳判定+arrived可靠事件
 *   - 心跳、事件重发、本地可靠存储
 *   state_sync只更新订单缓存，绝不直接启动运动（Kevin 222.md第二节1）
 */

#ifndef __DELIVERY_CONTROLLER_HPP
#define __DELIVERY_CONTROLLER_HPP

#include "include.hpp"
#include "car_gateway.hpp"
#include "delivery_store.hpp"

/*============================================================================
 *                              协调状态机
 *============================================================================*/
enum DeliveryState {
    DELIVERY_OFF = 0,        // 配送模式关闭（本地模式：屏幕START自主导航）
    DELIVERY_BOOT,           // 上电停车
    DELIVERY_WAIT_CONN,      // 等待/重建TCP连接
    DELIVERY_WAIT_SYNC,      // 已认证，等待首个state_sync
    DELIVERY_READY,          // 已同步，停车待命
    DELIVERY_NAVIGATING,     // 执行goto_stop分段导航
    DELIVERY_ARRIVED_WAIT,   // 已到站上报，等待服务器下一命令
    DELIVERY_HOLDING,        // hold暂停（保留目标）
    DELIVERY_EMERGENCY,      // 急停锁存（仅resume可解除）
    DELIVERY_FAULT,          // 致命故障（VERSION_CONFLICT等，需人工/服务器介入）
};

const char* Delivery_State_Name(DeliveryState s);

/*============================================================================
 *                              配送协调器
 *============================================================================*/
class DeliveryController {
public:
    struct Config {
        bool        enabled      = false;                 // 配送模式开关
        std::string host         = "127.0.0.1";
        uint16_t    port         = 8898;
        std::string device_token = "change-me";
        int         map_id       = 1;
        int         map_version  = 3;
        std::string map_checksum = "sha256:local";
        std::string store_path   = "/home/root/delivery_state.json";
        std::string config_path  = "/home/root/delivery_config.txt";
    };

    // 初始化（主线程）：读配置文件 -> 加载本地存储 -> 配置网关
    void Init(void);
    // 启动网关线程（All_Init后调用）
    void Start(void);
    void Stop(void);

    // 主循环每帧调用：消费服务器消息+推进状态机+到站观察+心跳+事件重发
    void Process(void);

    // 运动许可（main.cpp据此发布motion_permitted；配送模式下唯一运动来源）
    bool Motion_Permitted(void) const;
    DeliveryState State(void) const { return state_; }
    bool Enabled(void) const { return cfg_.enabled; }

    // 最新订单快照（屏幕渲染用，阶段6接入）
    const StateSync& Snapshot(void) const { return snap_; }
    uint64_t Current_Command_Version(void) const { return store_.command_version; }

private:
    void Handle_Message(const ServerMessage& m);
    void Handle_Hello_Ack(const ServerMessage& m);
    void Handle_State_Sync(const ServerMessage& m);
    void Handle_Goto_Stop(const ServerMessage& m);
    void Handle_Hold(const ServerMessage& m);
    void Handle_Emergency(const ServerMessage& m);
    void Handle_Resume(const ServerMessage& m);
    void Handle_Event_Ack(const ServerMessage& m);
    void Handle_Link_Change(void);
    void Observe_Arrival(void);
    void Resend_Pending_Events(void);

    void Send_Hello(void);
    void Send_Heartbeat(void);
    void Send_Command_Ack(const std::string& reply_to, bool accepted,
                          uint64_t version, const std::vector<int>& path,
                          DeliveryErrorCode err, const std::string& err_msg);
    void Send_Fault(DeliveryErrorCode code, int level, const std::string& msg);
    std::string Next_Message_Id(const char* prefix);
    void Store_Save(void);

    bool Load_Config(const std::string& path);

    Config          cfg_;
    CarGateway*     gw_ = nullptr;
    DeliveryState   state_ = DELIVERY_OFF;
    CommandVersionGuard guard_;
    DeliveryStoreData store_;
    StateSync       snap_;                 // 最新权威快照
    bool            synced_ = false;
    std::string     server_epoch_;
    uint64_t        hb_interval_ms_ = 2000;

    // 当前执行段
    std::string     cur_trip_;
    std::string     cur_stop_;
    uint64_t        cur_cmd_ver_ = 0;

    // 心跳与计时
    uint64_t        hb_seq_ = 0;
    uint64_t        msg_seq_ = 0;
    uint32_t        last_hb_ms_ = 0;
    uint32_t        last_resend_ms_ = 0;
    std::string     boot_id_;

    // 到站流程状态
    bool            pending_arrival_ = false;   // 已消费到站锁存，等停稳
    bool            arrival_reported_ = false;  // arrived事件已入待确认表
    uint32_t        arrival_wait_start_ = 0;
    int             arrival_map_ = 0;
    int             arrival_node_ = 0;

    bool            last_link_up_ = false;
};

// 全局单例（工程风格）
extern DeliveryController delivery;

#endif /* __DELIVERY_CONTROLLER_HPP */
