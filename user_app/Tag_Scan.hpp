/**
 * @file Tag_Scan.hpp
 * @brief AprilTag标签识别模块头文件
 * @details 角度定义（车辆坐标系）：
 *          - 正前方为0°
 *          - 顺时针增加（右偏为正）
 *          - 左偏从360递减
 *          - 范围：0-360°（无负数）
 */

#ifndef __TAG_SCAN_HPP
#define __TAG_SCAN_HPP

#include "include.hpp"

/*============================================================================
 *                              方向常量定义
 *============================================================================*/
// 南北方向
#define DIR_NS_NORTH    0
#define DIR_NS_SOUTH    1
#define DIR_NS_PENDING  2

// 东西方向
#define DIR_EW_EAST     0
#define DIR_EW_WEST     1
#define DIR_EW_PENDING  2

/*============================================================================
 *                              全局变量声明
 *============================================================================*/
extern int tag_id;                          // 标签ID (-1表示未检测到)
extern float tag_angle;                     // 标签相对角度 (度, 0-360°, 正前方为0°)
extern float tag_center_x;                  // 标签中心X坐标
extern float tag_center_y;                  // 标签中心Y坐标
extern int det_corners[4][2];               // AprilTag 四个角点坐标
extern bool det_found;                      // 是否检测到标签

// Tag方向信息（在Tag_Detect中计算，后续直接使用）
extern int tag_dir_ns;                      // 南北方向：0-北, 1-南, 2-未检测
extern int tag_dir_ew;                      // 东西方向：0-东, 1-西, 2-未检测
extern const char* tag_dir_ns_name;         // 南北方向名称："N"/"S"/"P"
extern const char* tag_dir_ew_name;         // 东西方向名称："E"/"W"/"P"

/*============================================================================
 *                              函数声明
 *============================================================================*/
void Tag_Scan_Init(void);                   // 标签识别初始化
void Tag_Scan_Process(void);                // 标签识别处理

#endif /* __TAG_SCAN_HPP */
