/**
 * @file dijkstra.cpp
 * @brief THU / SUTD 双地图最短路径规划实现
 * @details
 *   本文件与陶晶驰屏幕的地图选择协议配套：
 *   - TARGET,1,x 表示 THU 地图的第 x 个目标点；
 *   - TARGET,2,x 表示 SUTD 地图的第 x 个目标点。
 *
 *   V5 设计原则：
 *   1. 不再创建辅助路口 ID，避免 Tag ID、屏幕目标 ID、Dijkstra 节点 ID 混乱。
 *   2. 如果某个目标点本身就在路口，就把这个目标点 ID 加入 intersection_nodes[]。
 *   3. 路径规划时，路径数组中的每个节点都是真实地图目标点。
 *   4. 是否需要路口决策，通过 is_intersection_node(id) 判断。
 */
#include "dijkstra.hpp"
#include <cmath>    // for atan2

/**
 * @brief 构造函数
 * @param map_id 初始地图编号，默认 MAP_THU
 * @note 先清空所有缓存，再根据 map_id 初始化对应地图。
 */
Dijkstra::Dijkstra(int map_id) { clear_all(); init_map(map_id); }

/**
 * @brief 清空图结构和算法缓存
 * @details
 *   每次切换地图前都要清空：
 *   - graph 邻接表，避免 THU/SUTD 的边混在一起；
 *   - node_names，避免旧地图名称残留；
 *   - intersection_nodes，避免旧地图路口残留；
 *   - dist/prev/visited，避免上一次 Dijkstra 计算结果影响下一次。
 */
void Dijkstra::clear_all(void) {
    current_map = MAP_NONE;
    active_node_count = 0;
    graph.clear(); graph.resize(MAX_NODE_COUNT);
    for (int i = 0; i < MAX_NODE_COUNT; i++) {
        dist[i] = INF; prev[i] = -1; visited[i] = false;
        node_names[i] = "Unknown"; node_names_en[i] = "Unknown";
        intersection_nodes[i] = -1;
    }
    intersection_count = 0;
}

/**
 * @brief 注册一个路口节点
 * @param id 节点 ID
 * @details
 *   本项目中“路口”不是单独一类新节点，而是节点的一个属性。
 *   例如 THU_NODE_TUSHUGUAN 既是屏幕可选目标“图书馆”，也是路口。
 *   因此这里把已有目标点 ID 加入 intersection_nodes[]，后续用
 *   is_intersection_node(id) 判断是否需要做路口转向决策。
 */
void Dijkstra::add_intersection_node(int id) {
    if (id < 0 || id >= active_node_count || intersection_count >= MAX_NODE_COUNT) return;
    for (int i = 0; i < intersection_count; i++) if (intersection_nodes[i] == id) return;
    intersection_nodes[intersection_count++] = id;
}

/**
 * @brief 判断一个节点是否是路口
 * @param id 待判断节点 ID
 * @return true 是路口；false 不是路口
 * @note 不通过 ID 大小判断路口，而是查 intersection_nodes[]，这样以后增删路口更安全。
 */
bool Dijkstra::is_intersection_node(int id) {
    for (int i = 0; i < intersection_count; i++) if (intersection_nodes[i] == id) return true;
    return false;
}

void Dijkstra::set_map(int map_id) { init_map(map_id); }
int Dijkstra::get_map(void) const { return current_map; }
int Dijkstra::get_node_count(void) const { return active_node_count; }
const char* Dijkstra::get_map_name(void) const { return current_map == MAP_SUTD ? "SUTD" : (current_map == MAP_THU ? "THU" : "NONE"); }

void Dijkstra::init_map(int map_id) {
    clear_all();
    if (map_id == MAP_SUTD) init_sutd(); else init_thu();
}

/**
 * @brief 初始化 THU 地图
 * @details
 *   THU 地图共14个目标点，根据实际邻近关系建立连接：
 *   1-紫荆操场, 2-理科楼, 3-图书馆, 4-苏世民书院, 5-东大操场
 *   6-校医院, 7-学生宿舍, 8-东门, 9-大礼堂, 10-新清华学堂
 *   11-中央主楼, 12-A点, 13-照澜院, 14-科技大楼
 */
void Dijkstra::init_thu(void) {
    current_map = MAP_THU;
    active_node_count = THU_NODE_COUNT;

    // 中文名称（用于调试打印）
    node_names[1]="紫荆操场"; node_names[2]="理科楼"; node_names[3]="图书馆";
    node_names[4]="苏世民书院"; node_names[5]="东大操场"; node_names[6]="校医院";
    node_names[7]="学生宿舍"; node_names[8]="东门"; node_names[9]="大礼堂";
    node_names[10]="新清华学堂"; node_names[11]="中央主楼"; node_names[12]="A点";
    node_names[13]="照澜院"; node_names[14]="科技大楼";

    // 英文缩写（用于陶晶驰屏幕显示，避免中文乱码）
    node_names_en[1]="ZJCC"; node_names_en[2]="LKL"; node_names_en[3]="TSG";
    node_names_en[4]="SSM"; node_names_en[5]="DDCC"; node_names_en[6]="XYY";
    node_names_en[7]="XSSS"; node_names_en[8]="DM"; node_names_en[9]="DLT";
    node_names_en[10]="XQH"; node_names_en[11]="ZYZL"; node_names_en[12]="A";
    node_names_en[13]="ZLY"; node_names_en[14]="KJDL";

    // THU 路口数组：图书馆、苏世民书院、东大操场、大礼堂、新清华学堂、A点
    add_intersection_node(THU_NODE_TUSHUGUAN);      // 3
    add_intersection_node(THU_NODE_SUSHIMIN);       // 4
    add_intersection_node(THU_NODE_DONGDACAOCHANG); // 5
    add_intersection_node(THU_NODE_DALITANG);       // 9
    add_intersection_node(THU_NODE_XINQINGHUA);     // 10
    add_intersection_node(THU_NODE_A);              // 12

    // 紫荆操场(1)的邻近点：图书馆(3)、东大操场(5)
    add_undirected_edge(1, 3, 150);
    add_undirected_edge(1, 5, 150);

    // 理科楼(2)的邻近点：图书馆(3)、校医院(6)
    add_undirected_edge(2, 3, 75);
    add_undirected_edge(2, 6, 130);

    // 图书馆(3)的邻近点：紫荆操场(1)、理科楼(2)、苏世民书院(4)
    // (部分已在上面添加)
    add_undirected_edge(3, 4, 85);

    // 苏世民书院(4)的邻近点：图书馆(3)、东大操场(5)、学生宿舍(7)
    // (3-4已在上面添加)
    add_undirected_edge(4, 5, 85);
    add_undirected_edge(4, 7, 65);

    // 东大操场(5)的邻近点：紫荆操场(1)、苏世民书院(4)、东门(8)
    // (1-5, 4-5已在上面添加)
    add_undirected_edge(5, 8, 140);

    // 校医院(6)的邻近点：理科楼(2)、大礼堂(9)
    // (2-6已在上面添加)
    add_undirected_edge(6, 9, 205);

    // 学生宿舍(7)的邻近点：苏世民书院(4)、新清华学堂(10)
    // (4-7已在上面添加)
    add_undirected_edge(7, 10, 65);

    // 东门(8)的邻近点：东大操场(5)、A点(12)
    // (5-8已在上面添加)
    add_undirected_edge(8, 12, 65);

    // 大礼堂(9)的邻近点：校医院(6)、新清华学堂(10)、照澜院(13)
    // (6-9已在上面添加)
    add_undirected_edge(9, 10, 85);
    add_undirected_edge(9, 13, 150);

    // 新清华学堂(10)的邻近点：学生宿舍(7)、大礼堂(9)、中央主楼(11)
    // (7-10, 9-10已在上面添加)
    add_undirected_edge(10, 11, 85);

    // 中央主楼(11)的邻近点：新清华学堂(10)、A点(12)
    // (10-11已在上面添加)
    add_undirected_edge(11, 12, 75);

    // A点(12)的邻近点：东门(8)、中央主楼(11)、科技大楼(14)
    // (8-12, 11-12已在上面添加)
    add_undirected_edge(12, 14, 140);

    // 照澜院(13)的邻近点：大礼堂(9)、科技大楼(14)
    // (9-13已在上面添加)
    add_undirected_edge(13, 14, 85);

    // 科技大楼(14)的邻近点：A点(12)、照澜院(13)
    // (12-14, 13-14已在上面添加)
}

/**
 * @brief 初始化 SUTD 地图
 * @details
 *   SUTD 地图共12个目标点，根据实际邻近关系建立连接：
 *   1=A, 2=B, 3=C, 4=D, 5=E, 6=F
 *   7=LIB, 8=AUD, 9=SSH, 10=CC, 11=POOL, 12=SRC
 */
void Dijkstra::init_sutd(void) {
    current_map = MAP_SUTD;
    active_node_count = SUTD_NODE_COUNT;

    // 中文名称（用于调试打印）
    node_names[1]="A"; node_names[2]="B"; node_names[3]="C"; node_names[4]="D";
    node_names[5]="E"; node_names[6]="F"; node_names[7]="LIB"; node_names[8]="AUD";
    node_names[9]="SSH"; node_names[10]="CC"; node_names[11]="POOL"; node_names[12]="SRC";

    // 英文缩写（用于陶晶驰屏幕显示，避免中文乱码）
    node_names_en[1]="A"; node_names_en[2]="B"; node_names_en[3]="C"; node_names_en[4]="D";
    node_names_en[5]="E"; node_names_en[6]="F"; node_names_en[7]="LIB"; node_names_en[8]="AUD";
    node_names_en[9]="SSH"; node_names_en[10]="CC"; node_names_en[11]="POOL"; node_names_en[12]="SRC";

    // SUTD 路口数组：A、B、C、D、E、F、CC
    add_intersection_node(SUTD_NODE_A);   // 1
    add_intersection_node(SUTD_NODE_B);   // 2
    add_intersection_node(SUTD_NODE_C);   // 3
    add_intersection_node(SUTD_NODE_D);   // 4
    add_intersection_node(SUTD_NODE_E);   // 5
    add_intersection_node(SUTD_NODE_F);   // 6
    add_intersection_node(SUTD_NODE_CC);  // 10

    // A点(1)的邻近点：B点(2)、C点(3)、CC(10)
    add_undirected_edge(1, 2, 236);
    add_undirected_edge(1, 3, 247);
    add_undirected_edge(1, 10, 80);

    // B点(2)的邻近点：A点(1)、POOL(11)、SRC(12)
    // (1-2已在上面添加)
    add_undirected_edge(2, 11, 138);
    add_undirected_edge(2, 12, 70);

    // C点(3)的邻近点：A点(1)、D点(4)、LIB(7)
    // (1-3已在上面添加)
    add_undirected_edge(3, 4, 80);
    add_undirected_edge(3, 7, 60);

    // D点(4)的邻近点：C点(3)、F点(6)、AUD(8)
    // (3-4已在上面添加)
    add_undirected_edge(4, 6, 278);
    add_undirected_edge(4, 8, 60);

    // E点(5)的邻近点：AUD(8)、SSH(9)、CC(10)、SRC(12)
    add_undirected_edge(5, 8, 65);
    add_undirected_edge(5, 9, 75);
    add_undirected_edge(5, 10, 80);
    add_undirected_edge(5, 12, 65);

    // F点(6)的邻近点：D点(4)、SSH(9)、POOL(11)
    // (4-6已在上面添加)
    add_undirected_edge(6, 9, 70);
    add_undirected_edge(6, 11, 75);

    // AUD(8)的邻近点：D点(4)、E点(5)
    // (4-8, 5-8已在上面添加)

    // LIB(7)的邻近点：C点(3)、CC(10)
    // (3-7已在上面添加)
    add_undirected_edge(7, 10, 65);

    // SSH(9)的邻近点：E点(5)、F点(6)
    // (5-9, 6-9已在上面添加)

    // CC(10)的邻近点：A点(1)、E点(5)、LIB(7)
    // (1-10, 5-10, 7-10已在上面添加)

    // POOL(11)的邻近点：B点(2)、F点(6)
    // (2-11, 6-11已在上面添加)

    // SRC(12)的邻近点：B点(2)、E点(5)
    // (2-12, 5-12已在上面添加)
}

/**
 * @brief 屏幕目标 ID 转 Dijkstra 节点 ID
 * @details
 *   当前设计中三种 ID 完全统一：
 *   屏幕目标 ID = AprilTag ID = Dijkstra 节点 ID。
 *   所以合法时直接返回 target_id。
 */
int Dijkstra::target_to_node(int map_id, int target_id) {
    if (map_id == MAP_THU  && target_id >= 1 && target_id <= 14) return target_id;
    if (map_id == MAP_SUTD && target_id >= 1 && target_id <= 12) return target_id;
    return -1;
}

const char* Dijkstra::get_node_name(int id) { return (id >= 0 && id < active_node_count) ? node_names[id] : "Unknown"; }

const char* Dijkstra::get_node_name_en(int id) { return (id >= 0 && id < active_node_count) ? node_names_en[id] : "Unknown"; }
int Dijkstra::get_node_id(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < active_node_count; i++) if (strcmp(node_names[i], name) == 0) return i;
    return -1;
}

/**
 * @brief 判断路口转向方向（直行//左转/右转/掉头）
 * @param prev_id 前一个节点 ID
 * @param current_id 当前路口节点 ID
 * @param next_id 下一个节点 ID
 * @return 转向常量: TURN_FOLLOW / TURN_STRAIGHT / TURN_LEFT / TURN_RIGHT / TURN_UTURN
 *
 * @details 原理：通过向量叉乘判断转向
 *
 *   向量A = current - prev （来车方向）
 *   向量B = next - current  （去往方向）
 * 
 *   叉乘 = Ax * By - Ay * Bx
 *          叉乘 > 0  →  左转（B在A左侧）
 *          叉乘 < 0  →  右转（B在A右侧）
 *          叉乘 = 0  →  直行或掉头（共线）
 * 
 *   叉乘为0时，通过点积判断：
 *   点积 = Ax * Bx + Ay * By
 *           点积 > 0  →  直行（同向）
 *           点积 < 0  →  掉头（反向）
 */
int Dijkstra::get_turn_direction(int prev_id, int current_id, int next_id) {
    // 参数合法性检查
    if (prev_id < 0 || prev_id >= active_node_count ||
        current_id < 0 || current_id >= active_node_count ||
        next_id < 0 || next_id >= active_node_count) {
        return TURN_FOLLOW;
    }

    // 获取三个节点的坐标
    int px, py, cx, cy, nx, ny;
    get_node_coord(prev_id, &px, &py);
    get_node_coord(current_id, &cx, &cy);
    get_node_coord(next_id, &nx, &ny);

    // 计算向量
    int ax = cx - px;   // 向量A = current - prev
    int ay = cy - py;
    int bx = nx - cx;   // 向量B = next - current
    int by = ny - cy;

    // 叉乘
    int cross = ax * by - ay * bx;

    // 判断转向
    if (cross > 0)  return TURN_LEFT;      // 左转
    if (cross < 0)  return TURN_RIGHT;     // 右转

    // 叉乘为0，共线，需要区分直行和掉头
    // 通过点积判断：同向为直行，反向为掉头
    int dot = ax * bx + ay * by;
    if (dot > 0)  return TURN_STRAIGHT;    // 直行
    return TURN_UTURN;                      // 掉头
}

/**
 * @brief 判断起始路口转向方向（已知车辆当前朝向）
 * @param current_id 当前路口节点 ID（起始点）
 * @param next_id 下一个节点 ID
 * @param vehicle_angle 车辆当前朝向角度（度），正北 = 0°，顺时针增加
 * @return 转向常量: TURN_FOLLOW / TURN_STRAIGHT / TURN_LEFT / TURN_RIGHT / TURN_UTURN
 *
 * @details 适用于路径起点就在路口的情况
 *          通过车辆朝向角度构造"来车方向"向量，再与目标方向做叉乘判断
 */
int Dijkstra::get_turn_direction_from_heading(int current_id, int next_id, int vehicle_angle) {
    // 参数合法性检查
    if (current_id < 0 || current_id >= active_node_count ||
        next_id < 0 || next_id >= active_node_count) {
        return TURN_FOLLOW;
    }

    // 获取当前点和下一个点的坐标
    int cx, cy, nx, ny;
    get_node_coord(current_id, &cx, &cy);
    get_node_coord(next_id, &nx, &ny);

    // 向量B = next - current（目标方向）
    int bx = nx - cx;
    int by = ny - cy;

    // 从车辆角度构造向量A（来车方向）
    // 角度：正北=0°，顺时针，与 get_direction 一致
    // 单位向量：角度转弧度，再转方向分量
    float rad = vehicle_angle * 3.14159265f / 180.0f;
    // 正北(0°)→(0,-1)，正东(90°)→(1,0)，正南(180°)→(0,1)，正西(270°)→(-1,0)
    int ax = (int)(100 * sin(rad));   // 水平分量
    int ay = (int)(-100 * cos(rad));  // 垂直分量（y向下为正）

    // 叉乘
    int cross = ax * by - ay * bx;

    if (cross > 0)  return TURN_LEFT;      // 左转
    if (cross < 0)  return TURN_RIGHT;     // 右转

    // 共线，用点积区分直行和掉头
    int dot = ax * bx + ay * by;
    if (dot > 0)  return TURN_STRAIGHT;    // 直行
    return TURN_UTURN;                      // 掉头
}

/**
 * @brief 获取转向名称字符串
 * @param turn 转向常量
 * @return 可读的转向名称
 */
const char* Dijkstra::get_turn_name(int turn) {
    switch (turn) {
        case TURN_FOLLOW:    return "循迹";
        case TURN_STRAIGHT:  return "直行";
        case TURN_LEFT:      return "左转";
        case TURN_RIGHT:     return "右转";
        case TURN_UTURN:     return "掉头";
        default:             return "未知";
    }
}

// THU 节点坐标
// 坐标系：x 向右为正，y 向上为正；单位为标注图中的比例坐标。
// 用途：后续可通过 prev-current-next 三点叉乘判断左转/右转/直行。
static const int thu_coords[MAX_NODE_COUNT][2] = {
    {0,0}, {225,130}, {65,65}, {140,65}, {225,65}, {310,65}, {0,0},
    {225,0}, {385,0}, {140,-65}, {225,-65}, {310,-65}, {385,-65},
    {225,-130}, {310,-130}, {0,0}
};
// SUTD 节点坐标
static const int sutd_coords[MAX_NODE_COUNT][2] = {
    {0,0}, {0,-160}, {135,0}, {-125,-80}, {-125,0}, {0,0}, {0,145},
    {-65,-80}, {-65,0}, {0,75}, {0,-80}, {65,105}, {65,0}, {0,0}, {0,0}, {0,0}
};

/**
 * @brief 根据节点 ID 获取该节点在地图上的坐标
 * @param id 节点 ID（如 THU_NODE_TUSHUGUAN = 3）
 * @param x 输出参数，接收 x 坐标
 * @param y 输出参数，接收 y 坐标
 *
 * @note
 *   - 参数非法时，x 和 y 输出为 0
 *   - 根据 current_map 自动选择 THU 或 SUTD 的坐标数组
 */
void Dijkstra::get_node_coord(int id, int* x, int* y) {
    if (!x || !y) return;
    if (id < 0 || id >= active_node_count) { *x=0; *y=0; return; }
    const int (*coords)[2] = (current_map == MAP_SUTD) ? sutd_coords : thu_coords;
    *x = coords[id][0]; *y = coords[id][1];
}

/**
 * @brief 添加一条无向边到邻接表中
 * @param from 起点节点 ID
 * @param to 终点节点 ID
 * @param weight 边权重（可理解为两点间的距离）
 *
 * @details
 *   无向边意味着道路双向可通行，因此在邻接表中同时添加：
 *   - graph[from] 中添加到 to 的边
 *   - graph[to] 中添加到 from 的边
 *
 *   例如：add_undirected_edge(3, 4, 85) 表示
 *   图书馆(3) 和 苏世民书院(4) 之间有一条双向道路，距离 85
 */
void Dijkstra::add_undirected_edge(int from, int to, int weight) {
    if (from <= 0 || from >= active_node_count || to <= 0 || to >= active_node_count) return;
    graph[from].emplace_back(to, weight); graph[to].emplace_back(from, weight);
}

/**
 * @brief 临时阻塞一条边（道路无向，两个方向都标记）
 * @details 不删除边、不改权重，只置blocked标志；find_shortest_path()据此跳过。
 *          用 unblock_edge() 可恢复，供避障绕行后道路重新可用时使用。
 */
void Dijkstra::block_edge(int from, int to) {
    if (from <= 0 || from >= active_node_count || to <= 0 || to >= active_node_count) return;
    for (Edge& e : graph[from]) if (e.to == to) e.blocked = true;
    for (Edge& e : graph[to]) if (e.to == from) e.blocked = true;
}

void Dijkstra::unblock_edge(int from, int to) {
    if (from <= 0 || from >= active_node_count || to <= 0 || to >= active_node_count) return;
    for (Edge& e : graph[from]) if (e.to == to) e.blocked = false;
    for (Edge& e : graph[to]) if (e.to == from) e.blocked = false;
}

bool Dijkstra::is_edge_blocked(int from, int to) {
    if (from <= 0 || from >= active_node_count || to <= 0 || to >= active_node_count) return false;
    for (const Edge& e : graph[from]) if (e.to == to) return e.blocked;
    return false;
}

/**
 * @brief 计算 start 到 end 的最短距离
 * @details
 *   使用朴素 Dijkstra，复杂度 O(V^2)。本项目节点数很少，没必要引入优先队列。
 *   计算完成后：
 *   - dist[end] 是最短距离；
 *   - prev[] 保存最短路径前驱，供 get_path() 回溯路径。
 *   被 block_edge() 标记的边跳过，不参与松弛（等效于临时从图中移除该边）。
 */
int Dijkstra::find_shortest_path(int start, int end) {
    if (start <= 0 || start >= active_node_count || end <= 0 || end >= active_node_count) return -1;
    for (int i=0;i<active_node_count;i++){dist[i]=INF;prev[i]=-1;visited[i]=false;}
    dist[start]=0;
    for (int i=0;i<active_node_count;i++){
        int u=-1, md=INF;
        for(int j=1;j<active_node_count;j++) if(!visited[j]&&dist[j]<md){md=dist[j];u=j;}
        if(u==-1)break; visited[u]=true;
        for(const Edge& e:graph[u]) if(!e.blocked&&!visited[e.to]&&dist[u]+e.weight<dist[e.to]){dist[e.to]=dist[u]+e.weight;prev[e.to]=u;}
    }
    return dist[end]==INF?-1:dist[end];
}

/**
 * @brief 获取 start 到 end 的最短路径节点序列
 * @param path 输出数组，建议长度至少 MAX_PATH_LEN
 * @return 路径节点数；返回 0 表示不可达或参数非法
 */
int Dijkstra::get_path(int start, int end, int* path) {
    if(!path) return 0;
    if(find_shortest_path(start,end)==-1) return 0;
    int tmp[MAX_PATH_LEN], n=0, cur=end;
    while(cur!=-1 && n<MAX_PATH_LEN){tmp[n++]=cur;cur=prev[cur];}
    for(int i=0;i<n;i++) path[i]=tmp[n-1-i];
    return n;
}

/**
 * @brief 打印最短路径，调试用
 */
void Dijkstra::print_path(int start, int end) {
    int path[MAX_PATH_LEN]; int n=get_path(start,end,path);
    if(n==0){printf("[%s] No path from %s to %s\n",get_map_name(),get_node_name(start),get_node_name(end));return;}
    printf("[%s] Shortest path from %s to %s (distance: %d):\n",get_map_name(),get_node_name(start),get_node_name(end),dist[end]);
    for(int i=0;i<n;i++){printf("%s",get_node_name(path[i])); if(i<n-1)printf(" -> ");}
    printf("\n");
}

/**
 * @brief 获取节点完整信息
 * @param id 节点 ID
 * @return NavNodeInfo 包含名称、坐标、是否路口等信息
 */
NavNodeInfo Dijkstra::get_node_info(int id) {
    if (id < 0 || id >= active_node_count) {
        return NavNodeInfo(0, "Unknown", 0, 0, false);
    }
    int x, y;
    get_node_coord(id, &x, &y);
    return NavNodeInfo(id, node_names[id], x, y, is_intersection_node(id));
}

/**
 * @brief 测试最短路径（独立调试接口）
 * @param start 起点 ID
 * @param end 终点 ID
 * @param result 输出结果结构体
 * @return true 路径存在，false 不可达或参数错误
 * @details 不依赖当前 Tag 和状态机，用于单独测试地图连通性
 */
bool Dijkstra::test_shortest_path(int start, int end, NavResult& result) {
    // 清空结果
    result = NavResult();

    // 参数检查
    if (start <= 0 || start >= active_node_count || end <= 0 || end >= active_node_count) {
        return false;
    }

    // 计算最短路径
    int dist = find_shortest_path(start, end);
    if (dist < 0) {
        return false;
    }

    // 填充基本信息
    result.valid = true;
    result.map_id = current_map;
    result.map_name = get_map_name();
    result.shortest_distance = dist;

    // 获取路径
    result.path_len = get_path(start, end, result.path);

    // 填充起点和终点信息
    result.current_id = start;
    result.current_name = get_node_name(start);
    result.target_id = end;
    result.target_name = get_node_name(end);

    // 如果有下一个节点，填充 next 信息
    if (result.path_len >= 2) {
        result.next_id = result.path[1];
        result.next_name = get_node_name(result.next_id);
        get_node_coord(result.next_id, &result.next_x, &result.next_y);
    }

    // 获取当前节点坐标
    get_node_coord(start, &result.current_x, &result.current_y);
    result.current_is_intersection = is_intersection_node(start);

    return true;
}

/**
 * @brief 更新导航状态（主入口）
 * @param input 导航输入参数（当前位置、目标、角度等）
 * @param output 导航输出结果（路径、转向决策等）
 * @return true 计算成功，false 失败
 * @details 这是 Dijkstra 模块的主入口，状态机每帧调用此函数获取导航决策
 */
bool Dijkstra::update_nav(const NavUpdateInput& input, NavResult& output) {
    // 清空输出
    output = NavResult();

    // 检查并切换地图
    if (input.map_id != current_map && input.map_id != MAP_NONE) {
        set_map(input.map_id);
    }

    // 参数检查
    if (input.current_id <= 0 || input.current_id >= active_node_count ||
        input.target_id <= 0 || input.target_id >= active_node_count) {
        return false;
    }

    // 填充基本信息
    output.valid = true;
    output.map_id = current_map;
    output.map_name = get_map_name();
    output.current_id = input.current_id;
    output.current_name = get_node_name(input.current_id);
    output.target_id = input.target_id;
    output.target_name = get_node_name(input.target_id);
    output.tag_angle = input.tag_angle;

    // 方向信息直接使用 Tag 检测模块计算好的值
    output.dir_ns = tag_dir_ns;
    output.dir_ew = tag_dir_ew;
    output.dir_ns_name = tag_dir_ns_name;
    output.dir_ew_name = tag_dir_ew_name;

    // 计算最短路径
    int dist = find_shortest_path(input.current_id, input.target_id);
    if (dist < 0) {
        output.valid = false;
        return false;
    }
    output.shortest_distance = dist;

    // 获取路径
    output.path_len = get_path(input.current_id, input.target_id, output.path);
    if (output.path_len == 0) {
        output.valid = false;
        return false;
    }

    // 查找当前节点在路径中的位置
    int current_idx = -1;
    for (int i = 0; i < output.path_len; i++) {
        if (output.path[i] == input.current_id) {
            current_idx = i;
            break;
        }
    }

    if (current_idx < 0) {
        output.valid = false;
        return false;
    }

    // 填充 prev 信息
    if (current_idx > 0) {
        output.prev_id = output.path[current_idx - 1];
        output.prev_name = get_node_name(output.prev_id);
        get_node_coord(output.prev_id, &output.prev_x, &output.prev_y);
    }

    // 填充 next 信息
    if (current_idx < output.path_len - 1) {
        output.next_id = output.path[current_idx + 1];
        output.next_name = get_node_name(output.next_id);
        get_node_coord(output.next_id, &output.next_x, &output.next_y);
    }

    // 获取当前节点信息
    get_node_coord(input.current_id, &output.current_x, &output.current_y);
    output.current_is_intersection = is_intersection_node(input.current_id);

    // 判断是否到达终点
    if (input.current_id == input.target_id) {
        output.strategy = STRATEGY_STOP;
        output.turn_action = TURN_FOLLOW;
        return true;
    }

    // 判断转向（如果在路口且有下一个节点）
    if (output.current_is_intersection && output.next_id > 0) {
        int turn;
        if (output.prev_id > 0) {
            // 有前一个节点，用三点判断
            turn = get_turn_direction(output.prev_id, input.current_id, output.next_id);
        } else {
            // 起点，用车辆朝向判断
            turn = get_turn_direction_from_heading(input.current_id, output.next_id, (int)input.tag_angle);
        }
        output.turn_action = turn;

        // 计算叉乘和点积（用于调试）
        if (output.prev_id > 0) {
            int px, py, cx, cy, nx, ny;
            get_node_coord(output.prev_id, &px, &py);
            get_node_coord(input.current_id, &cx, &cy);
            get_node_coord(output.next_id, &nx, &ny);
            int ax = cx - px, ay = cy - py;
            int bx = nx - cx, by = ny - cy;
            output.cross_value = ax * by - ay * bx;
            output.dot_value = ax * bx + ay * by;
        }

        // 根据转向决定策略
        if (turn == TURN_FOLLOW) {
            output.strategy = STRATEGY_FOLLOW;
        } else {
            output.strategy = STRATEGY_LINE;  // 需要转向时，先走直线到路口中心
        }
    } else {
        // 非路口，循迹行驶
        output.strategy = STRATEGY_FOLLOW;
        output.turn_action = TURN_FOLLOW;
    }

    return true;
}

/**
 * @brief 查表获取期望朝向（用于确定车辆在路口的朝向）
 * @param prev_id 上一个节点 ID
 * @param current_id 当前路口 ID
 * @return 期望朝向角度（0-360度），-1表示未知
 * @details 根据 prev->current 的方向，确定车辆在当前路口应该面向的方向
 *          例如：从图书馆(3)到苏世民书院(4)，应该面朝东(90度)
 */
int Dijkstra::get_expected_heading(int prev_id, int current_id) {
    // 参数检查
    if (prev_id <= 0 || prev_id >= active_node_count ||
        current_id <= 0 || current_id >= active_node_count) {
        return -1;
    }

    // 获取坐标
    int px, py, cx, cy;
    get_node_coord(prev_id, &px, &py);
    get_node_coord(current_id, &cx, &cy);

    // 计算方向向量
    int dx = cx - px;  // 水平分量
    int dy = cy - py;  // 垂直分量

    // 计算角度（atan2返回弧度，转换为度）
    // atan2(dy, dx) 返回 -π 到 π 的角度
    // 我们需要转换为 0-360 度，正北为0度，顺时针增加
    float angle_rad = atan2(dy, dx);
    int angle_deg = (int)(angle_rad * 180.0f / 3.14159265f);

    // 转换为正北为0度的坐标系
    // 正东(90) -> 90, 正北(0) -> 0, 正西(270) -> 270, 正南(180) -> 180
    angle_deg = 90 - angle_deg;

    // 规范化到 0-360
    angle_deg %= 360;
    if (angle_deg < 0) angle_deg += 360;

    return angle_deg;
}

/**
 * @brief 查表获取转向动作（用于确定从当前路口往哪转）
 * @param current_id 当前路口 ID
 * @param next_id 下一个节点 ID
 * @return 转向动作：TURN_LEFT/RIGHT/STRAIGHT/UTURN
 * @details 根据 current->next 的方向，确定应该往哪转
 *          这个函数假设车辆已经正确朝向（通过 get_expected_heading 确定）
 */
int Dijkstra::get_intersection_turn(int current_id, int next_id) {
    // 参数检查
    if (current_id <= 0 || current_id >= active_node_count ||
        next_id <= 0 || next_id >= active_node_count) {
        return TURN_FOLLOW;
    }

    // 获取当前节点和下一个节点的坐标
    int cx, cy, nx, ny;
    get_node_coord(current_id, &cx, &cy);
    get_node_coord(next_id, &nx, &ny);

    // 计算目标方向向量
    int dx = nx - cx;
    int dy = ny - cy;

    // 计算目标朝向角度
    float angle_rad = atan2(dy, dx);
    int target_angle = (int)(angle_rad * 180.0f / 3.14159265f);
    target_angle = 90 - target_angle;
    target_angle %= 360;
    if (target_angle < 0) target_angle += 360;

    // 获取当前期望朝向（假设车辆已经正确朝向进入路口）
    // 这里简化处理：假设车辆朝向与出路口方向的关系决定转向
    // 实际上应该结合车辆的实际朝向来判断

    // 简化逻辑：根据目标角度判断大致方向
    // 0-45 或 315-360: 北
    // 45-135: 东
    // 135-225: 南
    // 225-315: 西

    if ((target_angle >= 315) || (target_angle < 45)) {
        // 目标在北边，如果当前也是朝北，就是直行
        return TURN_STRAIGHT;
    } else if (target_angle >= 45 && target_angle < 135) {
        // 目标在东边
        return TURN_RIGHT;  // 假设右转可以到东
    } else if (target_angle >= 135 && target_angle < 225) {
        // 目标在南边
        return TURN_UTURN;  // 需要掉头
    } else {
        // 目标在西边
        return TURN_LEFT;   // 假设左转可以到西
    }
}

/**
 * @brief 获取路段的期望方位（用于非路口路段的方位对比）
 * @param from_id 起点 ID
 * @param to_id 终点 ID
 * @return 期望方位角（0-360度），-1表示错误
 */
int Dijkstra::get_segment_heading(int from_id, int to_id) {
    // 这和 get_expected_heading 实现相同
    return get_expected_heading(from_id, to_id);
}

/**
 * @brief 判断两个节点是否相邻（有边直接连接）
 * @param id1 节点1 ID
 * @param id2 节点2 ID
 * @return true相邻，false不相邻
 * @details 用于验证检测到的Tag是否是当前节点的邻近点，防止误判
 */
bool Dijkstra::is_neighbor(int id1, int id2) {
    // 参数检查
    if (id1 <= 0 || id1 >= active_node_count ||
        id2 <= 0 || id2 >= active_node_count) {
        return false;
    }
    
    // 遍历id1的邻接表，检查是否有id2
    for (const Edge& e : graph[id1]) {
        if (e.to == id2) {
            return true;
        }
    }
    
    return false;
}
