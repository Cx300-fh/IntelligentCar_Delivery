/**
 * @file observe.cpp
 * @brief 观测模块 - 调试观测框架实现
 */

#include "observe.hpp"

/*============================================================================
 *                              FPS 统计
 *============================================================================*/
int fps_observe = 0;          // 观测模块计算的帧率（每秒更新）

/*============================================================================
 *                              设备IP地址显示（由wifi_manager管理）
 *============================================================================*/
// 注意：device_ip_str, wifi_connected, wifi_ssid_display, wifi_state
// 这些变量定义在 wifi_manager.cpp，这里仅通过 extern 引用

/**
 * @brief   发送图像
 * @param   type 图像类型: 0-不发送, 1-原图, 2-灰度图, 3-二值化图
 * @note    通过UDP发送指定类型的JPEG压缩图像
 */
void Send_Image(uint8_t type)
{
    cv::Mat* img_ptr = nullptr;
    const char* img_name = nullptr;

    switch (type)
    {
        case 0:
            return;     // 不发送
        case 1:
            img_ptr = &image_frame;
            img_name = "原图";
            break;
        case 2:
            img_ptr = &image_gray;
            img_name = "灰度图";
            break;
        case 3:
            img_ptr = &image_binary;
            img_name = "二值化图";
            break;
        default:
            printf("ERROR: 无效的图像类型 %d\n", type);
            return;
    }

    if (img_ptr->empty())
    {
        printf("ERROR: %s为空!\n", img_name);
        return;
    }

    // 发送JPEG压缩图像
    if (udp_client.udp_send_image(*img_ptr, JPEG_QUALITY) < 0)
    {
        printf("ERROR: 发送%s失败!\n", img_name);
        return;
    }
}

/**
 * @brief   打印数据信息（实时调用）
 * @note    只负责打印，不负责采集数据
 */
void Print_Data(void)
{
    printf("========================================\n");
    printf("  Encoder L: %.0f, Encoder R: %.0f\n", encoder_l, encoder_r);
    printf("  Otsu Threshold: %d\n", image_threshold);
    printf("  Tag ID: %d, Angle: %.1f deg, Direction: %s,%s\n",
           tag_id, tag_angle, tag_dir_ns_name, tag_dir_ew_name);
    printf("  Center: (%.0f, %.0f)\n", tag_center_x, tag_center_y);
    printf("  ele_out: %.0f\n", ele_out);
    printf("  FPS: %d\n", fps_observe);
    printf("========================================\n");
}

/**
 * @brief   计算FPS（每秒更新一次fps_observe变量，不打印）
 */
void Calculate_FPS(void)
{
    frame_count++;

    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

    if (elapsed >= 1)
    {
        fps_observe = frame_count / elapsed;
        frame_count = 0;
        start_time = now;
    }
}

/**
 * @brief   绘制所有可视化内容（边线、Tag等）
 * @note    在 image_frame 上绘制：
 *          - 边界线（左红右绿中黄，每隔4行采样）
 *          - 图像中心列白色线
 *          - 循迹参考点（浅蓝色）
 *          - AprilTag 框和信息

 */
void Draw_RGB(void)
{
    if (image_frame.empty())
        return;

    // ====== 1. 绘制边界和中线 ======
    // 收集点集（每隔4行采样一次，减少数据量）
    std::vector<cv::Point> left_pts, right_pts, center_pts;
    left_pts.reserve(SCAN_START_ROW / 4);
    right_pts.reserve(SCAN_START_ROW / 4);
    center_pts.reserve(SCAN_START_ROW / 4);

    for (int row = SCAN_START_ROW; row > 0; row -= 4)
    {
        if (border_msg[row].left_border >= 0 && border_msg[row].left_border < IMAGE_WIDTH)
            left_pts.emplace_back(border_msg[row].left_border, row);

        if (border_msg[row].right_border >= 0 && border_msg[row].right_border < IMAGE_WIDTH)
            right_pts.emplace_back(border_msg[row].right_border, row);

        if (border_msg[row].center_line >= 0 && border_msg[row].center_line < IMAGE_WIDTH)
            center_pts.emplace_back(border_msg[row].center_line, row);
    }

    // 用 polylines 绘制连线（左红右绿中黄）
    if (left_pts.size() > 1)
        cv::polylines(image_frame, left_pts, false, cv::Scalar(0, 0, 255), 2);
    if (right_pts.size() > 1)
        cv::polylines(image_frame, right_pts, false, cv::Scalar(0, 255, 0), 2);
    if (center_pts.size() > 1)
        cv::polylines(image_frame, center_pts, false, cv::Scalar(0, 255, 255), 2);

    // 画图像中心列白色线
    cv::line(image_frame, cv::Point(IMAGE_WIDTH / 2, 0), cv::Point(IMAGE_WIDTH / 2, IMAGE_HEIGHT - 1), cv::Scalar(255, 255, 255), 1);

    // 用浅蓝色标出循迹参考点
    for (int i = follow_row - 3; i <= follow_row + 4; i++)
    {
        cv::circle(image_frame, cv::Point(border_msg[i].center_line, i), 3, cv::Scalar(255, 100, 0), -1);
    }

    // ====== 2. 绘制 AprilTag ======
    // 图像右上角固定显示位置
    const int start_x = image_frame.cols - 65;
    const int start_y = 15;
    const int line_height = 14;
    char text[64];

    if (!det_found)
    {
        // 未检测到AprilTag
        cv::putText(image_frame, "Tag: --",
                    cv::Point(start_x, start_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.35,
                    cv::Scalar(255, 255, 255), 1);
    }
    else
    {
        // 绘制彩色边框（绿色）
        for (int i = 0; i < 4; i++)
        {
            int j = (i + 1) % 4;
            cv::line(image_frame,
                     cv::Point(det_corners[i][0], det_corners[i][1]),
                     cv::Point(det_corners[j][0], det_corners[j][1]),
                     cv::Scalar(0, 255, 0), 2);
        }

        // 绘制中心点（黄色）
        cv::circle(image_frame,
                   cv::Point((int)tag_center_x, (int)tag_center_y),
                   5, cv::Scalar(0, 255, 255), -1);

        // 绘制ID和角度
        snprintf(text, sizeof(text), "ID: %d %.1f", tag_id, tag_angle);
        cv::putText(image_frame, text,
                    cv::Point(start_x, start_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.35,
                    cv::Scalar(255, 255, 255), 1);

    // 绘制南北方向和东西方向（直接使用Tag检测中计算好的值）
    snprintf(text, sizeof(text), "DIR: %s,%s", tag_dir_ns_name, tag_dir_ew_name);
        cv::putText(image_frame, text,
                    cv::Point(start_x, start_y + line_height),
                    cv::FONT_HERSHEY_SIMPLEX, 0.35,
                    cv::Scalar(255, 255, 255), 1);

        // 绘制中心点坐标
        snprintf(text, sizeof(text), "POS: %.0f,%.0f", tag_center_x, tag_center_y);
        cv::putText(image_frame, text,
                    cv::Point(start_x, start_y + line_height * 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.35,
                    cv::Scalar(255, 255, 255), 1);
    }
}
