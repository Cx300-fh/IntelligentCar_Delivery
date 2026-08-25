#ifndef __BORDER_HPP
#define __BORDER_HPP

#include "include.hpp"

//================================================================================
// 宏定义
//================================================================================
#define STEP_MAX 240  // 最大巡线步数

#define Image_BLACK  0    // 二值化后黑色为0（赛道）
#define Image_WHITE  255  // 二值化后白色为255（背景）

//================================================================================
// 结构体定义
//================================================================================
typedef struct Border_message
{
    uint8_t left_border;    // 左边界
    uint8_t right_border;   // 右边界
    uint8_t center_line;    // 中线
    uint8_t road_width;     // 道路宽度
} Border_message;

//================================================================================
// 外部变量声明
//================================================================================
extern uint8_t step_max;                    // 最大巡线步数
extern int top_row;                         // 顶部截止行
extern int left_point_count;                // 左边界点数
extern int right_point_count;               // 右边界点数

extern Border_message border_msg[120];      // 边界信息数组

extern uint8_t start_point_left[2];         // 左起始点 [col, row]
extern uint8_t start_point_right[2];        // 右起始点 [col, row]

extern uint8_t left_line_points[STEP_MAX][2];    // 左边界点数组 [col, row]
extern uint8_t right_line_points[STEP_MAX][2];   // 右边界点数组 [col, row]

extern const uint8_t SCAN_START_ROW;            // 扫描起始行

//================================================================================
// 函数声明
//================================================================================
// 获取起始点
uint8_t get_start_point(uint8_t scan_row, uint8_t last_col);

// 迷宫法巡线
void maze_find_line(uint8_t left_start[], uint8_t right_start[],
                    uint8_t left_points[][2], uint8_t right_points[][2]);

// 边界分析
void border_analyse(void);

// 边界提取主函数
void Boundary_Extract(void);

#endif
