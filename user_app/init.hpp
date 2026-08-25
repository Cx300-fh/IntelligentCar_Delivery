/**
 * @file init.hpp
 * @brief 系统初始化模块头文件
 * @details 定义UDP配置、摄像头初始化相关变量及函数声明
 */

#ifndef __INIT_HPP
#define __INIT_HPP

#include "include.hpp"

/*============================================================================
 *                              UDP 配置参数
 *============================================================================*/
extern const std::string UDP_IP;        // UDP服务器IP地址
extern const uint16_t UDP_PORT;         // UDP端口号
extern const uint16_t IMAGE_FPS;        // 图像帧率
extern const uint8_t JPEG_QUALITY;      // JPEG压缩质量

/*============================================================================
 *                              初始化状态
 *============================================================================*/
extern uint8_t image_init_ok;           // 图像模块初始化状态: 1-成功, 0-失败

/*============================================================================
 *                              内部对象声明
 *============================================================================*/
extern lq_udp_client udp_client;        // UDP客户端对象
extern lq_camera_ex cam;                // 摄像头对象
extern uint32_t frame_count;            // 帧计数器
extern std::chrono::high_resolution_clock::time_point start_time;  // 起始时间点

/*============================================================================
 *                              函数声明
 *============================================================================*/
/**
 * @brief 摄像头与UDP初始化
 */
void Image_Init(void);

/**
 * @brief 系统总初始化
 */
void All_Init(void);

#endif /* __INIT_HPP */
