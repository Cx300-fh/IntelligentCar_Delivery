#ifndef __DIJKSTRA_HPP
#define __DIJKSTRA_HPP

#include <vector>

// 地图编号：与陶晶驰 page_sys.v_map.val 保持一致
#define MAP_NONE 0
#define MAP_THU  1
#define MAP_SUTD 2

#define MAX_NODE_COUNT 16
#define MAX_PATH_LEN   MAX_NODE_COUNT
#define INF 999999

// ==================== THU 节点ID定义 ====================
// ID 与陶晶驰 TARGET,1,target_id 完全一致：1~14 都是屏幕可选目标点。
// 重要：不再额外定义“辅助路口节点”。如果某地点位于路口，就直接把该目标点 ID 注册为路口。
#define THU_NODE_ZIJINGCAOCHANG 1   // 紫荆操场
#define THU_NODE_LIKELU         2   // 理科楼
#define THU_NODE_TUSHUGUAN      3   // 图书馆：目标点，同时也是路口
#define THU_NODE_SUSHIMIN       4   // 苏世民书院：目标点，同时也是路口
#define THU_NODE_DONGDACAOCHANG 5   // 东大操场：目标点，同时也是路口
#define THU_NODE_XIAOYIYUAN     6   // 校医院
#define THU_NODE_XUESHENGUSHE   7   // 学生宿舍
#define THU_NODE_DONGMEN        8   // 东门
#define THU_NODE_DALITANG       9   // 大礼堂：目标点，同时也是路口
#define THU_NODE_XINQINGHUA     10  // 新清华学堂：目标点，同时也是路口
#define THU_NODE_ZHONGYANGZHU   11  // 中央主楼
#define THU_NODE_A              12  // A 点：目标点，同时也是路口
#define THU_NODE_ZHAOLANYUAN    13  // 照澜院
#define THU_NODE_KEJIDALOU      14  // 科技大楼
#define THU_NODE_COUNT          15  // 有效 ID: 1~14，0 不使用

// ==================== SUTD 节点ID定义 ====================
// ID 与陶晶驰 TARGET,2,target_id 完全一致：1~12 都是屏幕可选目标点。
#define SUTD_NODE_A      1   // A：目标点，同时也是路口
#define SUTD_NODE_B      2   // B：目标点，同时也是路口
#define SUTD_NODE_C      3   // C
#define SUTD_NODE_D      4   // D
#define SUTD_NODE_E      5   // E：目标点，同时也是路口/中心连接点
#define SUTD_NODE_F      6   // F：目标点，同时也是路口
#define SUTD_NODE_LIB    7   // LIB
#define SUTD_NODE_AUD    8   // AUD
#define SUTD_NODE_SSH    9   // SSH
#define SUTD_NODE_CC     10  // CC
#define SUTD_NODE_POOL   11  // POOL
#define SUTD_NODE_SRC    12  // SRC
#define SUTD_NODE_COUNT  13  // 有效 ID: 1~12，0 不使用

// ==================== 朝向定义 ====================
#define DIR_NORTH   0
#define DIR_EAST    1
#define DIR_SOUTH   2
#define DIR_WEST    3
#define DIR_NS_NORTH    0
#define DIR_NS_SOUTH    1
#define DIR_NS_PENDING  2
#define DIR_EW_EAST     0
#define DIR_EW_WEST     1
#define DIR_EW_PENDING  2

// ==================== 转向定义 ====================
#define TURN_FOLLOW    0    // 自然循迹（默认）
#define TURN_STRAIGHT  1    // 直行
#define TURN_LEFT      2    // 左转
#define TURN_RIGHT     3    // 右转
#define TURN_UTURN     4    // 掉头

// ==================== 行进策略定义 ====================
#define STRATEGY_FOLLOW    0    // 自然循迹（循迹行驶）
#define STRATEGY_LINE      1    // 走直线（Tag丢失时使用）
#define STRATEGY_STOP      2    // 停车（到达终点）

// ==================== 边权模式定义 ====================
enum WeightMode {
    WEIGHT_BASE = 0,       // 仅使用基础距离
    WEIGHT_DYNAMIC = 1     // 基础距离 + 人工权重 + 动态权重
};

/**
 * @brief 动态路况权重提供器
 * @details
 *   第一版可以不设置 provider，直接使用 Dijkstra 内部的本地模拟权重。
 *   后续接入地图 API 时，只需要实现此接口并传入 Dijkstra。
 */
class DynamicWeightProvider {
public:
    virtual ~DynamicWeightProvider() {}
    virtual int get_dynamic_penalty(int map_id, int from, int to) = 0;
    virtual bool is_edge_blocked(int map_id, int from, int to) { return false; }
};

// ==================== 导航输入结构体 ====================
/**
 * @brief 导航更新输入结构体
 * @details 状态机调用 Dijkstra 时传入的导航参数
 */
struct NavUpdateInput {
    int map_id;         // 当前地图
    int current_id;     // 当前节点 ID（来自 AprilTag）
    int target_id;      // 目标 ID（来自屏幕选择）
    float tag_angle;    // 当前 AprilTag 相对角度

    NavUpdateInput(int m=0, int c=0, int t=0, float a=0.0f)
        : map_id(m), current_id(c), target_id(t), tag_angle(a) {}
};

// ==================== 节点信息结构体 ====================
/**
 * @brief 节点完整信息结构体
 * @details 一次返回节点的所有相关信息
 */
struct NavNodeInfo {
    int id;                 // 节点 ID
    const char* name;       // 节点名称
    int x, y;               // 节点坐标
    bool is_intersection;   // 是否是路口

    NavNodeInfo(int i=0, const char* n=nullptr, int xx=0, int yy=0, bool inter=false)
        : id(i), name(n), x(xx), y(yy), is_intersection(inter) {}
};

// ==================== 导航输出结构体 ====================
/**
 * @brief 导航结果输出结构体
 * @details Dijkstra 模块对外输出的完整导航数据
 */
struct NavResult {
    // 地图信息
    int map_id;
    const char* map_name;

    // 节点信息
    int target_id;
    const char* target_name;
    int current_id;
    const char* current_name;
    int prev_id;
    const char* prev_name;
    int next_id;
    const char* next_name;

    // 朝向信息
    float tag_angle;
    int dir_ns;
    int dir_ew;
    const char* dir_ns_name;
    const char* dir_ew_name;

    // 路口信息
    bool current_is_intersection;
    int current_x, current_y;
    int prev_x, prev_y;
    int next_x, next_y;

    // 转向计算
    int cross_value;        // 叉乘结果
    int dot_value;          // 点积结果
    int strategy;           // 行进策略 (STRATEGY_FOLLOW/LINE/STOP)
    int turn_action;        // 转向动作 (TURN_FOLLOW/STRAIGHT/LEFT/RIGHT/UTURN)

    // 路径信息
    int shortest_distance;
    int path[MAX_PATH_LEN];
    int path_len;

    // 有效性
    bool valid;

    NavResult()
        : map_id(0), map_name(nullptr),
          target_id(0), target_name(nullptr),
          current_id(0), current_name(nullptr),
          prev_id(0), prev_name(nullptr),
          next_id(0), next_name(nullptr),
          tag_angle(0.0f), dir_ns(0), dir_ew(0),
          dir_ns_name(nullptr), dir_ew_name(nullptr),
          current_is_intersection(false),
          current_x(0), current_y(0),
          prev_x(0), prev_y(0),
          next_x(0), next_y(0),
          cross_value(0), dot_value(0),
          strategy(STRATEGY_FOLLOW), turn_action(TURN_FOLLOW),
          shortest_distance(0), path_len(0), valid(false) {
        for (int i = 0; i < MAX_PATH_LEN; i++) path[i] = 0;
    }
};

/**
 * @brief 图的一条边
 * @details
 *   Dijkstra 使用邻接表 graph[u] 存储从 u 出发能到达的所有节点。
 *   由于校园道路一般是双向可走，所以添加边时使用 add_undirected_edge()，
 *   会同时写入 from->to 和 to->from 两条 Edge。
 */
struct Edge {
    int to;      // 这条边连接到的目标节点 ID
    int weight;  // 兼容旧代码的基础边权重
    int base_weight;
    int manual_penalty;
    int dynamic_penalty;
    bool blocked;

    Edge(int t, int w)
        : to(t), weight(w), base_weight(w),
          manual_penalty(0), dynamic_penalty(0), blocked(false) {}
};

class Dijkstra {
public:
    /**
     * @brief 构造 Dijkstra 路径规划对象
     * @param map_id 初始地图，默认 THU
     * @note
     *   构造时会自动初始化对应地图的：
     *   1. 节点名称 node_names[]
     *   2. 路口数组 intersection_nodes[]
     *   3. 邻接表 graph
     *   4. Dijkstra 临时数组 dist/prev/visited
     */
    Dijkstra(int map_id = MAP_THU);

    /**
     * @brief 切换当前地图
     * @param map_id MAP_THU 或 MAP_SUTD
     * @details
     *   THU 和 SUTD 都从 ID=1 开始编号，因此同一个 tag_id 在不同地图中含义不同。
     *   例如：THU 的 1 是紫荆操场，SUTD 的 1 是 A。
     *   所以路径规划前必须保证 Dijkstra 当前地图与屏幕选择的地图一致。
     */
    void set_map(int map_id);

    int get_map(void) const;              // 获取当前地图编号
    int get_node_count(void) const;       // 获取当前地图节点数量上界，合法 ID 范围为 1 ~ get_node_count()-1

    // 判断节点是否是路口。
    // 路口信息由 intersection_nodes[] 数组维护，不靠 ID 大小判断。
    // 本版没有辅助路口：数组里放的都是"本身也是目标点"的节点 ID。
    bool is_intersection_node(int id);

    /**
     * @brief 判断两个节点是否相邻（有边直接连接）
     * @param id1 节点1 ID
     * @param id2 节点2 ID
     * @return true相邻，false不相邻
     * @details 用于验证检测到的Tag是否是当前节点的邻近点，防止误判
     */
    bool is_neighbor(int id1, int id2);

    const char* get_map_name(void) const;      // 获取当前地图名称：THU / SUTD / NONE
    const char* get_node_name(int id);         // 根据节点 ID 获取地点名（中文，用于调试）
    const char* get_node_name_en(int id);      // 根据节点 ID 获取地点名（英文缩写，用于陶晶驰屏幕显示）
    int get_node_id(const char* name);         // 根据地点名反查 ID，主要用于调试或后续扩展

    /**
     * @brief 将陶晶驰屏幕目标 ID 转换为 Dijkstra 节点 ID
     * @details
     *   当前 V5 方案中没有辅助路口节点，屏幕目标 ID、AprilTag ID、Dijkstra 节点 ID 三者一致。
     *   因此合法情况下直接返回 target_id。
     *   仍保留该函数，是为了把“通信协议”和“图节点编号”隔离开，方便以后扩展。
     */
    int target_to_node(int map_id, int target_id);

    // 转向判断相关函数：通过 prev-current-next 三点叉乘判断左转/右转/直行/掉头。
    int get_turn_direction(int prev_id, int current_id, int next_id);
    int get_turn_direction_from_heading(int current_id, int next_id, int vehicle_angle);  // 起始点用车辆朝向
    const char* get_turn_name(int turn);

    void add_undirected_edge(int from, int to, int weight); // 添加双向边
    void set_dynamic_weight_provider(DynamicWeightProvider* provider);
    bool set_edge_penalty(int from, int to, int manual_penalty, int dynamic_penalty, bool blocked);
    int get_edge_cost(int from, int to, WeightMode mode = WEIGHT_DYNAMIC);
    void get_node_coord(int id, int* x, int* y);            // 获取节点坐标，用于路口转向判断
    int find_shortest_path(int start, int end, WeightMode mode = WEIGHT_DYNAMIC); // 计算最短距离，结果写入 dist[] 和 prev[]
    int get_path(int start, int end, int* path, WeightMode mode = WEIGHT_DYNAMIC); // 获取最短路径节点序列
    int get_path_cost(const int* path, int path_len, WeightMode mode = WEIGHT_DYNAMIC);
    void print_path(int start, int end);                    // 打印路径，调试用

    // ==================== 高级导航 API ====================

    /**
     * @brief 更新导航状态（主入口）
     * @param input 导航输入参数（当前位置、目标、角度等）
     * @param output 导航输出结果（路径、转向决策等）
     * @return true 计算成功，false 失败
     * @details 这是 Dijkstra 模块的主入口，状态机每帧调用此函数获取导航决策
     */
    bool update_nav(const NavUpdateInput& input, NavResult& output);

    /**
     * @brief 获取节点完整信息
     * @param id 节点 ID
     * @return NavNodeInfo 包含名称、坐标、是否路口等信息
     */
    NavNodeInfo get_node_info(int id);

    /**
     * @brief 测试最短路径（独立调试接口）
     * @param start 起点 ID
     * @param end 终点 ID
     * @param result 输出结果结构体
     * @return true 路径存在，false 不可达或参数错误
     * @details 不依赖当前 Tag 和状态机，用于单独测试地图连通性
     */
    bool test_shortest_path(int start, int end, NavResult& result);

    // ==================== 路口查表 API ====================

    /**
     * @brief 查表获取期望朝向（用于确定车辆在路口的朝向）
     * @param prev_id 上一个节点 ID
     * @param current_id 当前路口 ID
     * @return 期望朝向角度（0-360度），-1表示未知
     * @details 根据 prev->current 的方向，确定车辆在当前路口应该面向的方向
     *          例如：从图书馆到苏世民书院，应该面朝东
     */
    int get_expected_heading(int prev_id, int current_id);

    /**
     * @brief 查表获取转向动作（用于确定从当前路口往哪转）
     * @param current_id 当前路口 ID
     * @param next_id 下一个节点 ID
     * @return 转向动作：TURN_LEFT/RIGHT/STRAIGHT/UTURN
     * @details 根据 current->next 的方向，确定应该往哪转
     *          这个函数内部可以用叉乘，也可以用预定义的转向表
     */
    int get_intersection_turn(int current_id, int next_id);

    /**
     * @brief 获取路段的期望方位（用于非路口路段的方位对比）
     * @param from_id 起点 ID
     * @param to_id 终点 ID
     * @return 期望方位角（0-360度）
     */
    int get_segment_heading(int from_id, int to_id);

private:
    int current_map;        // 当前加载的地图编号
    int active_node_count;  // 当前地图节点数量上界；ID=0 不使用，因此有效 ID 为 1~active_node_count-1

    // 邻接表：graph[u] 保存从节点 u 出发可直接到达的所有节点。
    // 例如 graph[THU_NODE_TUSHUGUAN] 里会有理科楼、苏世民书院等相邻节点。
    std::vector<std::vector<Edge>> graph;
    DynamicWeightProvider* weight_provider;

    // Dijkstra 临时数组。
    // dist[i]    : start 到 i 的当前最短距离
    // prev[i]    : 最短路径中 i 的前驱节点，用于最后反向回溯路径
    // visited[i] : i 是否已经确定最短距离
    int dist[MAX_NODE_COUNT];
    int prev[MAX_NODE_COUNT];
    bool visited[MAX_NODE_COUNT];
    int edge_manual_penalty[3][MAX_NODE_COUNT][MAX_NODE_COUNT];
    int edge_dynamic_penalty[3][MAX_NODE_COUNT][MAX_NODE_COUNT];
    bool edge_blocked[3][MAX_NODE_COUNT][MAX_NODE_COUNT];

    const char* node_names[MAX_NODE_COUNT];      // 节点 ID 到地点名称的映射（中文，用于调试）
    const char* node_names_en[MAX_NODE_COUNT];   // 节点 ID 到地点名称的映射（英文缩写，用于陶晶驰屏幕显示）

    // 路口节点数组。
    // 本项目 V5 版本没有辅助路口 ID：路口一定是地图上已有的目标点。
    // 例如 THU_NODE_TUSHUGUAN 既是终点选项，也可以作为路口决策点。
    int intersection_nodes[MAX_NODE_COUNT];
    int intersection_count;

    void clear_all(void);
    void clear_weight_tables(void);
    void add_intersection_node(int id);
    void init_map(int map_id);
    void init_thu(void);
    void init_sutd(void);
};

#endif
