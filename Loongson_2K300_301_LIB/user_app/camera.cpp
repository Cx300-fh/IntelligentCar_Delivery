/**
 * @file camera.cpp
 * @brief 图像采集与处理模块
 * @details 实现摄像头图像获取、灰度转换、二值化处理等功能
 */

#include "camera.hpp"

/*============================================================================
 *                              全局变量定义
 *============================================================================*/
cv::Mat image_frame;                // 原始图像（用于绘制彩色标注作图传）
cv::Mat image_gray;                 // 灰度图像（用于检测和计算）
cv::Mat image_binary;               // 二值化图像
uint8_t image_threshold = 0;        // 二值化阈值（大津法自动计算）

/*============================================================================
 *                              函数实现
 *============================================================================*/

/**
 * @brief   获取图像
 * @note    从摄像头获取一帧图像存入 image_frame，并转换为灰度图存入 image_gray
 */
void Get_Image(void)
{
    // 检查初始化状态
    if (!image_init_ok)
    {
        printf("ERROR: 摄像头或UDP初始化失败!\n");
        return;
    }

    // 获取原始图像（RGB格式）
    image_frame = cam.get_frame_raw();
    if (image_frame.empty())
    {
        printf("ERROR: 读取图像失败!\n");
        return;
    }

    // 翻转图像（旋转180度）
    // cv::flip(image_frame, image_frame, -1);
}

/**
 * @brief   大津法自动计算二值化阈值
 * @note    对 image_gray 进行二值化处理，使用形态学闭运算抗反光
 */
void Otsu_Threshold(void)
{
    if (image_gray.empty())
    {
        printf("ERROR: 灰度图为空!\n");
        image_threshold = 128;  // 使用默认阈值
        return;
    }

    // 步骤1：高斯模糊降噪（减少噪声干扰）
    cv::GaussianBlur(image_gray, image_gray, cv::Size(3, 3), 0);

    // 步骤2：使用OpenCV大津法计算阈值并二值化
    double threshold = cv::threshold(image_gray, image_binary, 0, 255,
                                     cv::THRESH_BINARY | cv::THRESH_OTSU);
    image_threshold = (uint8_t)threshold;

    // 步骤3：形态学闭运算（填充反光造成的小白洞）
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(image_binary, image_binary, cv::MORPH_CLOSE, kernel);

    // 步骤4：形态学开运算（去除阴影造成的小黑点噪点）
    cv::morphologyEx(image_binary, image_binary, cv::MORPH_OPEN, kernel);
}

/**
 * @brief   图像处理总入口
 * @note    依次执行：获取图像 -> 灰度转换 -> 大津法二值化
 */
void Image_Process(void)
{
    Get_Image();        // 获取图像
    cv::cvtColor(image_frame, image_gray, cv::COLOR_RGB2GRAY);  // 转换为灰度图（用于检测和计算）
    Otsu_Threshold();   // 大津法二值化（含形态学抗反光）

    // 在上、左、右边界各画2像素白框，形成背景墙封住迷宫巡线的逃逸路径（下边界不画）
    image_binary.rowRange(0, 2).setTo(Image_WHITE);                  // 上边界
    image_binary.colRange(0, 2).setTo(Image_WHITE);                  // 左边界
    image_binary.colRange(IMAGE_WIDTH - 2, IMAGE_WIDTH).setTo(Image_WHITE);  // 右边界
}
