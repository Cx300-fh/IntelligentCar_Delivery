/**
 * @file run_mode.cpp
 * @brief 小车运行模式控制模块实现文件
 */

#include "run_mode.hpp"
#include "navigation.hpp"
#include "camera.hpp"
#include "border.hpp"
#include "control.hpp"

// 变量定义
// is_uturning 已移入5ms控制线程私有状态（control.cpp g_uturn_stage），跨线程读取用 Control_Is_Uturning()

/**
 * @brief 计算循迹左边界标志（主线程调用）
 * @details 原dir_control(5ms线程)中的判定移至此处：
 *          无来向信息或首个节点时，若下一节点是路口则沿左边界走
 */
int compute_follow_left(void)
{
    const NavStatus& nav_status = nav_fsm.get_status();

    if (nav_status.has_prev_info == false || nav_status.is_first_node == true)
    {
        Dijkstra* dijkstra = nav_fsm.get_dijkstra();
        if (dijkstra != nullptr) {
            bool is_intersection = dijkstra->is_intersection_node(nav_status.current_id);
            if (nav_status.next_id != 0 && is_intersection == true)
            {
                return 1;  // 遇到的第一个路口默认沿着左边界走，能左转就左转，不能左转就直行
            }
        }
    }
    return 0;
}

// 补线配置
#define LEFT_SCAN_COL       30      // 左转扫描列
#define RIGHT_SCAN_COL      130     // 右转扫描列
#define CENTER_COL          (IMAGE_WIDTH / 2)   // 图像中心列 (80)
#define BOTTOM_ROW          (IMAGE_HEIGHT - 1)  // 图像底部行 (119)

/**
 * @brief 获取小车的运行速度
 */
int get_run_speed(int target_speed)
{
    // 获取当前导航状态
    NavState state = nav_fsm.get_state();

    // 如果处于 IDLE / WAITING / ARRIVED 状态，停止运行
    if (state == NAV_STATE_IDLE ||
        state == NAV_STATE_WAITING ||
        state == NAV_STATE_ARRIVED) {
        return 0;
    }

    // 获取当前动作类型
    ActionType action = nav_fsm.get_action();

    // 如果动作类型为 NONE 或 STOP，停止运行
    if (action == ACTION_NONE || action == ACTION_STOP) {
        return 0;
    }

    // 其他情况，使用目标速度运行
    return target_speed;
}

/**
 * @brief 判断小车是否应该运行
 */
bool should_run(void)
{
    // 获取当前导航状态
    NavState state = nav_fsm.get_state();

    // 如果处于 IDLE / WAITING / ARRIVED 状态，不运行
    if (state == NAV_STATE_IDLE ||
        state == NAV_STATE_WAITING ||
        state == NAV_STATE_ARRIVED) {
        return false;
    }

    // 获取当前动作类型
    ActionType action = nav_fsm.get_action();

    // 如果动作类型为 NONE 或 STOP，不运行
    if (action == ACTION_NONE || action == ACTION_STOP) {
        return false;
    }

    // 其他情况，可以运行
    return true;
}

/**
 * @brief 检测跳变点：白-白-黑-黑（从下向上扫描）
 * @param col 扫描列
 * @return 跳变点行坐标，未找到返回-1
 * @details 跳变特征：当前点黑，下面两行白点，上面一行黑点
 */
static int find_edge_jump_point(int col)
{
    // 从下向上扫描（从底部向上，留出足够边界）
    for (int row = BOTTOM_ROW - 2; row >= 2; row--) {
        uint8_t curr = image_binary.at<uchar>(row, col);       // 当前点
        uint8_t down1 = image_binary.at<uchar>(row + 1, col);  // 下一行
        uint8_t down2 = image_binary.at<uchar>(row + 2, col);  // 下两行
        uint8_t up1 = image_binary.at<uchar>(row - 1, col);    // 上一行

        // 白-白-黑-黑跳变
        if (curr == Image_BLACK && down1 == Image_WHITE &&
            down2 == Image_WHITE && up1 == Image_BLACK) {
            return row;
        }
    }
    return -1;  // 未找到跳变点
}

/**
 * @brief 从底部中心点向目标点拉线，重新构造中线
 * @param target_col 目标点列坐标
 * @param target_row 目标点行坐标
 * @details 从底部中心点(CENTER_COL, BOTTOM_ROW)向目标点拉直线，
 *          更新border_msg中从第0行到BOTTOM行的中线位置
 */
static void construct_midline(int target_col, int target_row)
{
    // 底部中心点坐标
    int start_col = CENTER_COL;
    int start_row = BOTTOM_ROW;

    // 计算斜率 (col变化 / row变化)
    // 注意：row向上递减，所以row差值为负
    double slope = (double)(target_col - start_col) / (double)(target_row - start_row);

    // 从第0行到BOTTOM_ROW，计算每一行的中线位置
    for (int row = 0; row <= BOTTOM_ROW; row++) {
        // 根据直线方程计算该row对应的col
        // col = start_col + slope * (row - start_row)
        int center_col = (int)(start_col + slope * (row - start_row) + 0.5);

        // 限幅确保不越界
        if (center_col < 0) center_col = 0;
        if (center_col >= IMAGE_WIDTH) center_col = IMAGE_WIDTH - 1;

        // 更新中线
        border_msg[row].center_line = (uint8_t)center_col;
    }
}

/**
 * @brief 中线偏置操作，用于在转向时调整循迹中线
 */
void midline_offset(void)
{
    // 如果正在掉头，不操作中线
    if (Control_Is_Uturning()) {
        return;
    }

    // 获取当前动作类型
    ActionType action = nav_fsm.get_action();
    if (Control_Get_Mile() >= TURN_MILE_LIMIT) action = ACTION_FOLLOW;  // 里程清零后又达到限值，停止补线

    int scan_col = -1;

    switch (action) {
        case ACTION_TURN_LEFT:
            // 左转：从左侧第30列扫描跳变点
            scan_col = LEFT_SCAN_COL;
            break;

        case ACTION_TURN_RIGHT:
            // 右转：从右侧第130列扫描跳变点
            scan_col = RIGHT_SCAN_COL;
            break;

        default:
            // 正常循迹，无偏移
            return;
    }

    if (Control_Get_Mile() >= TURN_MILE_LIMIT) scan_col = -1;  // 里程清零后又达到限值，停止补线

    if (scan_col == -1) {
        return;  // 未设置扫描列，直接返回
    }

    // 在指定列寻找跳变点
    int jump_row = find_edge_jump_point(scan_col);

    // 如果找到跳变点，从底部中心点向该点拉线，重新构造中线
    // if (jump_row >= 0) {
    //     // 找到跳变点，从底部中心点向该点拉线，重新构造中线
    //     construct_midline(scan_col, jump_row);
    //     return;
    // }
}

/**
 * @brief 更新运行模式状态
 * @deprecated 掉头标志位已移入5ms控制线程自持（见control.cpp dir_control），
 *             本函数保留空实现以兼容旧调用点
 */
void update_run_mode(void)
{
    // 掉头状态机（进入/阶段推进/退出）已全部在5ms控制线程内闭环，
    // 主线程不再写掉头标志位
}

/**
 * @brief 设置掉头完成
 * @deprecated 掉头阶段由5ms线程按里程阈值自行退出，本函数保留空实现以兼容旧调用点
 */
void set_uturn_done(void)
{
}

/**
 * @brief 获取当前是否正在掉头
 */
bool is_uturn_in_progress(void)
{
    return Control_Is_Uturning();
}

/**
 * @brief 掉头状态机处理
 * @details 在掉头过程中，根据当前偏航角判断是否完成掉头
 */
void uturning()
{
    // 状态机（掉头阶段经控制反馈快照读取）
    if (!Control_Is_Uturning())
    {
        angle_yaw = 0;
    }
    else if (Control_Get_Telemetry().uturn_stage == 1)
    {
        // 左转
        if (angle_yaw > 200)
        {
            set_uturn_done();  // 掉头完成（空实现，实际退出由5ms线程里程阈值决定）
        }
    }
}
