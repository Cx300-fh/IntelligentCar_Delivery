/**
 * @file delivery_controller.cpp
 * @brief 中央配送——配送协调器 实现
 * @details 主线程内运行（Process由主循环调用），网络解析已在网关线程完成。
 *          goto_stop处理遵循Kevin 222.md第三节十四步顺序：
 *          验证和规划在前，持久化在ACK前，运动许可在ACK之后。
 */

#include "delivery_controller.hpp"

#include <cstdio>
#include <cstring>

// 全局单例
DeliveryController delivery;

const char* Delivery_State_Name(DeliveryState s)
{
    switch (s) {
        case DELIVERY_OFF:         return "OFF";
        case DELIVERY_BOOT:        return "BOOT";
        case DELIVERY_WAIT_CONN:   return "WAIT_CONN";
        case DELIVERY_WAIT_SYNC:   return "WAIT_SYNC";
        case DELIVERY_READY:       return "READY";
        case DELIVERY_NAVIGATING:  return "NAVIGATING";
        case DELIVERY_ARRIVED_WAIT:return "ARRIVED_WAIT";
        case DELIVERY_HOLDING:     return "HOLDING";
        case DELIVERY_EMERGENCY:   return "EMERGENCY";
        case DELIVERY_FAULT:       return "FAULT";
    }
    return "?";
}

/*============================================================================
 *                              初始化
 *============================================================================*/

// 简单key=value配置解析（#注释行忽略）
bool DeliveryController::Load_Config(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "r");
    if (f == NULL) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n'); if (nl) *nl = 0;
        char* cr = strchr(line, '\r'); if (cr) *cr = 0;
        if (line[0] == '#' || line[0] == 0) continue;
        char* eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = 0;
        std::string key = line;
        std::string val = eq + 1;

        if      (key == "enabled")     cfg_.enabled = (val == "1" || val == "true");
        else if (key == "host")        cfg_.host = val;
        else if (key == "port")        cfg_.port = (uint16_t)atoi(val.c_str());
        else if (key == "device_token") cfg_.device_token = val;
        else if (key == "map_id")      cfg_.map_id = atoi(val.c_str());
        else if (key == "map_version") cfg_.map_version = atoi(val.c_str());
        else if (key == "map_checksum") cfg_.map_checksum = val;
    }
    fclose(f);
    return true;
}

void DeliveryController::Init(void)
{
    // 1. 配置文件（不存在=本地模式）
    if (!Load_Config(cfg_.config_path)) {
        printf("[DLV] 无%s，配送模式关闭（本地模式）\n", cfg_.config_path.c_str());
        cfg_.enabled = false;
        state_ = DELIVERY_OFF;
        return;
    }

    if (!cfg_.enabled) {
        printf("[DLV] delivery_config.txt enabled!=1，本地模式\n");
        state_ = DELIVERY_OFF;
        return;
    }

    // 2. 本地可靠存储
    if (Delivery_Store_Load(cfg_.store_path, &store_)) {
        printf("[DLV] 恢复本地存储：epoch=%s snapshot=%llu cmd_ver=%llu 急停锁=%d 待确认事件=%zu\n",
               store_.server_epoch.c_str(),
               (unsigned long long)store_.snapshot_version,
               (unsigned long long)store_.command_version,
               (int)store_.emergency_latched,
               store_.pending_events.size());
    } else {
        printf("[DLV] 无本地存储（首次运行或损坏），从零开始\n");
    }

    // 3. 上电安全：急停锁存恢复（上次急停未解除则保持禁止）
    if (store_.emergency_latched) {
        Safety_Inhibit_Set(INHIBIT_REASON_EMERGENCY);
        printf("[DLV] 恢复上次急停锁存，等待resume\n");
    }

    // 4. boot_id（每次进程启动生成）
    char bid[48];
    snprintf(bid, sizeof(bid), "boot-%u", (unsigned)lq_get_tick_ms());
    boot_id_ = bid;

    // 5. 导航状态机恢复最后已知位置
    if (store_.last_current_node > 0) {
        // 位置交给Tag_Scan/nav自然确认；这里仅记录（nav重新识别首个Tag后has_prev建立）
        printf("[DLV] 上次最后位置：节点%d（以摄像头实测为准）\n",
               store_.last_current_node);
    }

    // 6. 网关
    CarGateway::Config gcfg;
    gcfg.host = cfg_.host;
    gcfg.port = cfg_.port;
    gcfg.connection_timeout_ms = 6000;
    gw_ = new CarGateway(gcfg);
    // 掉线回调：网络线程 -> 安全禁止位（唯一允许的跨线程安全动作）
    gw_->Set_Link_Loss_Callback([]() {
        Safety_Inhibit_Set(INHIBIT_REASON_LINK_LOSS);
    });

    state_ = DELIVERY_WAIT_CONN;
    // 原地定位：停车扫描Tag确认当前位置（等服务器期间位置就绪，goto免被拒）
    nav_fsm.begin_localization(cfg_.map_id);
    printf("[DLV] 配送模式启动：服务器%s:%u 地图v%d\n",
           cfg_.host.c_str(), (unsigned)cfg_.port, cfg_.map_version);
}

void DeliveryController::Start(void)
{
    if (gw_ != NULL) gw_->Start();
}

void DeliveryController::Stop(void)
{
    if (gw_ != NULL) gw_->Stop();
}

/*============================================================================
 *                              主循环入口
 *============================================================================*/

void DeliveryController::Process(void)
{
    if (!cfg_.enabled || gw_ == NULL) return;

    uint32_t now = lq_get_tick_ms();

    // 1. 消费网关已解析的服务器消息
    ServerMessage m;
    while (gw_->Poll_Server_Message(&m)) {
        Handle_Message(m);
    }

    // 1.5 消费屏幕事件（DConfirm页确认按钮；串口屏线程已入队）
    Process_Screen_Events();

    // 2. 链路状态变化（建立/断开）
    Handle_Link_Change();

    // 3. 到站观察（锁存消费+停稳+arrived可靠事件）
    Observe_Arrival();

    // 4. 心跳（链路已建立后按服务器参数周期发送）
    if (gw_->Is_Link_Up() && now - last_hb_ms_ >= hb_interval_ms_) {
        last_hb_ms_ = now;
        Send_Heartbeat();
    }

    // 5. 未ACK事件重发（1s周期）
    if (gw_->Is_Link_Up() && now - last_resend_ms_ >= 1000) {
        last_resend_ms_ = now;
        Resend_Pending_Events();
    }

    // 6. 断线/手动安全禁止位的受控清除：
    //    READY+已停稳时清除LINK_LOSS和MANUAL位（清除许可≠允许运动——运动由
    //    协调器状态门控，清除后车不会自行恢复行驶）；EMERGENCY位仅resume可清
    //    LINK_LOSS 的清除不能绑定 DELIVERY_READY：断线重连后服务器往往立刻重发
    //    goto_stop，车端在下一个清除周期到来前就从 READY 转入 NAVIGATING，此后
    //    state_ 再也不等于 READY，该位永久卡住。表现极具迷惑性——车接受命令、
    //    导航授权 motion_permitted=1、心跳报 MOVING/FOLLOW、fault_code 为空，
    //    但 control 的 drive 门控被 inhibit 压住，轮子一动不动且不报任何错。
    //    这个位的成因是"链路断开"，链路恢复后成因即消失，与协调器处于哪个状态无关。
    //    清除禁止位不等于允许运动：运动仍由 motion_permitted 门控（断线时已
    //    pause_task，重连后要等服务器新命令，车不会自行恢复行驶）。
    if (gw_->Is_Link_Up() && Control_Is_Stopped() &&
        (Safety_Inhibit_Reason() & INHIBIT_REASON_LINK_LOSS)) {
        Safety_Inhibit_Clear_Bits(INHIBIT_REASON_LINK_LOSS);
        printf("[DLV] 链路恢复+停稳，清除LINK_LOSS安全禁止\n");
    }
    if (state_ == DELIVERY_READY && Control_Is_Stopped()) {
        uint32_t reasons = Safety_Inhibit_Reason();
        if (reasons & (INHIBIT_REASON_LINK_LOSS | INHIBIT_REASON_MANUAL)) {
            Safety_Inhibit_Clear_Bits(INHIBIT_REASON_LINK_LOSS | INHIBIT_REASON_MANUAL);
            printf("[DLV] READY+停稳，清除LINK_LOSS/MANUAL安全禁止\n");
        }
    }
}

bool DeliveryController::Motion_Permitted(void) const
{
    if (!cfg_.enabled) return false;
    // 只有NAVIGATING状态允许运动；HOLDING/EMERGENCY/FAULT/WAIT_*全部停车。
    // 最终安全门仍在5ms线程（inhibit/看门狗），这里是业务层许可。
    return state_ == DELIVERY_NAVIGATING;
}

/*============================================================================
 *                              消息分发
 *============================================================================*/

void DeliveryController::Handle_Message(const ServerMessage& m)
{
    switch (m.type) {
        case SRV_MSG_HELLO_ACK:      Handle_Hello_Ack(m);      break;
        case SRV_MSG_HEARTBEAT_ACK:  /* 保活，无业务动作 */     break;
        case SRV_MSG_STATE_SYNC:     Handle_State_Sync(m);     break;
        case SRV_MSG_GOTO_STOP:      Handle_Goto_Stop(m);      break;
        case SRV_MSG_HOLD:           Handle_Hold(m);           break;
        case SRV_MSG_EMERGENCY_STOP: Handle_Emergency(m);      break;
        case SRV_MSG_RESUME:         Handle_Resume(m);         break;
        case SRV_MSG_EVENT_ACK:      Handle_Event_Ack(m);      break;
        default: break;   // 未知类型已在解析层拒绝
    }
}

/*============================================================================
 *                              连接与同步流程
 *============================================================================*/

void DeliveryController::Handle_Link_Change(void)
{
    bool up = gw_->Is_Link_Up();
    if (up == last_link_up_) return;
    last_link_up_ = up;

    if (up) {
        // 连接建立：重置会话状态并发hello（保持停车）
        synced_ = false;
        if (state_ == DELIVERY_BOOT || state_ == DELIVERY_WAIT_CONN) {
            state_ = DELIVERY_WAIT_CONN;   // 等hello_ack
        }
        Send_Hello();
    } else {
        // 断线：inhibit已由网关回调置位；导航任务暂停保留（重连后等服务器命令，不自行恢复运动）
        synced_ = false;
        printf("[DLV] 链路断开，停车等待重连（目标保留：%s）\n",
               cur_stop_.empty() ? "无" : cur_stop_.c_str());
        if (state_ == DELIVERY_NAVIGATING) nav_fsm.pause_task();
        if (state_ != DELIVERY_EMERGENCY && state_ != DELIVERY_FAULT) {
            state_ = DELIVERY_WAIT_CONN;
        }
    }
}

void DeliveryController::Handle_Hello_Ack(const ServerMessage& m)
{
    const HelloAck& a = m.hello_ack;
    if (!a.accepted) {
        printf("[DLV] 认证失败：%s\n", a.error.message.c_str());
        Send_Fault(ERR_AUTH_FAILED, FAULT_STOP_REQUIRED, "auth rejected");
        gw_->Notify_Auth_Result(false);   // 断开，按退避重连
        return;
    }
    gw_->Notify_Auth_Result(true);
    server_epoch_ = a.server_epoch;
    if (a.heartbeat_interval_ms >= 500) hb_interval_ms_ = a.heartbeat_interval_ms;
    state_ = DELIVERY_WAIT_SYNC;          // 等首个state_sync
    printf("[DLV] 认证成功：epoch=%s 心跳=%ums\n",
           server_epoch_.c_str(), (unsigned)hb_interval_ms_);
}

void DeliveryController::Handle_State_Sync(const ServerMessage& m)
{
    const StateSync& s = m.state_sync;

    // epoch换代：接受新权威快照，重置版本比较
    // 同epoch旧快照：忽略（重复投递）
    if (s.server_epoch == server_epoch_ && synced_) {
        if (s.snapshot_version <= snap_.snapshot_version) {
            printf("[DLV] 忽略旧快照 v%llu（本地v%llu）\n",
                   (unsigned long long)s.snapshot_version,
                   (unsigned long long)snap_.snapshot_version);
            return;
        }
    }

    // 容量校验（status 3/4 合计<=5，违反则拒绝快照保持停车）
    ParseResult cap = Delivery_Check_Capacity(s.orders);
    if (!cap.ok) {
        printf("[DLV] 快照容量非法，拒绝应用：%s\n", cap.message.c_str());
        SyncAck ack;
        ack.reply_to = m.header.message_id;
        ack.server_epoch = s.server_epoch;
        ack.snapshot_version = s.snapshot_version;
        ack.accepted = false;
        ack.error.has_error = true;
        ack.error.code = ERR_INVALID_CAPACITY_STATE;
        ack.error.message = cap.message;
        CommonHeader h; h.type = "sync_ack";
        h.message_id = Next_Message_Id("ack-sync");
        h.sent_at = Delivery_Format_Utc_Now();
        gw_->Send_Line(Delivery_Serialize_SyncAck(ack, h));
        Send_Fault(ERR_INVALID_CAPACITY_STATE, FAULT_STOP_REQUIRED, cap.message);
        return;
    }

    // 应用快照（原子替换缓存；不触碰导航状态——绝不自动恢复运动）
    server_epoch_ = s.server_epoch;
    snap_ = s;
    synced_ = true;

    // 重启恢复 stop 上下文：等确认期间后端不再下发 goto_stop，cur_trip_/cur_stop_
    // 无从赋值（重启后为空串，user_action 会被后端 ACTION_STOP_CONFLICT 拒绝）。
    // 从权威快照恢复上报上下文；不触发任何运动。active_trip.current_stop_id 由
    // 后端从 frozen_stop_id 映射（无冻结 stop 时为空串则只恢复 trip_id）。
    if (s.active_trip.present && !s.active_trip.trip_id.empty()) {
        cur_trip_ = s.active_trip.trip_id;
        if (!s.active_trip.current_stop_id.empty()) {
            cur_stop_ = s.active_trip.current_stop_id;
        }
    }

    // 持久化版本
    store_.server_epoch = s.server_epoch;
    store_.snapshot_version = s.snapshot_version;
    Store_Save();

    // sync_ack
    SyncAck ack;
    ack.reply_to = m.header.message_id;
    ack.server_epoch = s.server_epoch;
    ack.snapshot_version = s.snapshot_version;
    ack.accepted = true;
    CommonHeader h; h.type = "sync_ack";
    h.message_id = Next_Message_Id("ack-sync");
    h.sent_at = Delivery_Format_Utc_Now();
    gw_->Send_Line(Delivery_Serialize_SyncAck(ack, h));

    // 状态迁移：仅"等待同步"状态进入READY；导航/到站等待状态保持不变
    if (state_ == DELIVERY_WAIT_SYNC || state_ == DELIVERY_WAIT_CONN) {
        state_ = DELIVERY_READY;
        printf("[DLV] 同步完成：phase=%d 订单%zu条 → READY\n",
               snap_.screen_phase, snap_.orders.size());
    } else {
        printf("[DLV] 快照更新：v%llu phase=%d（状态保持%s）\n",
               (unsigned long long)s.snapshot_version, snap_.screen_phase,
               Delivery_State_Name(state_));
    }
}

/*============================================================================
 *                              goto_stop十四步管道
 *============================================================================*/

void DeliveryController::Handle_Goto_Stop(const ServerMessage& m)
{
    const GotoStopCommand& c = m.goto_stop;

    // ---- 步骤5：命令版本与幂等（两层去重）----
    uint64_t hash = Delivery_Canonical_Command_Hash(c);
    CommandVersionGuard::Verdict v = guard_.Check(c.command_version, hash);
    if (v == CommandVersionGuard::STALE) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_STALE_COMMAND, "older command_version");
        return;
    }
    if (v == CommandVersionGuard::REPLAY_SAME) {
        // 幂等重放：返回之前的ACK，不重启导航
        printf("[DLV] goto_stop幂等重放 v%llu，返回缓存ACK\n",
               (unsigned long long)c.command_version);
        gw_->Send_Line(guard_.cached_ack());
        return;
    }
    if (v == CommandVersionGuard::CONFLICT) {
        // 同版本不同内容：严重协议错误，停车上报
        printf("[DLV] VERSION_CONFLICT v%llu！停车上报\n",
               (unsigned long long)c.command_version);
        nav_fsm.pause_task();
        state_ = DELIVERY_FAULT;
        Safety_Inhibit_Set(INHIBIT_REASON_MANUAL);   // 停车保持，需人工介入
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_VERSION_CONFLICT,
                         "same version, different content");
        Send_Fault(ERR_VERSION_CONFLICT, FAULT_FATAL, "command version conflict");
        return;
    }

    // ---- 步骤4/6：会话与同步状态 ----
    if (!synced_) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_NOT_SYNCHRONIZED, "not synchronized");
        return;
    }

    // ---- 步骤6：地图校验 ----
    if (c.map_id != cfg_.map_id) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_INVALID_MAP, "wrong map_id");
        return;
    }
    // required_map_version 在协议里是可选字段（delivery_protocol.cpp 按 required=false
    // 解析），缺失时保持结构体默认值 0。0 表示服务器没有提出地图版本要求，此时必须
    // 跳过校验：否则任何不带该字段的合法 goto_stop 都会被 MAP_VERSION_MISMATCH 拒收，
    // 车端收到命令却原地不动（真后端就不发这个字段）。服务器明确给出版本时照常校验。
    if (c.required_map_version != 0 && c.required_map_version != cfg_.map_version) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_MAP_VERSION_MISMATCH,
                         "map version mismatch");
        return;
    }

    // ---- 步骤6：目标节点有效性 ----
    if (c.target_node <= 0) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_INVALID_TARGET, "bad target_node");
        return;
    }

    // ---- 步骤6：当前位置必须已知（V1禁止盲走）----
    const NavStatus& ns = nav_fsm.get_status();
    if (ns.current_id <= 0) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_POSITION_UNKNOWN,
                         "current position unknown");
        return;
    }

    // ---- 步骤8：本地规划（start_leg：验证+安装，运动许可仍关闭）----
    if (!nav_fsm.start_leg(c.map_id, c.target_node)) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_UNREACHABLE_TARGET,
                         "local path planning failed");
        return;
    }

    // ---- 步骤10：先持久化（接受前落盘）----
    cur_trip_ = c.trip_id;
    cur_stop_ = c.stop_id;
    cur_cmd_ver_ = c.command_version;
    pending_arrival_ = false;
    arrival_reported_ = false;
    store_.command_version = c.command_version;
    store_.command_hash = hash;
    store_.command_json = m.header.type;   // 简化：版本+哈希已足够恢复一致性校验
    // 这里不得清 emergency_latched：急停是安全锁存，设计上"EMERGENCY位仅resume可清"。
    // 原来在接受 goto_stop 时把标志清成 false，却没有同时清 INHIBIT_REASON_EMERGENCY
    // 禁止位，于是状态自相矛盾——禁止位还压着运动，而 Handle_Resume 检查
    // !emergency_latched 后判定"nothing to resume"直接拒收。结果是急停之后只要再来
    // 一条 goto_stop（断线重连后服务器必然重发），这台车就再也 resume 不回来了。
    Store_Save();

    // ---- 步骤12：生成accepted=true的ACK（先构造并缓存，用于幂等重放）----
    std::vector<int> path(ns.path, ns.path + ns.path_len);
    {
        CommandAck ack;
        ack.reply_to = m.header.message_id;
        ack.accepted = true;
        ack.command_version = c.command_version;
        ack.execution_state = "NAVIGATING";
        ack.path = path;
        CommonHeader h; h.type = "command_ack";
        h.message_id = Next_Message_Id("ack-cmd");
        h.sent_at = Delivery_Format_Utc_Now();
        std::string ack_json = Delivery_Serialize_CommandAck(ack, h);
        guard_.Record(c.command_version, hash, ack_json);
        gw_->Send_Line(ack_json);
    }

    // ---- 步骤13/14：ACK生成之后才允许运动 ----
    state_ = DELIVERY_NAVIGATING;
    printf("[DLV] goto_stop接受：v%llu 目标节点%d 路径%zu段 → NAVIGATING\n",
           (unsigned long long)c.command_version, c.target_node, path.size());
}

/*============================================================================
 *                              hold / emergency_stop / resume
 *============================================================================*/

void DeliveryController::Handle_Hold(const ServerMessage& m)
{
    const HoldCommand& c = m.hold;
    // 版本必须高于已接受版本（共用全局command_version序列）
    if (c.command_version <= store_.command_version) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_STALE_COMMAND, "stale hold");
        return;
    }
    nav_fsm.pause_task();
    state_ = DELIVERY_HOLDING;
    store_.command_version = c.command_version;
    Store_Save();
    Send_Command_Ack(m.header.message_id, true, c.command_version,
                     std::vector<int>(), ERR_NONE, "");
    printf("[DLV] hold v%llu → HOLDING\n", (unsigned long long)c.command_version);
}

void DeliveryController::Handle_Emergency(const ServerMessage& m)
{
    const EmergencyStopCommand& c = m.emergency_stop;
    if (c.command_version <= store_.command_version) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_STALE_COMMAND, "stale estop");
        return;
    }
    Safety_Inhibit_Set(INHIBIT_REASON_EMERGENCY);   // 立即快停（5ms线程执行）
    nav_fsm.pause_task();                           // 目标与订单保留
    state_ = DELIVERY_EMERGENCY;
    store_.command_version = c.command_version;
    store_.emergency_latched = true;                // 锁存持久化（重启也保持）
    Store_Save();
    Send_Command_Ack(m.header.message_id, true, c.command_version,
                     std::vector<int>(), ERR_NONE, "");
    printf("[DLV] emergency_stop v%llu → 锁存急停（仅resume可解）\n",
           (unsigned long long)c.command_version);
}

void DeliveryController::Handle_Resume(const ServerMessage& m)
{
    const ResumeCommand& c = m.resume;
    if (c.command_version <= store_.command_version) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_STALE_COMMAND, "stale resume");
        return;
    }
    // 恢复目标必须与本地持久化目标一致
    if (c.trip_id != cur_trip_ || c.stop_id != cur_stop_) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_VERSION_CONFLICT,
                         "resume target mismatch");
        Send_Fault(ERR_VERSION_CONFLICT, FAULT_STOP_REQUIRED, "resume target mismatch");
        return;
    }
    // 可恢复场景：急停锁存 / HOLDING暂停 / 导航中人工急停（MANUAL置位，state仍NAVIGATING）
    bool manual_stopped_nav = (Safety_Inhibit_Reason() & INHIBIT_REASON_MANUAL) &&
                              state_ == DELIVERY_NAVIGATING;
    if (!store_.emergency_latched && state_ != DELIVERY_HOLDING && !manual_stopped_nav) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_NOT_SYNCHRONIZED, "nothing to resume");
        return;
    }
    // 急停可能落在“已到站等待确认”上：nav 在 handle_arrived 里已把 task.active
    // 置为 false，resume_task() 第一行 (!task.active) 就返回 false。原实现据此拒收，
    // 而解除禁止位的代码在它之后——于是到站等确认时按一次急停，车就再也
    // 解不开：重启同样无效，emergency_latched 是持久化的，而重启后的 nav
    // 也没有活跃任务。这种场景本就不需要“恢复导航”，恢复的语义只是解除
    // 禁止位、退回到站等待。
    // 不能只看 is_navigating()：它读的是 task.active，而定位阶段也会把它置 true
    // （navigation.cpp“状态机需要激活才会扫描Tag”），于是车在原地定位时会被
    // 误判成“导航中”，resume 放行运动后车就跑了——实测从新清华学堂一路
    // 跑到照澜院。真正可恢复的只有在途状态；定位/到站/空闲都是“停着”，
    // 恢复的语义仅仅是解除禁止位。
    const NavState nav_state = nav_fsm.get_state();
    const bool nav_resumable = nav_fsm.is_navigating() &&
                               nav_state != NAV_STATE_LOCATING &&
                               nav_state != NAV_STATE_ARRIVED &&
                               nav_state != NAV_STATE_IDLE;
    if (nav_resumable && !nav_fsm.resume_task()) {
        Send_Command_Ack(m.header.message_id, false, c.command_version,
                         std::vector<int>(), ERR_NAVIGATION_FAILED, "nav resume failed");
        return;
    }
    // 解除急停与人工禁止（resume=服务器权威恢复指令；MANUAL在导航中置位后
    // 仅此路径可清——否则屏幕急停会死锁：车永停且无人能恢复）
    Safety_Inhibit_Clear_Bits(INHIBIT_REASON_EMERGENCY | INHIBIT_REASON_MANUAL);
    store_.emergency_latched = false;
    store_.command_version = c.command_version;
    Store_Save();
    // 无导航任务可恢复 = 车停在站上等确认，不能谎报 NAVIGATING
    // （前面已校验 trip_id/stop_id 与本地一致，所以必定是本 trip 的到站等待）
    state_ = nav_resumable ? DELIVERY_NAVIGATING : DELIVERY_ARRIVED_WAIT;   // 从零缓加速由5ms线程保证（current_speed已为0）
    Send_Command_Ack(m.header.message_id, true, c.command_version,
                     std::vector<int>(), ERR_NONE, "");
    printf("[DLV] resume v%llu → %s\n",
           (unsigned long long)c.command_version,
           nav_resumable ? "NAVIGATING（零速缓启）"
                         : "ARRIVED_WAIT（到站等确认，仅解除急停）");
}

/*============================================================================
 *                              事件确认
 *============================================================================*/

void DeliveryController::Handle_Event_Ack(const ServerMessage& m)
{
    const EventAck& a = m.event_ack;
    if (!a.accepted) {
        printf("[DLV] event_ack拒绝：%s（%s）停止重发\n",
               a.reply_to.c_str(), a.error.message.c_str());
    }
    // 成功：删除待确认事件（失败也移出，保留日志记录在上层）
    for (size_t i = 0; i < store_.pending_events.size(); i++) {
        if (store_.pending_events[i].message_id == a.reply_to) {
            store_.pending_events.erase(store_.pending_events.begin() + i);
            Store_Save();
            printf("[DLV] 事件已确认：%s（剩余%zu）\n",
                   a.reply_to.c_str(), store_.pending_events.size());
            return;
        }
    }
}

/*============================================================================
 *                              到站观察
 *============================================================================*/

void DeliveryController::Observe_Arrival(void)
{
    // 消费导航到站锁存（一次性；即使非NAVIGATING也消费防止残留）
    int map, node;
    if (!pending_arrival_) {
        if (nav_fsm.consume_target_arrival(&map, &node)) {
            pending_arrival_ = true;
            arrival_map_ = map;
            arrival_node_ = node;
            arrival_wait_start_ = lq_get_tick_ms();
            // 立即关闭运动许可：Motion_Permitted仅在NAVIGATING为true，
            // 切到ARRIVED_WAIT后5ms线程正常缓停，停稳后才发arrived
            if (state_ == DELIVERY_NAVIGATING) state_ = DELIVERY_ARRIVED_WAIT;
            printf("[DLV] 目标到站锁存：地图%d 节点%d，立即停车等待停稳\n", map, node);
        } else {
            return;
        }
    }
    if (arrival_reported_) return;   // 已上报，等event_ack/下一命令

    // Kevin 222.md二.4：确认目标Tag -> 受控减速 -> 停稳 -> arrived
    if (Control_Is_Stopped()) {
        arrival_reported_ = true;

        // 构造arrived可靠事件：持久化 -> 入发送队列（message_id固定用于重发）
        ArrivedEvent ev;
        ev.trip_id = cur_trip_;
        ev.stop_id = cur_stop_;
        ev.command_version = cur_cmd_ver_;
        ev.map_id = arrival_map_;
        ev.node_id = arrival_node_;
        ev.tag_id = arrival_node_;
        ev.motion_state = MOTION_STOPPED;   // 发送前必须停稳
        ev.position_source = POSITION_SOURCE_APRILTAG;

        CommonHeader h; h.type = "arrived";
        h.message_id = Next_Message_Id("evt-arrive");
        h.sent_at = Delivery_Format_Utc_Now();
        std::string json = Delivery_Serialize_Arrived(ev, h);

        PendingEvent pe;
        pe.message_id = h.message_id;
        pe.event_type = "arrived";
        pe.payload_json = json;
        pe.created_at = h.sent_at;
        store_.pending_events.push_back(pe);
        store_.last_current_node = arrival_node_;   // 最后确认位置
        Store_Save();
        if (gw_->Send_Line(json)) {
            printf("[DLV] arrived已发送：%s（节点%d，可靠重发直到event_ack）\n",
                   h.message_id.c_str(), arrival_node_);
        } else {
            printf("[DLV] 发送队列满，arrived待重发：%s\n", h.message_id.c_str());
        }
        return;
    }

    // 停稳超时：STOP_TIMEOUT故障
    uint32_t waited = lq_get_tick_ms() - arrival_wait_start_;
    if (waited > 5000) {
        arrival_reported_ = true;   // 防重复上报，等待人工检查
        Send_Fault(ERR_STOP_TIMEOUT, FAULT_STOP_REQUIRED, "stop not settled in 5s");
        printf("[DLV] 停稳超时5s，上报STOP_TIMEOUT\n");
    }
}

void DeliveryController::Resend_Pending_Events(void)
{
    if (store_.pending_events.empty()) return;
    // 全部按原message_id重发（服务器去重）
    for (size_t i = 0; i < store_.pending_events.size(); i++) {
        gw_->Send_Line(store_.pending_events[i].payload_json);
    }
}

/*============================================================================
 *                   屏幕确认事件 → user_action（阶段6）
 *============================================================================*/

// 生成并发送单个订单的确认事件（可靠：持久化+固定message_id重发）
bool DeliveryController::Send_User_Action(const OrderInfo& order, UserActionType action)
{
    UserActionEvent ev;
    ev.trip_id = cur_trip_;
    ev.stop_id = cur_stop_;
    ev.order_id = order.order_id;
    ev.action = action;
    ev.expected_status = (action == USER_ACTION_CONFIRM_PICKUP_LOADED) ? 2 : 4;
    ev.order_version = order.order_version;
    ev.display_revision = snap_.snapshot_version;

    CommonHeader h; h.type = "user_action";
    h.message_id = Next_Message_Id("evt-confirm");
    h.sent_at = Delivery_Format_Utc_Now();
    std::string json = Delivery_Serialize_UserAction(ev, h);

    // 防重复：同一订单已有未确认的user_action时忽略重复点击
    for (size_t i = 0; i < store_.pending_events.size(); i++) {
        if (store_.pending_events[i].event_type == "user_action" &&
            store_.pending_events[i].payload_json.find(order.order_id) != std::string::npos) {
            printf("[DLV] 订单%s确认已在等待ACK，忽略重复点击\n", order.order_id.c_str());
            return false;
        }
    }

    PendingEvent pe;
    pe.message_id = h.message_id;
    pe.event_type = "user_action";
    pe.payload_json = json;
    pe.created_at = h.sent_at;
    store_.pending_events.push_back(pe);
    Store_Save();
    if (gw_->Send_Line(json)) {
        printf("[DLV] user_action已发送：%s 订单%s 动作%s（可靠重发直到event_ack）\n",
               h.message_id.c_str(), order.order_id.c_str(),
               action == USER_ACTION_CONFIRM_PICKUP_LOADED ? "装载确认" : "取件确认");
    }
    return true;
}

void DeliveryController::Process_Screen_Events(void)
{
    ScreenEvent ev;
    while (Screen_Poll_Event(&ev)) {
        switch (ev) {
            case SCREEN_EV_STOP:
                // inhibit已在串口线程置位；此处仅记录（V1不上报fault，等服务器看心跳）
                printf("[DLV] 屏幕急停（安全禁止已由串口线程置位）\n");
                break;

            case SCREEN_EV_LOAD_CONFIRMED:
            case SCREEN_EV_UNLOAD_CONFIRMED: {
                if (!synced_) {
                    printf("[DLV] 未同步，忽略屏幕确认\n");
                    break;
                }
                // 槽位防错单：按当前active槽位对应的真实order_id确认（Kevin 222.md二.2）
                UserActionType action = (ev == SCREEN_EV_LOAD_CONFIRMED)
                                        ? USER_ACTION_CONFIRM_PICKUP_LOADED
                                        : USER_ACTION_CONFIRM_DROPOFF_TAKEN;
                int want_status = (ev == SCREEN_EV_LOAD_CONFIRMED) ? 2 : 4;

                // 当前订单：current_order_id优先，否则首个匹配状态的订单
                const OrderInfo* target = nullptr;
                for (size_t i = 0; i < snap_.orders.size(); i++) {
                    if (snap_.has_current_order &&
                        snap_.orders[i].order_id == snap_.current_order_id) {
                        target = &snap_.orders[i];
                        break;
                    }
                }
                if (target == nullptr) {
                    for (size_t i = 0; i < snap_.orders.size(); i++) {
                        if (snap_.orders[i].status == want_status) {
                            target = &snap_.orders[i];
                            break;
                        }
                    }
                }
                if (target == nullptr) {
                    printf("[DLV] 屏幕确认但无匹配订单（状态%d），忽略\n", want_status);
                    break;
                }
                if (target->status != want_status) {
                    printf("[DLV] 订单%s状态%d与确认动作不匹配（需%d），忽略（防错单）\n",
                           target->order_id.c_str(), target->status, want_status);
                    break;
                }
                Send_User_Action(*target, action);
                break;
            }
            default:
                break;
        }
    }
}

/*============================================================================
 *                              发送构造
 *============================================================================*/

std::string DeliveryController::Next_Message_Id(const char* prefix)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s-%llu", prefix, (unsigned long long)++msg_seq_);
    return std::string(buf);
}

void DeliveryController::Send_Hello(void)
{
    Hello h;
    h.device_token = cfg_.device_token;      // 不打印日志
    h.boot_id = boot_id_;
    h.software_version = "1.0.0-stage5";
    h.last_server_epoch = store_.server_epoch;
    h.last_snapshot_version = store_.snapshot_version;
    h.last_command_version = store_.command_version;
    h.map_id = cfg_.map_id;
    h.map_version = cfg_.map_version;
    h.map_checksum = cfg_.map_checksum;
    h.current_node = nav_fsm.get_status().current_id;
    h.position_valid = (h.current_node > 0);
    h.motion_state = Motion_Permitted() ? MOTION_MOVING : MOTION_STOPPED;
    h.emergency_latched = store_.emergency_latched;
    for (size_t i = 0; i < store_.pending_events.size(); i++) {
        h.pending_event_ids.push_back(store_.pending_events[i].message_id);
    }

    CommonHeader ch; ch.type = "hello";
    ch.message_id = Next_Message_Id("hello");
    ch.sent_at = Delivery_Format_Utc_Now();
    gw_->Send_Line(Delivery_Serialize_Hello(h, ch));
    printf("[DLV] hello已发送（boot=%s 待确认事件%zu）\n",
           boot_id_.c_str(), h.pending_event_ids.size());
}

void DeliveryController::Send_Heartbeat(void)
{
    Heartbeat hb;
    hb.boot_id = boot_id_;
    hb.sequence = ++hb_seq_;
    hb.uptime_ms = lq_get_tick_ms();
    hb.map_id = cfg_.map_id;
    const NavStatus& ns = nav_fsm.get_status();
    hb.current_node = ns.current_id;
    hb.last_tag_id = ns.current_id;   // 简化：最后识别节点
    hb.mile_since_node = Control_Get_Mile();
    hb.position_source = (ns.current_id > 0) ? POSITION_SOURCE_APRILTAG
                                             : POSITION_SOURCE_NONE;
    hb.motion_state = Motion_Permitted() ? MOTION_MOVING : MOTION_STOPPED;
    hb.navigation_state = get_nav_state_name(ns.state);
    hb.current_action = get_action_name(ns.current_action);
    hb.has_trip = !cur_trip_.empty();
    hb.trip_id = cur_trip_;
    hb.has_stop = !cur_stop_.empty();
    hb.stop_id = cur_stop_;
    hb.command_version = store_.command_version;

    CommonHeader ch; ch.type = "heartbeat";
    ch.message_id = Next_Message_Id("hb");
    ch.sent_at = Delivery_Format_Utc_Now();
    gw_->Send_Line(Delivery_Serialize_Heartbeat(hb, ch));
}

void DeliveryController::Send_Command_Ack(const std::string& reply_to, bool accepted,
                                          uint64_t version, const std::vector<int>& path,
                                          DeliveryErrorCode err, const std::string& err_msg)
{
    CommandAck ack;
    ack.reply_to = reply_to;
    ack.accepted = accepted;
    ack.command_version = version;
    ack.execution_state = accepted ? "NAVIGATING" : "STOPPED";
    ack.path = path;
    if (!accepted) {
        ack.error.has_error = true;
        ack.error.code = err;
        ack.error.message = err_msg;
    }
    CommonHeader h; h.type = "command_ack";
    h.message_id = Next_Message_Id("ack");
    h.sent_at = Delivery_Format_Utc_Now();
    if (accepted) printf("[DLV] ACK接受 v%llu\n", (unsigned long long)version);
    else          printf("[DLV] ACK拒绝 v%llu %s: %s\n", (unsigned long long)version,
                         Delivery_Error_Name(err), err_msg.c_str());
    gw_->Send_Line(Delivery_Serialize_CommandAck(ack, h));
}

void DeliveryController::Send_Fault(DeliveryErrorCode code, int level, const std::string& msg)
{
    FaultReport f;
    f.fault_code = code;
    f.fault_level = level;
    f.fault_message = msg;
    f.motion_state = Motion_Permitted() ? MOTION_MOVING : MOTION_STOPPED;
    f.has_trip = !cur_trip_.empty(); f.trip_id = cur_trip_;
    f.has_stop = !cur_stop_.empty();  f.stop_id = cur_stop_;
    f.command_version = store_.command_version;
    f.map_id = cfg_.map_id;
    f.current_node = nav_fsm.get_status().current_id;

    CommonHeader h; h.type = "fault";
    h.message_id = Next_Message_Id("fault");
    h.sent_at = Delivery_Format_Utc_Now();
    gw_->Send_Line(Delivery_Serialize_Fault(f, h));
}

void DeliveryController::Store_Save(void)
{
    // 禁止高频调用：仅命令/快照/事件/急停变化时（本文件各调用点已保证）
    Delivery_Store_Save(cfg_.store_path, store_);
}
