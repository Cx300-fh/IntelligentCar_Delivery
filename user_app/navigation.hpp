/**
 * @file navigation.hpp
 * @brief 导航状态机模块头文件
 * @details 实现基于状态机的全自主导航控制
 *          - 起点盲走策略（不知道朝向前提下）
 *          - 到达Tag后的路径重评估
 *          - 路口查表转向决策
 */

#ifndef __NAVIGATION_HPP
#define __NAVIGATION_HPP

#include "include.hpp"

// 前向声明
class Dijkstra;

// 路径最大长度（与 dijkstra.hpp 保持一致）
#define NAV_MAX_PATH_LEN 16

/*============================================================================
 *                              导航状态定义
 *============================================================================*/

// 导航主状态
enum NavState {
    NAV_STATE_IDLE = 0,         // 空闲/等待指令
    NAV_STATE_SEARCHING,        // 寻找第一个Tag（盲走阶段）
    NAV_STATE_AT_NODE,          // 到达节点，决策中
    NAV_STATE_WAITING,          // 等待3秒
    NAV_STATE_EXECUTING,        // 执行动作（循迹/转向/直行）
    NAV_STATE_ARRIVED,          // 到达终点
};

// 执行动作类型
enum ActionType {
    ACTION_NONE = 0,            // 无动作
    ACTION_FOLLOW,              // 循迹行驶
    ACTION_STRAIGHT,            // 直行（路口中心）
    ACTION_TURN_LEFT,           // 左转
    ACTION_TURN_RIGHT,          // 右转
    ACTION_UTURN,               // 掉头
    ACTION_STOP,                // 停车
};

/*============================================================================
 *                              查表结构定义
 *============================================================================*/

// 路口转向查表条目
// 格式：从prev来，在current，要去next → 执行什么动作
struct TurnTableEntry {
    int prev_id;        // 上一个节点ID
    int current_id;     // 当前路口节点ID
    int next_id;        // 下一个节点ID
    int action;         // 动作：左转/右转/直行/掉头
    
    TurnTableEntry(int p=-1, int c=-1, int n=-1, int a=ACTION_NONE) 
        : prev_id(p), current_id(c), next_id(n), action(a) {}
};

/*============================================================================
 *                              导航数据结构
 *============================================================================*/

// 导航任务配置
struct NavTask {
    bool active;            // 任务是否激活
    int map_id;             // 地图ID
    int target_id;          // 目标节点ID
    
    NavTask() : active(false), map_id(0), target_id(0) {}
};

// 导航状态数据
struct NavStatus {
    // 当前状态
    NavState state;         // 当前导航状态
    ActionType current_action;  // 当前执行的动作
    
    // 位置信息
    int current_id;         // 当前所在节点ID
    int prev_id;            // 上一个节点ID
    int next_id;            // 预期的下一个节点ID
    int expected_next_id;   // 检测前预期的下一个节点（用于路线偏离检测）
    
    // 路径信息
    int path[NAV_MAX_PATH_LEN]; // 当前规划的路径
    int path_len;           // 路径长度
    int path_index;         // 当前在路径中的索引
    
    // 等待计时
    uint32_t wait_start_ms; // 等待开始时间
    uint32_t wait_duration_ms;  // 等待持续时间
    
    // 盲走阶段标记
    bool is_first_node;     // 是否是第一个节点（起点）
    bool has_prev_info;     // 是否有上一个位置的信息
    
    NavStatus() 
        : state(NAV_STATE_IDLE), current_action(ACTION_NONE),
          current_id(-1), prev_id(-1), next_id(-1), expected_next_id(-1),
          path_len(0), path_index(0),
          wait_start_ms(0), wait_duration_ms(3000),
          is_first_node(true), has_prev_info(false) {
        for (int i = 0; i < NAV_MAX_PATH_LEN; i++) path[i] = 0;
    }
};

/*============================================================================
 *                              导航状态机类
 *============================================================================*/

class NavigationFSM {
public:
    NavigationFSM();
    
    // 初始化
    void init(void);
    
    // 启动导航任务
    bool start_task(int map_id, int target_id);
    bool start_leg(int map_id, int target_node);
    bool on_arrived_node(int node_id);
    
    // 取消导航任务
    void cancel_task(void);
    
    // 状态机主更新（每帧调用）
    void update(void);
    
    // 获取当前状态
    NavState get_state(void) const { return status.state; }
    ActionType get_action(void) const { return status.current_action; }
    
    // 获取导航状态数据（用于调试显示）
    const NavStatus& get_status(void) const { return status; }
    const NavTask& get_task(void) const { return task; }
    
    // 查询是否正在导航
    bool is_navigating(void) const { return task.active; }
    
    // 获取当前应执行的动作（供控制模块使用）
    ActionType get_current_action(void) const { return status.current_action; }

    // 获取Dijkstra路径规划器指针（供屏幕显示使用）
    Dijkstra* get_dijkstra(void) const { return dijkstra; }

    // 设置等待完成（外部调用，表示3秒等待结束）
    void set_wait_done(void);
    
    // 设置动作执行完成（外部调用）
    void set_action_done(void);

private:
    NavTask task;           // 当前任务
    NavStatus status;       // 当前状态
    Dijkstra* dijkstra;     // 路径规划器
    
    // 查表查找转向动作
    int lookup_turn_action(int prev_id, int current_id, int next_id);
    

    
    // 计算从当前点到目标的两条路径距离
    // 返回：从当前点直达目标的距离，通过返程再到目标的距离
    void calculate_path_distances(int current_id, int via_id, int target_id,
                                   int* direct_dist, int* return_dist);
    
    // 决策：走错路后，是继续从新点走，还是返回
    bool should_return_to_prev(int current_id, int prev_id, int target_id);
    
    // 状态处理函数
    void handle_idle(void);
    void handle_searching(void);
    void handle_at_node(void);
    void handle_waiting(void);
    void handle_executing(void);
    void handle_arrived(void);
    
    // 更新路径规划
    // at_start: 是否是起点（第一个节点），起点时expected_next_id设为current_id
    bool replan_path(bool at_start = false);
    
    // 填充next_id（根据当前路径索引）
    void update_next_id(void);
};

// 全局导航状态机实例
extern NavigationFSM nav_fsm;

// 导航状态字符串（调试用）
const char* get_nav_state_name(NavState state);
const char* get_action_name(ActionType action);

#endif /* __NAVIGATION_HPP */
