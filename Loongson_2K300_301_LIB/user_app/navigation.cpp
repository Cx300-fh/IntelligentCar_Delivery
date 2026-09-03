/**
 * @file navigation.cpp
 * @brief 导航状态机模块实现
 * @details 实现基于状态机的全自主导航控制逻辑
 */

#include "navigation.hpp"

// 获取当前时间（毫秒）
static uint32_t get_current_ms(void) {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

// 全局导航状态机实例
NavigationFSM nav_fsm;

// =============================================================================
//                           转向查找表
// =============================================================================
// 格式：{prev_id, current_id, next_id, action}
// action: TURN_STRAIGHT=1, TURN_LEFT=2, TURN_RIGHT=3, TURN_UTURN=4

// =============================================================================
// THU 地图路口转向表
// =============================================================================
// 点位编号：
//   1=紫荆操场, 2=理科楼, 3=图书馆, 4=苏世民书院, 5=东大操场
//   6=校医院, 7=学生宿舍, 8=东门, 9=大礼堂
//   10=新清华学堂, 11=中央主楼, 12=A点, 13=照澜院, 14=科技大楼
static const TurnTableEntry thu_turn_table[] = {
    // ==================== 图书馆 (ID=3) ====================
    {1, 3, 2, TURN_RIGHT},      // 紫荆操场 → 图书馆 → 理科楼    : 右转
    {1, 3, 4, TURN_LEFT},       // 紫荆操场 → 图书馆 → 苏世民书院: 左转
    {2, 3, 1, TURN_LEFT},       // 理科楼   → 图书馆 → 紫荆操场  : 左转
    {2, 3, 4, TURN_STRAIGHT},   // 理科楼   → 图书馆 → 苏世民书院: 直行
    {4, 3, 1, TURN_RIGHT},      // 苏世民书院→图书馆 → 紫荆操场  : 右转
    {4, 3, 2, TURN_STRAIGHT},   // 苏世民书院→图书馆 → 理科楼    : 直行

    // ==================== 苏世民书院 (ID=4) ====================
    {3, 4, 5, TURN_STRAIGHT},   // 图书馆   → 苏世民书院 → 东大操场    : 直行
    {3, 4, 7, TURN_RIGHT},      // 图书馆   → 苏世民书院 → 学生宿舍    : 右转
    {5, 4, 3, TURN_STRAIGHT},   // 东大操场 → 苏世民书院 → 图书馆      : 直行
    {5, 4, 7, TURN_LEFT},       // 东大操场 → 苏世民书院 → 学生宿舍    : 左转
    {7, 4, 3, TURN_LEFT},       // 学生宿舍 → 苏世民书院 → 图书馆      : 左转
    {7, 4, 5, TURN_RIGHT},      // 学生宿舍 → 苏世民书院 → 东大操场    : 右转

    // ==================== 东大操场 (ID=5) ====================
    {1, 5, 4, TURN_RIGHT},      // 紫荆操场 → 东大操场 → 苏世民书院    : 右转
    {1, 5, 8, TURN_LEFT},       // 紫荆操场 → 东大操场 → 东门          : 左转
    {4, 5, 1, TURN_LEFT},       // 苏世民书院→东大操场 → 紫荆操场      : 左转
    {4, 5, 8, TURN_STRAIGHT},   // 苏世民书院→东大操场 → 东门          : 直行
    {8, 5, 1, TURN_RIGHT},      // 东门     → 东大操场 → 紫荆操场      : 右转
    {8, 5, 4, TURN_STRAIGHT},   // 东门     → 东大操场 → 苏世民书院    : 直行

    // ==================== 大礼堂 (ID=9) ====================
    {6, 9, 10, TURN_STRAIGHT},  // 校医院     → 大礼堂 → 新清华学堂    : 直行
    {6, 9, 13, TURN_RIGHT},     // 校医院     → 大礼堂 → 照澜院        : 右转
    {10, 9, 6, TURN_STRAIGHT},  // 新清华学堂 → 大礼堂 → 校医院        : 直行
    {10, 9, 13, TURN_LEFT},     // 新清华学堂 → 大礼堂 → 照澜院        : 左转
    {13, 9, 6, TURN_LEFT},      // 照澜院     → 大礼堂 → 校医院        : 左转
    {13, 9, 10, TURN_RIGHT},    // 照澜院     → 大礼堂 → 新清华学堂    : 右转

    // ==================== 新清华学堂 (ID=10) ====================
    {7, 10, 9, TURN_RIGHT},     // 学生宿舍   → 新清华学堂 → 大礼堂      : 右转
    {7, 10, 11, TURN_LEFT},     // 学生宿舍   → 新清华学堂 → 中央主楼    : 左转
    {9, 10, 7, TURN_LEFT},      // 大礼堂     → 新清华学堂 → 学生宿舍    : 左转
    {9, 10, 11, TURN_STRAIGHT}, // 大礼堂     → 新清华学堂 → 中央主楼    : 直行
    {11, 10, 7, TURN_RIGHT},    // 中央主楼   → 新清华学堂 → 学生宿舍    : 右转
    {11, 10, 9, TURN_STRAIGHT}, // 中央主楼   → 新清华学堂 → 大礼堂      : 直行

    // ==================== A点 (ID=12) ====================
    {8, 12, 11, TURN_RIGHT},    // 东门   → A点 → 中央主楼    : 右转
    {8, 12, 14, TURN_STRAIGHT}, // 东门   → A点 → 科技大楼    : 直行
    {11, 12, 8, TURN_LEFT},     // 中央主楼→A点 → 东门        : 左转
    {11, 12, 14, TURN_RIGHT},   // 中央主楼→A点 → 科技大楼    : 右转
    {14, 12, 8, TURN_STRAIGHT}, // 科技大楼→A点 → 东门        : 直行
    {14, 12, 11, TURN_LEFT},    // 科技大楼→A点 → 中央主楼    : 左转

    // 结束标记
    {-1, -1, -1, ACTION_NONE}
};

// =============================================================================
// SUTD 地图路口转向表
// =============================================================================
// 点位编号：
//   1=A, 2=B, 3=C, 4=D, 5=E, 6=F
//   7=LIB, 8=AUD, 9=SSH, 10=CC, 11=POOL, 12=SRC
static const TurnTableEntry sutd_turn_table[] = {
    // ==================== A点 (ID=1) ====================
    {2, 1, 3, TURN_STRAIGHT},   // B  → A → C  : 直行
    {2, 1, 10, TURN_RIGHT},     // B  → A → CC : 右转
    {3, 1, 2, TURN_STRAIGHT},   // C  → A → B  : 直行
    {3, 1, 10, TURN_LEFT},      // C  → A → CC : 左转
    {10, 1, 2, TURN_LEFT},      // CC → A → B  : 左转
    {10, 1, 3, TURN_RIGHT},     // CC → A → C  : 右转

    // ==================== B点 (ID=2) ====================
    {1, 2, 11, TURN_STRAIGHT},  // A    → B → POOL : 直行
    {1, 2, 12, TURN_LEFT},      // A    → B → SRC  : 左转
    {11, 2, 1, TURN_STRAIGHT},  // POOL → B → A    : 直行
    {11, 2, 12, TURN_RIGHT},    // POOL → B → SRC  : 右转
    {12, 2, 1, TURN_RIGHT},     // SRC  → B → A    : 右转
    {12, 2, 11, TURN_LEFT},     // SRC  → B → POOL : 左转

    // ==================== C点 (ID=3) ====================
    {1, 3, 4, TURN_STRAIGHT},   // A   → C → D   : 直行
    {1, 3, 7, TURN_RIGHT},      // A   → C → LIB : 右转
    {4, 3, 1, TURN_STRAIGHT},   // D   → C → A   : 直行
    {4, 3, 7, TURN_LEFT},       // D   → C → LIB : 左转
    {7, 3, 1, TURN_LEFT},       // LIB → C → A   : 左转
    {7, 3, 4, TURN_RIGHT},      // LIB → C → D   : 右转

    // ==================== D点 (ID=4) ====================
    {3, 4, 6, TURN_STRAIGHT},   // C   → D → F   : 直行
    {3, 4, 8, TURN_RIGHT},      // C   → D → AUD : 右转
    {6, 4, 3, TURN_STRAIGHT},   // F   → D → C   : 直行
    {6, 4, 8, TURN_LEFT},       // F   → D → AUD : 左转
    {8, 4, 3, TURN_LEFT},       // AUD → D → C   : 左转
    {8, 4, 6, TURN_RIGHT},      // AUD → D → F   : 右转

    // ==================== E点 (ID=5) ====================
    {8, 5, 9, TURN_LEFT},       // AUD → E → SSH : 左转
    {8, 5, 10, TURN_RIGHT},     // AUD → E → CC  : 右转
    {8, 5, 12, TURN_STRAIGHT},  // AUD → E → SRC : 直行
    {9, 5, 8, TURN_RIGHT},      // SSH → E → AUD : 右转
    {9, 5, 10, TURN_STRAIGHT},  // SSH → E → CC  : 直行
    {9, 5, 12, TURN_LEFT},      // SSH → E → SRC : 左转
    {10, 5, 8, TURN_LEFT},      // CC  → E → AUD : 左转
    {10, 5, 9, TURN_STRAIGHT},  // CC  → E → SSH : 直行
    {10, 5, 12, TURN_RIGHT},    // CC  → E → SRC : 右转
    {12, 5, 8, TURN_STRAIGHT},  // SRC → E → AUD : 直行
    {12, 5, 9, TURN_RIGHT},     // SRC → E → SSH : 右转
    {12, 5, 10, TURN_LEFT},     // SRC → E → CC  : 左转

    // ==================== F点 (ID=6) ====================
    {4, 6, 9, TURN_RIGHT},      // D   → F → SSH : 右转
    {4, 6, 11, TURN_STRAIGHT},  // D   → F → POOL: 直行
    {9, 6, 4, TURN_LEFT},       // SSH → F → D   : 左转
    {9, 6, 11, TURN_RIGHT},     // SSH → F → POOL: 右转
    {11, 6, 4, TURN_STRAIGHT},  // POOL→ F → D   : 直行
    {11, 6, 9, TURN_LEFT},      // POOL→ F → SSH : 左转

    // ==================== CC点 (ID=10) ====================
    {1, 10, 5, TURN_STRAIGHT},  // A   → CC → E   : 直行
    {1, 10, 7, TURN_LEFT},      // A   → CC → LIB : 左转
    {5, 10, 1, TURN_STRAIGHT},  // E   → CC → A   : 直行
    {5, 10, 7, TURN_RIGHT},     // E   → CC → LIB : 右转
    {7, 10, 1, TURN_RIGHT},     // LIB → CC → A   : 右转
    {7, 10, 5, TURN_LEFT},      // LIB → CC → E   : 左转

    // 结束标记
    {-1, -1, -1, ACTION_NONE}
};

/*============================================================================
 *                              工具函数
 *============================================================================*/

/**
 * @brief 获取导航状态名称字符串
 * @param state 导航状态枚举值
 * @return 状态名称字符串（用于调试打印）
 */
const char* get_nav_state_name(NavState state) {
    switch (state) {
        case NAV_STATE_IDLE:      return "IDLE";
        case NAV_STATE_SEARCHING: return "SEARCHING";
        case NAV_STATE_LOCATING:  return "LOCATING";
        case NAV_STATE_AT_NODE:   return "AT_NODE";
        case NAV_STATE_WAITING:   return "WAITING";
        case NAV_STATE_EXECUTING: return "EXECUTING";
        case NAV_STATE_ARRIVED:   return "ARRIVED";
        default:                  return "UNKNOWN";
    }
}

/**
 * @brief 获取动作类型名称字符串
 * @param action 动作类型枚举值
 * @return 动作名称字符串（用于调试打印）
 */
const char* get_action_name(ActionType action) {
    switch (action) {
        case ACTION_NONE:      return "NONE";
        case ACTION_FOLLOW:    return "FOLLOW";
        case ACTION_STRAIGHT:  return "STRAIGHT";
        case ACTION_TURN_LEFT: return "LEFT";
        case ACTION_TURN_RIGHT:return "RIGHT";
        case ACTION_UTURN:     return "UTURN";
        case ACTION_STOP:      return "STOP";
        default:               return "UNKNOWN";
    }
}

/*============================================================================
 *                              类实现
 *============================================================================*/

/**
 * @brief 构造函数
 * @details 初始化时dijkstra指针设为nullptr，在init()中创建实例
 */
NavigationFSM::NavigationFSM() : dijkstra(nullptr) {
}

/**
 * @brief 初始化导航状态机
 * @details 
 *   - 重置任务和状态数据
 *   - 创建Dijkstra路径规划器实例（默认加载THU地图）
 *   - 在系统启动时调用一次
 */
void NavigationFSM::init(void) {
    task = NavTask();
    status = NavStatus();
    
    if (dijkstra) {
        delete dijkstra;
    }
    dijkstra = new Dijkstra(MAP_THU);
    
    printf("[Nav] 导航状态机初始化完成\n");
}

/**
 * @brief 启动导航任务
 * @param map_id    地图编号（MAP_THU或MAP_SUTD）
 * @param target_id 目标节点ID
 * @return true启动成功，false启动失败（目标ID无效）
 * @details
 *   - 设置任务为激活状态
 *   - 切换到SEARCHING状态，开始盲走寻找第一个Tag
 *   - 标记为第一个节点（is_first_node=true），此时没有prev信息
 */
bool NavigationFSM::start_task(int map_id, int target_id) {
    if (target_id <= 0) {
        printf("[Nav] 错误：目标ID无效\n");
        return false;
    }
    
    // 初始化任务参数
    task.active = true;
    task.map_id = map_id;
    task.target_id = target_id;
    
    // 初始化导航状态
    status = NavStatus();
    status.state = NAV_STATE_SEARCHING;
    status.is_first_node = true;      // 标记为起点，需要盲走
    status.has_prev_info = false;     // 暂时没有上一个位置信息
    
    // 切换地图
    dijkstra->set_map(map_id);
    
    printf("[Nav] 任务启动：地图=%s, 目标=%s\n", 
           dijkstra->get_map_name(), 
           dijkstra->get_node_name(target_id));
    
    // TODO: 语音播报 - 任务开始
    // Voice_Broadcast_Start();
    
    return true;
}

/**
 * @brief 取消导航任务
 * @details 
 *   - 设置任务为非激活状态
 *   - 切换到IDLE状态，动作为STOP
 *   - 小车将停止运动
 */
void NavigationFSM::cancel_task(void) {
    if (task.active) {
        printf("[Nav] 任务取消\n");
        // TODO: 语音播报 - 任务取消
        // Voice_Broadcast_Cancel();
    }

    task.active = false;
    task.paused = false;
    status.state = NAV_STATE_IDLE;
    status.current_action = ACTION_STOP;
}

/**
 * @brief 避障绕行：标记 from-to 边阻塞并重新规划
 * @details 与 cancel_task() 的区别：不清空任务，保留目标，只是换一条路走。
 *   任务未激活（task.active==false）时只标记阻塞，供以后规划参考，返回true。
 *   任务激活时立即从当前位置重新规划；若无其他可达路径，停车（保留任务，
 *   等待边解除阻塞或障碍物消失后由调用方重试）。
 */
bool NavigationFSM::reroute_blocked_edge(int from, int to) {
    if (!dijkstra) return false;
    dijkstra->block_edge(from, to);
    printf("[Nav] 道路阻塞：%s-%s 已标记不可通行\n",
           dijkstra->get_node_name(from), dijkstra->get_node_name(to));

    if (!task.active) return true;

    if (!replan_path()) {
        printf("[Nav] 绕行失败：无其他可达路径，停车等待\n");
        status.current_action = ACTION_STOP;
        return false;
    }

    status.current_action = ACTION_FOLLOW;
    status.state = NAV_STATE_EXECUTING;
    printf("[Nav] 绕行：已重新规划路径，继续前往%s\n",
           dijkstra->get_node_name(task.target_id));
    return true;
}

/*============================================================================
 *                    配送模式分段导航接口（阶段5新增）
 *============================================================================*/

/**
 * @brief 分段导航：位置已知时从当前点直接规划到新目标
 * @details 与start_task（盲走）的区别：
 *   - 不进入SEARCHING，保留current_id/prev_id定位（同地图时）
 *   - 直接replan后进入EXECUTING循迹，到下一个Tag自然进入AT_NODE决策
 *   - 目标==当前位置：不启动电机，直接ARRIVED+锁存到站
 */
bool NavigationFSM::start_leg(int map_id, int target_id) {
    if (target_id <= 0) {
        printf("[Nav] start_leg错误：目标ID无效\n");
        return false;
    }
    // 位置未知（V1禁止盲走，由配送协调器拒绝goto_stop）
    if (status.current_id <= 0) {
        printf("[Nav] start_leg错误：当前位置未知，拒绝分段导航\n");
        return false;
    }
    // 地图切换时旧定位失效，按位置未知处理（V1单图运行，不做跨图保留）
    if (task.active && task.map_id != map_id) {
        printf("[Nav] start_leg错误：跨地图切换（%d→%d），旧定位失效\n",
               task.map_id, map_id);
        return false;
    }

    int keep_cur = status.current_id;
    int keep_prev = status.prev_id;
    bool keep_has_prev = status.has_prev_info;

    task.active = true;
    task.paused = false;
    task.map_id = map_id;
    task.target_id = target_id;

    // 重置状态但保留定位信息
    status = NavStatus();
    status.current_id = keep_cur;
    status.prev_id = keep_prev;
    status.has_prev_info = keep_has_prev;
    status.is_first_node = false;   // 已知位置，不是盲走起点

    dijkstra->set_map(map_id);

    // 目标==当前位置：不启动电机，直接到站锁存
    if (keep_cur == target_id) {
        status.state = NAV_STATE_ARRIVED;
        status.current_action = ACTION_STOP;
        printf("[Nav] start_leg：目标即当前位置(%s)，直接到站\n",
               dijkstra->get_node_name(target_id));
        return true;
    }

    if (!replan_path()) {
        // 规划失败：保持停车，任务回退为非激活
        task.active = false;
        status.state = NAV_STATE_IDLE;
        status.current_action = ACTION_STOP;
        return false;
    }

    // 从当前路段出发。
    //
    // 停靠点本身就是路口时，方向必须先定下来：原实现无条件 ACTION_FOLLOW，
    // 等于把出发方向交给摄像头此刻恰好看到的哪条线——实测在苏世民书院(4)
    // 本该直行去图书馆(3)，却循着旁边的线左拐进了学生宿舍(7)。
    //
    // 出发时没有“来路”可用（车刚停稳等确认，也可能是重启后重新定位的），
    // 所以改用 AprilTag 解出的车头朝向判断转向，不依赖历史轨迹。
    // 定不出朝向（tag 丢失或与当前节点不符）时退回原来的循迹行为。
    status.current_action = ACTION_FOLLOW;
    status.state = NAV_STATE_EXECUTING;

    if (dijkstra->is_intersection_node(keep_cur) && status.next_id > 0 &&
        det_found && tag_id == keep_cur) {
        int heading = dijkstra->tag_angle_to_heading(keep_cur, tag_angle);
        int turn = dijkstra->get_turn_direction_from_heading(keep_cur, status.next_id, heading);

        ActionType act = ACTION_FOLLOW;
        switch (turn) {
            case TURN_STRAIGHT: act = ACTION_STRAIGHT;   break;
            case TURN_LEFT:     act = ACTION_TURN_LEFT;  break;
            case TURN_RIGHT:    act = ACTION_TURN_RIGHT; break;
            case TURN_UTURN:    act = ACTION_UTURN;      break;
            default:            act = ACTION_FOLLOW;     break;
        }

        if (act != ACTION_FOLLOW) {
            // 转向动作靠里程结束（control.cpp 的 TURN_MILE_LIMIT），而里程只在
            // 稳定期结束时清零，所以这里必须走 WAITING，不能直接 EXECUTING，
            // 否则上一段残留的里程会让转向刚开始就被判定完成。
            status.current_action = act;
            status.state = NAV_STATE_WAITING;
            status.wait_start_ms = get_current_ms();
            status.wait_duration_ms = node_settle_ms_;
        }

        printf("[Nav] start_leg路口朝向：tag角%.1f°→车头%d°，%s → %s，动作=%s\n",
               tag_angle, heading,
               dijkstra->get_node_name(keep_cur),
               dijkstra->get_node_name(status.next_id),
               get_action_name(status.current_action));

        if (act == ACTION_UTURN) {
            printf("[Nav] 警告：车头背对目标，将在出发前掉头\n");
        }
    }

    printf("[Nav] start_leg：从%s出发 → 目标%s（保留定位）\n",
           dijkstra->get_node_name(keep_cur),
           dijkstra->get_node_name(target_id));
    return true;
}

/**
 * @brief 消费目标到站锁存（配送协调器每帧调用）
 */
bool NavigationFSM::consume_target_arrival(int* map_id, int* node_id) {
    if (!status.target_arrived) return false;
    if (map_id) *map_id = task.map_id;
    if (node_id) *node_id = task.target_id;
    status.target_arrived = false;   // 同一到站只消费一次
    return true;
}

/**
 * @brief 暂停（hold）：保留任务/路径/定位，仅停车
 */
void NavigationFSM::pause_task(void) {
    if (!task.active) return;
    task.paused = true;
    status.current_action = ACTION_STOP;
    printf("[Nav] 任务暂停（保留目标%s与定位）\n",
           dijkstra->get_node_name(task.target_id));
}

/**
 * @brief 恢复：从当前位置重新评估路径并继续
 */
bool NavigationFSM::resume_task(void) {
    if (!task.active) return false;
    if (!task.paused) return true;   // 本就在运行
    if (status.current_id <= 0) {
        printf("[Nav] resume错误：位置未知\n");
        return false;
    }
    task.paused = false;
    // 偏航可能发生在暂停期间，从当前位置重新规划
    if (!replan_path()) {
        printf("[Nav] resume错误：无法规划\n");
        return false;
    }
    status.current_action = ACTION_FOLLOW;
    status.state = NAV_STATE_EXECUTING;
    printf("[Nav] 任务恢复：从%s继续 → %s\n",
           dijkstra->get_node_name(status.current_id),
           dijkstra->get_node_name(task.target_id));
    return true;
}

/**
 * @brief 状态机主更新函数（每帧调用）
 * @details 
 *   根据当前状态调用对应的状态处理函数：
 *   - IDLE:      空闲等待
 *   - SEARCHING: 盲走寻找第一个Tag
 *   - AT_NODE:   到达节点，进行决策
 *   - WAITING:   等待3秒
 *   - EXECUTING: 执行转向/直行等动作
 *   - ARRIVED:   到达终点，任务结束
 */
void NavigationFSM::update(void) {
    if (!task.active) {
        status.state = NAV_STATE_IDLE;
        return;
    }

    // 暂停（hold）：保留任务和定位，停车不动
    if (task.paused) {
        status.current_action = ACTION_STOP;
        return;
    }

    // 状态机主循环：根据当前状态分发到对应处理函数
    switch (status.state) {
        case NAV_STATE_IDLE:
            handle_idle();
            break;
        case NAV_STATE_SEARCHING:
            handle_searching();
            break;
        case NAV_STATE_LOCATING:
            handle_locating();
            break;
        case NAV_STATE_AT_NODE:
            handle_at_node();
            break;
        case NAV_STATE_WAITING:
            handle_waiting();
            break;
        case NAV_STATE_EXECUTING:
            handle_executing();
            break;
        case NAV_STATE_ARRIVED:
            handle_arrived();
            break;
    }
}

/*============================================================================
 *                              状态处理函数
 *============================================================================*/

/**
 * @brief 开始原地定位（配送模式）
 * @details 停车状态下扫描Tag确认当前位置。不动车（动作恒为STOP）。
 *          start_leg可直接从LOCATING状态接管（保留已确认的current_id）。
 */
void NavigationFSM::begin_localization(int map_id) {
    task.active = true;          // 状态机需要激活才会扫描Tag
    task.paused = false;
    task.map_id = map_id;
    task.target_id = 0;
    dijkstra->set_map(map_id);

    // 保留可能已有的定位，否则从未知开始
    if (status.current_id <= 0) {
        status.current_id = -1;
        status.has_prev_info = false;
    }
    status.target_arrived = false;
    status.state = NAV_STATE_LOCATING;
    status.current_action = ACTION_STOP;
    printf("[Nav] 原地定位开始（地图%d），当前已知位置：%s\n",
           map_id,
           status.current_id > 0 ? dijkstra->get_node_name(status.current_id) : "未知");
}

/**
 * @brief 处理LOCATING状态（原地定位）
 * @details 与SEARCHING相同的Tag验证，但动作恒为STOP（不动车）。
 *          识别到有效Tag即确认位置并保持在LOCATING（等待start_leg）。
 */
void NavigationFSM::handle_locating(void) {
    status.current_action = ACTION_STOP;   // 定位阶段绝不动车

    if (tag_id >= 0 && det_found) {
        int max_valid_id = (task.map_id == MAP_SUTD) ? 12 : 14;
        if (tag_id < 1 || tag_id > max_valid_id) return;   // 地图外Tag忽略

        if (status.current_id != tag_id) {
            status.current_id = tag_id;
            status.has_prev_info = false;   // 静止识别无来向信息
            status.is_first_node = false;
            printf("[Nav] 定位确认：当前位置=%s（静止待命）\n",
                   dijkstra->get_node_name(tag_id));
        }
    }
    // 保持在LOCATING：位置持续跟随可见Tag，直到start_leg/cancel_task
}

/**
 * @brief 处理IDLE状态
 * @details 
 *   空闲状态，小车静止等待任务启动
 *   动作设置为STOP，电机不转动
 */
void NavigationFSM::handle_idle(void) {
    status.current_action = ACTION_STOP;
}

/**
 * @brief 处理SEARCHING状态（盲走阶段）
 * @details 
 *   起点阶段：不知道当前位置，只能盲走
 *   - 动作设置为FOLLOW，循迹行驶
 *   - 持续检测AprilTag，直到找到稳定的Tag
 *   - 找到Tag后切换到AT_NODE状态进行决策
 *   
 *   【严格验证】首个Tag必须是地图范围内的有效节点：
 *   - THU地图：ID必须在1-14范围内
 *   - SUTD地图：ID必须在1-12范围内
 *   - 超出范围的ID（如0、15、17等）视为无效，假装没识别到
 */
void NavigationFSM::handle_searching(void) {
    status.current_action = ACTION_FOLLOW;
    
    // 检查是否检测到Tag
    if (tag_id >= 0 && det_found) {
        // ===== 首个Tag严格验证 =====
        // 必须是当前地图的有效节点ID
        int max_valid_id = (task.map_id == MAP_SUTD) ? 12 : 14;
        
        if (tag_id < 1 || tag_id > max_valid_id) {
            // 地图外的Tag，假装没识别到（可能是干扰或错误识别）
            // 不打印日志，避免刷屏
            return;
        }
        
        // 有效Tag，切换到节点处理
        status.current_id = tag_id;
        status.state = NAV_STATE_AT_NODE;
        
        printf("[Nav] 首个Tag检测：ID=%d（%s），开始决策\n", 
               tag_id, dijkstra->get_node_name(tag_id));
    }
}

/**
 * @brief 处理AT_NODE状态（节点决策）
 * @details
 *   核心决策逻辑：
 *   1. 检查是否到达终点
 *   2. 如果是第一个节点（起点）：盲走策略
 *   3. 正常节点：
 *      - 检查是否偏离路径（当前Tag != 预期Tag）
 *      - 偏离路径时：判断是否走反（next_id == prev_id），走反则掉头返回
 *      - 未偏离时：前进路径索引，路口查表转向 / 非路口循迹
 *   4. 进入WAITING状态等待3秒
 */
void NavigationFSM::handle_at_node(void) {
    int current_id = status.current_id;
    int target_id = task.target_id;

    // TODO: 语音播报 - 当前位置
    // Voice_Broadcast_Location(tag_id);
    
    // ===== 终点判断 =====
    if (current_id == target_id) {
        status.state = NAV_STATE_ARRIVED;
        return;
    }
    
    // ===== 起点处理（盲走策略，仅本地模式start_task进入）=====
    if (status.is_first_node) {
        // 规划路径且更新路径索引和下一个目标 ID
        replan_path();

        // 起点无法确定朝向，采用盲走策略：
        // 能左转就左转，不能左转就走直行
        // （但是不是通过action进行舵机固定打角，而是通过follow_left，循左边线单边实现自动）
        status.current_action = ACTION_FOLLOW;
        status.state = NAV_STATE_WAITING;
        status.wait_start_ms = get_current_ms();
        status.wait_duration_ms = 3000;   // 本地模式起点盲走前3秒（人为观察用）

        printf("[Nav] 起点：等待3秒后盲走（左转优先，不行则执行）\n");
        return;
    } else {
        // ===== 正常节点处理 =====
        status.route_flag = 0;  // 重置偏离路线标志，默认未知

        // ----- 路径偏离检测 -----
        if (current_id == status.next_id) {
            // 正常到达预期节点，路径索引前进
            status.route_flag = 1;  // 正常路线
            replan_path();  // 规划路径且更新路径索引和下一个目标 ID
            printf("[Nav] 正常到达预期节点：%s\n", dijkstra->get_node_name(current_id));
        } else {
            // 走错路了！当前节点不是预期的下一个
            status.route_flag = 2;  // 偏离路线
            printf("[Nav] 偏离路径！预期=%s, 实际=%s\n",
                   dijkstra->get_node_name(status.next_id),
                   dijkstra->get_node_name(current_id));
            replan_path();  // 规划路径且更新路径索引和下一个目标 ID

            // 决策：判断是否走反（预期的下一个点是上一个点）
            if (status.next_id == status.prev_id) {
                // 朝向反了，需要掉头返回
                // 掉头阶段状态由5ms控制线程自持（见dir_control），这里只请求清零里程
                Control_Request_Mile_Clear();  // 清零里程，用于后续判断何时退出掉头动作
                printf("[Nav] 决策：掉头返回\n");
                status.current_action = ACTION_UTURN;
                status.state = NAV_STATE_WAITING;
                status.wait_start_ms = get_current_ms();
                status.wait_duration_ms = node_settle_ms_;   // 掉头前同样只做短暂稳定
                return;
            } else {
                // 没走反，从当前点重新规划路径到目标
                printf("[Nav] 决策：从当前点重新规划\n");
                replan_path();  // 规划路径且更新路径索引和下一个目标 ID
                // 保留prev_id和has_prev_info，当前位置作为新起点继续正常导航
            }
        }
    }
    
    // ----- 路口转向决策 -----
    bool is_intersection = dijkstra->is_intersection_node(current_id);

    if (is_intersection && status.has_prev_info) {
        // 路口且有来向信息，查表决定转向
        int action = lookup_turn_action(status.prev_id, current_id, status.next_id);

        switch (action) {
            case TURN_STRAIGHT:
                status.current_action = ACTION_STRAIGHT;
                break;
            case TURN_LEFT:
                status.current_action = ACTION_TURN_LEFT;
                break;
            case TURN_RIGHT:
                status.current_action = ACTION_TURN_RIGHT;
                break;
            case TURN_UTURN:
                status.current_action = ACTION_UTURN;
                break;
            default:
                status.current_action = ACTION_FOLLOW;
                break;
        }

        printf("[Nav] 路口查表：%s → %s → %s, 动作=%s\n",
                dijkstra->get_node_name(status.prev_id),
                dijkstra->get_node_name(current_id),
                dijkstra->get_node_name(status.next_id),
                get_action_name(status.current_action));

    } else {
        // 非路口或无prev信息，继续循迹
        status.current_action = ACTION_FOLLOW;
        printf("[Nav] 非路口或无prev信息，继续循迹\n");
    }

    // 节点稳定期：Tag消抖/转向准备（原固定3秒改为可配置，任务书十一.3）
    status.state = NAV_STATE_WAITING;
    status.wait_start_ms = get_current_ms();
    status.wait_duration_ms = node_settle_ms_;
}

/**
 * @brief 处理WAITING状态（节点稳定期）
 * @details
 *   原固定3秒改为可配置稳定期（node_settle_ms_，默认200ms）：
 *   - 中间节点：Tag消抖/转向准备，不播语音不进业务（任务书十一.3）
 *   - 起点盲走（本地模式）：保留3秒并播报位置
 */
void NavigationFSM::handle_waiting(void) {
    uint32_t elapsed = get_current_ms() - status.wait_start_ms;

    // 位置语音：仅本地模式起点或显式开启中间节点播报时（中间节点默认不播）
    if ((status.is_first_node || announce_intermediate_) &&
        (!status.voice_announced || status.current_id != status.last_announced_id)) {
        Voice_Play_Current_Node(task.map_id, status.current_id);
        status.voice_announced = true;
        status.last_announced_id = status.current_id;
    }

    if (elapsed >= status.wait_duration_ms) {
        // 稳定期结束，开始执行动作
        Control_Request_Mile_Clear();  // 请求5ms线程清零里程（重新累计，用于结束转向动作）
        status.state = NAV_STATE_EXECUTING;
        status.voice_announced = false;  // 重置语音播报标志
        printf("[Nav] 稳定期结束(%ums)，执行动作：%s\n",
               (unsigned)status.wait_duration_ms, get_action_name(status.current_action));
    }
}

/**
 * @brief 处理EXECUTING状态（执行动作）
 * @details
 *   执行当前设定的动作：
 *   - 循迹/直行/左转/右转/掉头
 *   - 实际控制由外部control模块根据current_action执行
 *   - 持续检测新Tag，到达后切换到AT_NODE状态
 *
 *   【严格验证】后续Tag必须是当前节点的邻近点：
 *   - 只有与当前位置(current_id)相邻的节点才视为有效到达
 *   - 非邻近点的Tag视为干扰，假装没识别到
 *   - 这可以防止远处Tag的误检测导致导航错乱
 */
void NavigationFSM::handle_executing(void) {

    if (Control_Is_Uturning()) return;  // 掉头中不处理新节点（状态由5ms控制线程持有）

    // 检测是否到达下一个节点（新Tag）
    if (tag_id >= 0 && det_found && tag_id != status.current_id) {

        // ===== 后续Tag严格验证 =====
        // 必须是当前节点的邻近点（有边直接连接）
        if (!dijkstra->is_neighbor(status.current_id, tag_id)) {
            // 非邻近点，可能是远处干扰或错误识别，假装没识别到
            return;
        }

        // 立刻更新上一个节点为当前节点，更新当前节点为当前检测到的Tag
        status.prev_id = status.current_id;
        status.current_id = tag_id;
        status.has_prev_info = true;
        status.is_first_node = false;

        printf("[Nav] 到达新节点：%s（来自：%s）\n",
               dijkstra->get_node_name(tag_id),
               dijkstra->get_node_name(status.prev_id));
        status.state = NAV_STATE_AT_NODE;
    }
}

/**
 * @brief 处理ARRIVED状态（到达终点）
 * @details
 *   到达目标点：
 *   - 动作设置为STOP，停车
 *   - 任务标记为非激活
 *   - 置到站锁存（target_arrived），由配送协调器消费后生成arrived事件
 *   - 到站后不自行前往下一站，等待下一次start_leg/goto_stop
 */
void NavigationFSM::handle_arrived(void) {
    status.current_action = ACTION_STOP;
    if (task.active) {
        task.active = false;
        status.target_arrived = true;   // 到站锁存：一次到站只消费一次
    }

    // 播报到达终点语音（仅播报一次）
    if (!status.arrived_announced) {
        Voice_Play_Current_Node(task.map_id, task.target_id);
        usleep(200 * 1000);  // 等待ASRPRO超时断帧，避免地名与END粘连成一帧
        Voice_Play_Arrived();  // 发送END终点指令
        status.arrived_announced = true;
        printf("[Nav] 到达终点：%s（已播报，到站锁存已置位）\n",
               dijkstra->get_node_name(task.target_id));
    } else {
        printf("[Nav] 到达终点：%s\n", dijkstra->get_node_name(task.target_id));
    }
}

/*============================================================================
 *                              辅助函数
 *============================================================================*/

/**
 * @brief 查表获取路口转向动作
 * @param prev_id    上一个节点ID（来向）
 * @param current_id 当前路口节点ID
 * @param next_id    下一个节点ID（去向）
 * @return 转向动作（TURN_STRAIGHT/LEFT/RIGHT/UTURN）
 * @details 
 *   根据当前地图选择对应的转向表（THU或SUTD）
 *   查表失败时回退到叉乘计算（基于坐标几何）
 */
int NavigationFSM::lookup_turn_action(int prev_id, int current_id, int next_id) {
    const TurnTableEntry* table = (task.map_id == MAP_SUTD) ? sutd_turn_table : thu_turn_table;
    
    // 遍历转向表查找匹配项
    for (int i = 0; table[i].prev_id >= 0; i++) {
        if (table[i].prev_id == prev_id && 
            table[i].current_id == current_id && 
            table[i].next_id == next_id) {
            return table[i].action;
        }
    }
    
    // 查表失败，打印警告并使用叉乘计算作为后备方案
    printf("[Nav] 警告：查表失败 (%d,%d,%d)，使用默认策略\n", prev_id, current_id, next_id);
    return dijkstra->get_turn_direction(prev_id, current_id, next_id);
}

/**
 * @brief 计算到达终点路径的距离
 * @param current_id   当前位置节点ID
 * @param target_id    目标节点ID
 * @param direct_dist  输出：距离
 * @details 
 */
void NavigationFSM::calculate_path_distances(int current_id, int target_id, int* dist) {
    // 计算距离
    *dist = dijkstra->find_shortest_path(current_id, target_id);
    if (*dist < 0) *dist = INF;
}

/**
 * @brief 重新规划路径，并更新路径索引和下一个目标ID
 * @return true规划成功，false规划失败
 * @details 
 *   基于当前位置（status.current_id）到目标点规划最短路径
 *   更新path数组、path_len、path_index和next_id
 */
bool NavigationFSM::replan_path(void) {
    int start_id = status.current_id;
    int target_id = task.target_id;
    
    if (start_id <= 0 || target_id <= 0) {
        printf("[Nav] 错误：无法规划路径，起点或终点无效\n");
        return false;
    }
    
    // 调用Dijkstra算法计算最短路径
    status.path_len = dijkstra->get_path(start_id, target_id, status.path);
    
    if (status.path_len == 0) {
        printf("[Nav] 错误：无法到达目标\n");
        return false;
    }
    
    // 重置路径索引，更新下一个目标
    status.path_index = 0;
    update_next_id();

    calculate_path_distances(status.current_id, task.target_id, &status.dist);
    
    // 打印规划结果
    printf("[Nav] 路径规划：");
    for (int i = 0; i < status.path_len; i++) {
        printf("%s", dijkstra->get_node_name(status.path[i]));
        if (i < status.path_len - 1) printf(" → ");
    }
    printf("\n");
    
    return true;
}

/**
 * @brief 更新下一个目标节点ID
 * @details 
 *   根据当前path_index计算next_id
 *   如果已到达终点，next_id设为-1
 */
void NavigationFSM::update_next_id(void) {
    if (status.path_index < status.path_len - 1) {
        status.next_id = status.path[status.path_index + 1];
    } else {
        status.next_id = -1;  // 已到达终点
    }
}

/**
 * @brief 设置等待完成（外部调用接口）
 * @details 
 *   用于外部模块（如屏幕按钮）强制结束等待
 *   立即从WAITING状态切换到EXECUTING状态
 */
void NavigationFSM::set_wait_done(void) {
    if (status.state == NAV_STATE_WAITING) {
        status.state = NAV_STATE_EXECUTING;
    }
}

/**
 * @brief 设置动作执行完成（外部调用接口）
 * @details 
 *   用于外部控制模块通知动作已完成
 *   将动作重置为FOLLOW，继续循迹
 */
void NavigationFSM::set_action_done(void) {
    if (status.state == NAV_STATE_EXECUTING) {
        status.current_action = ACTION_FOLLOW;
    }
}
