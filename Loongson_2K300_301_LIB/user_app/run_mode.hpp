/**
 * @file run_mode.hpp
 * @brief 小车运行模式控制模块头文件
 * @details 根据导航状态和动作类型决定是否运行以及运行速度
 *          以及根据动作类型处理中线偏移
 */

#ifndef __RUN_MODE_HPP
#define __RUN_MODE_HPP

#include "include.hpp"

#define TURN_MILE_LIMIT     2500    // 转向状态维持的里程（编码器脉冲数
#define UTURN_MILE_LIMIT_1    4000    // 掉头第一段（前进右弧）里程；单轮转动平均计数4/周期→1.875s
#define UTURN_MILE_LIMIT_2    4000    // 掉头第二段（倒车左弧）里程；切换时清零后独立计程

// 外部变量声明
// is_uturning 已移入5ms控制线程私有状态，跨线程读取请使用 Control_Is_Uturning()

/**
 * @brief 获取小车的运行速度
 * @param target_speed 目标速度
 * @return 实际运行速度（根据导航状态和动作类型决定）
 * @details
 * - 如果导航状态为 NAV_STATE_IDLE / NAV_STATE_WAITING / NAV_STATE_ARRIVED，返回 0
 * - 如果动作类型为 ACTION_NONE / ACTION_STOP，返回 0
 * - 其他情况返回 target_speed
 */
int get_run_speed(int target_speed);

/**
 * @brief 判断小车是否应该运行
 * @return true 表示应该运行，false 表示停止
 */
bool should_run(void);

/**
 * @brief 计算循迹左边界标志（主线程调用，结果经命令快照下发5ms线程）
 * @return 1=沿左边界循迹（第一个路口且无来向信息时），0=正常居中循迹
 * @details 原dir_control(5ms)中的判定逻辑移至主线程，消除5ms对nav_fsm的直接访问
 */
int compute_follow_left(void);

/**
 * @brief 中线偏置操作，用于在转向时调整循迹中线
 * @details 根据动作类型进行补线：
 *          - ACTION_TURN_LEFT:  从第30列找白-白-黑-黑跳变点，从底部中心点向该点拉线
 *          - ACTION_TURN_RIGHT: 从第130列找白-白-黑-黑跳变点，从底部中心点向该点拉线
 *          - ACTION_UTURN:      不操作中线
 *          - 其他:               正常中线，无操作
 */
void midline_offset(void);

/**
 * @brief 更新运行模式状态（包括掉头标志位）
 * @details 每帧调用，根据当前动作类型更新：
 *          - ACTION_UTURN:     设置 is_uturning = 1
 *          - ACTION_TURN_LEFT: 中线向左补线
 *          - ACTION_TURN_RIGHT:中线向右补线
 *          - 其他:             is_uturning = 0，正常中线
 */
void update_run_mode(void);

/**
 * @brief 设置掉头完成（外部调用，掉头动作结束后调用）
 * @details 将 is_uturning 标志位清零
 */
void set_uturn_done(void);

/**
 * @brief 获取当前是否正在掉头
 * @return true 表示正在掉头，false 表示未掉头
 */
bool is_uturn_in_progress(void);

#endif /* __RUN_MODE_HPP */
