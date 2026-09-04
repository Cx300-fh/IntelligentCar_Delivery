/**
 * @file    init.cpp
 * @brief   用户应用层初始化模块 - UDP图像传输初始化
 * @details 本文件实现UDP图传相关的初始化功能:
 *          - 定义UDP目标地址、端口及图像参数配置
 *          - 创建UDP客户端和摄像头全局对象
 *          - 提供图像模块初始化函数 Image_Init()
 *          - 提供系统总初始化函数 All_Init()
 */

#include "init.hpp"

/*============================================================================*/
/*                              全局配置参数                                    */
/*============================================================================*/

// 摄像头参数配置
const uint16_t IMAGE_FPS = 30;               // 图像帧率

// UDP目标地址和端口配置
const std::string UDP_IP = "10.99.90.240";    // UDP目标IP地址
const uint16_t UDP_PORT = 8888;              // UDP端口号
const uint8_t JPEG_QUALITY = 50;             // JPEG编码质量 (1-100)

/*============================================================================*/
/*                              全局对象定义                                    */
/*============================================================================*/

lq_udp_client udp_client(UDP_IP, UDP_PORT);  // UDP客户端对象，用于发送图像数据
lq_camera_ex  cam(IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_FPS, LQ_CAMERA_0CPU_MJPG);  // 摄像头对象

/*============================================================================*/
/*                              全局变量定义                                    */
/*============================================================================*/

uint32_t frame_count = 0;                    // 帧计数器，用于计算FPS
std::chrono::high_resolution_clock::time_point start_time;  // FPS统计起始时间点
uint8_t image_init_ok = 0;                   // 初始化状态: 1-成功, 0-失败

/*============================================================================*/
/*                              函数实现                                       */
/*============================================================================*/

/**
 * @brief   摄像头与UDP初始化
 * @param   none
 * @return  none
 * @note    初始化UDP客户端和摄像头，结果存入 image_init_ok (1-成功, 0-失败)
 */
void Image_Init(void)
{
    /* 检查摄像头是否打开成功 */
    if (!cam.is_cam_opened())
    {
        printf("ERROR: 打开摄像头失败!\n");
        image_init_ok = 0;
        return;
    }
    printf("摄像头参数: %dx%d @ %dfps\n", cam.get_camera_width(), cam.get_camera_height(), cam.get_camera_fps());
    printf("图传开始... 按下 Ctrl+C 停止\n");

    /* 设置初始化成功标志 */
    image_init_ok = 1;
}

/**
 * @brief   系统总初始化
 * @param   none
 * @return  none
 * @note    调用各模块初始化函数，完成系统启动准备工作
 */
void All_Init(void)
{
    WiFi_Manager_Init(); // WiFi管理模块初始化（从配置文件或屏幕配置连接WiFi）

    Image_Init();       // 图像模块初始化
    Tag_Scan_Init();    // 标签识别初始化
    HX711_Init();       // HX711称重模块初始化（GPIO+开机去皮+启动100ms服务定时器）

    // 初始化FPS统计
    frame_count = 0;  // 帧计数清零
    start_time = std::chrono::high_resolution_clock::now();  // 记录开始时间

    // Dijkstra算法测试
    printf("\n=========================================\n");
    printf("  Dijkstra Algorithm Test\n");
    printf("=========================================\n");
    Dijkstra dijkstra;
    // 测试1：校医院到东大操场
    dijkstra.print_path(THU_NODE_XIAOYIYUAN, THU_NODE_DONGDACAOCHANG);
    printf("=========================================\n\n");
}
