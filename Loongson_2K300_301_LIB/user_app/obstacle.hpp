#ifndef __OBSTACLE_HPP
#define __OBSTACLE_HPP

#include "include.hpp"

//================================================================================
// 宏定义
//================================================================================

//================================================================================
// 结构体定义
//================================================================================

//================================================================================
// 外部变量声明（避障状态机暂时注释，需要时取消注释）
//================================================================================
extern uint8_t flag_obstacle;            // 避障状态标志
// extern lq_i2c_lsm6dsr lsm6dsr;           // 陀螺仪设备
// extern lq_i2c_vl53l0x vl53l0x;           // 激光测距设备
extern int16_t ax, ay, az, gx, gy, gz;   // 陀螺仪数据变量
extern float angle_yaw;                  // 当前yaw角（角度）
extern float Gyro_z;                     // 偏航角速度

// 色块检测结果图像（用于图传）
extern cv::Mat image_color_mask;         // 合并的红黄色块二值化图
extern cv::Mat image_red_mask;           // 红色色块二值化图
extern cv::Mat image_yellow_mask;        // 黄色色块二值化图

// 色块检测结果
extern bool obstacle_detected;           // 是否检测到障碍物色块
extern bool red_block_detected;          // 红色色块检测结果
extern bool yellow_block_detected;       // 黄色色块检测结果

//================================================================================
// 函数声明
//================================================================================

/**
 * @brief 色块识别函数（通用）
 * @param image_frame 输入图像帧（BGR格式）
 * @param lower_rgb RGB下限阈值 (B, G, R)
 * @param upper_rgb RGB上限阈值 (B, G, R)
 * @param min_area 最小轮廓面积阈值
 * @return true-检测到色块, false-未检测到
 */
bool detect_color_block(const cv::Mat& image_frame,
                        const cv::Scalar& lower_rgb,
                        const cv::Scalar& upper_rgb,
                        double min_area);

/**
 * @brief 检测红色色块
 * @param image_frame 输入图像帧（BGR格式）
 * @return true-检测到色块, false-未检测到
 */
bool detect_red_block(const cv::Mat& image_frame);

/**
 * @brief 检测黄色色块
 * @param image_frame 输入图像帧（BGR格式）
 * @return true-检测到色块, false-未检测到
 */
bool detect_yellow_block(const cv::Mat& image_frame);

/**
 * @brief 检测红色或黄色色块
 * @param image_frame 输入图像帧（BGR格式）
 * @details 同时更新 obstacle_detected、red_block_detected、yellow_block_detected 全局变量
 *          并生成合并的二值化图像 image_color_mask 用于图传
 */
void detect_obstacle(const cv::Mat& image_frame);

/**
 * @brief 获取色块检测结果图（用于图传）
 * @return 色块二值化图像
 * @note 红色和黄色区域为白色(255)，其他区域为黑色(0)
 */
cv::Mat get_color_mask_image(void);

/**
 * @brief 更新yaw角（根据陀螺仪Z轴角速度积分）
 * @param gz 陀螺仪Z轴原始数据
 */
void update_angle_yaw(float gz);

/**
 * @brief 避障状态机主函数
 * @details
 *   flag_obstacle状态转换:
 *   0 - 正常巡线
 *   1 - 检测到障碍物，开始左转避障
 *   2 - 左转中，等待yaw>OBSTACLE_YAW_LEFT
 *   3 - 左转完成，开始右转
 *   4 - 右转完成，回到巡线
 */
void obstacle_avoidance(void);

/**
 * @brief 避障状态机转向输出计算
 * @param flag_obstacle 避障状态标志
 * @return 转向输出值
 */
int16_t obstacle_get_ele_out(uint8_t flag_obstacle);

#endif
