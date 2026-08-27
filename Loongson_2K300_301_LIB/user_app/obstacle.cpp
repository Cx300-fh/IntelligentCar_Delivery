#include "obstacle.hpp"

//================================================================================
// 变量定义
//================================================================================

uint8_t flag_obstacle = 0;       // 避障状态标志
float angle_yaw = 0;             // 积分角度
float Gyro_z = 0;                // 偏航角速度

// lq_i2c_lsm6dsr lsm6dsr;          // 陀螺仪设备
// lq_i2c_vl53l0x vl53l0x;          // 激光测距设备

// 色块检测结果图像（用于图传）
cv::Mat image_color_mask;        // 合并的红黄色块二值化图
cv::Mat image_red_mask;          // 红色色块二值化图
cv::Mat image_yellow_mask;       // 黄色色块二值化图

// 色块检测结果
bool obstacle_detected = false;      // 是否检测到障碍物色块
bool red_block_detected = false;     // 红色色块检测结果
bool yellow_block_detected = false;  // 黄色色块检测结果

// 红色色块参数
const cv::Scalar RED_LOWER_RGB(0, 0, 120);       // B G R
const cv::Scalar RED_UPPER_RGB(100, 100, 255);

// 黄色色块参数
const cv::Scalar YELLOW_LOWER_RGB(40, 110, 110); // B G R
const cv::Scalar YELLOW_UPPER_RGB(100, 255, 255);

const double OBSTACLE_MIN_AREA = 120.0;

// 避障参数
const uint16_t OBSTACLE_DIS_THRESHOLD = 300;  // 障碍物距离阈值(mm)
const uint8_t  OBSTACLE_CNT_TRIGGER   = 5;    // 触发所需连续检测次数
const float    OBSTACLE_YAW_LEFT      = 40.0f;   // 左转完成阈值
const float    OBSTACLE_YAW_RIGHT_START = -32.0f;// 右转开始阈值
const float    OBSTACLE_YAW_RIGHT_END = -5.0f;   // 右转完成阈值

// 避障转向输出配置
#define OBSTACLE_ELE_NORMAL     0       // 正常巡线
#define OBSTACLE_ELE_LEFT       70      // 左转
#define OBSTACLE_ELE_LEFT2RIGHT -60     // 左转后右转
#define OBSTACLE_ELE_RIGHT      50      // 右转
#define OBSTACLE_ELE_DONE       0       // 完成

// 陀螺仪积分增益，根据定时器周期调整
#define GYRO_INTEGRATION_DT     0.005f

//================================================================================
// 避障状态机相关变量
//================================================================================
static uint8_t obstacle_cnt = 0;     // 连续检测计数器

//================================================================================
// 函数实现
//================================================================================

/**
 * @brief 色块识别函数
 * @param image_frame 输入图像帧（BGR格式）
 * @param lower_rgb RGB下限阈值 (B, G, R)
 * @param upper_rgb RGB上限阈值 (B, G, R)
 * @param min_area 最小轮廓面积阈值
 * @return true-检测到色块, false-未检测到
 */
bool detect_color_block(const cv::Mat& image_frame,
                        const cv::Scalar& lower_rgb,
                        const cv::Scalar& upper_rgb,
                        double min_area)
{
    cv::Mat mask_color;
    cv::inRange(image_frame, lower_rgb, upper_rgb, mask_color);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask_color, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty())
    {
        printf("[COLOR_BLOCK] NOT FOUND\n");
        return false;
    }

    // 找最大轮廓
    size_t max_idx = 0;
    double max_area = cv::contourArea(contours[0]);
    for (size_t i = 1; i < contours.size(); i++)
    {
        double area = cv::contourArea(contours[i]);
        if (area > max_area)
        {
            max_area = area;
            max_idx = i;
        }
    }

    if (max_area >= min_area)
    {
        printf("[COLOR_BLOCK] DETECTED (area=%.1f)\n", max_area);
        return true;
    }
    else
    {
        printf("[COLOR_BLOCK] NOT FOUND (max_area=%.1f)\n", max_area);
        return false;
    }
}

/**
 * @brief 检测红色色块
 * @param image_frame 输入图像帧（BGR格式）
 * @return true-检测到色块, false-未检测到
 */
bool detect_red_block(const cv::Mat& image_frame)
{
    cv::inRange(image_frame, RED_LOWER_RGB, RED_UPPER_RGB, image_red_mask);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(image_red_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return false;

    double max_area = 0;
    for (const auto& contour : contours)
    {
        double area = cv::contourArea(contour);
        if (area > max_area) max_area = area;
    }

    if (max_area >= OBSTACLE_MIN_AREA)
    {
        // printf("[RED_BLOCK] DETECTED (area=%.1f)\n", max_area);
        red_block_detected = true;
        return true;
    }
    red_block_detected = false;
    return false;
}

/**
 * @brief 检测黄色色块
 * @param image_frame 输入图像帧（BGR格式）
 * @return true-检测到色块, false-未检测到
 */
bool detect_yellow_block(const cv::Mat& image_frame)
{
    cv::inRange(image_frame, YELLOW_LOWER_RGB, YELLOW_UPPER_RGB, image_yellow_mask);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(image_yellow_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return false;

    double max_area = 0;
    for (const auto& contour : contours)
    {
        double area = cv::contourArea(contour);
        if (area > max_area) max_area = area;
    }

    if (max_area >= OBSTACLE_MIN_AREA)
    {
        // printf("[YELLOW_BLOCK] DETECTED (area=%.1f)\n", max_area);
        yellow_block_detected = true;
        return true;
    }
    yellow_block_detected = false;
    return false;
}

/**
 * @brief 检测红色或黄色色块
 * @param image_frame 输入图像帧（BGR格式）
 * @details 同时更新 obstacle_detected、red_block_detected、yellow_block_detected 全局变量
 *          并生成合并的二值化图像 image_color_mask 用于图传
 */
void detect_obstacle(const cv::Mat& image_frame)
{
    red_block_detected = detect_red_block(image_frame);
    yellow_block_detected = detect_yellow_block(image_frame);

    // 更新综合检测结果
    obstacle_detected = red_block_detected || yellow_block_detected;

    // 合并红黄色块二值化图像用于图传
    image_color_mask = cv::Mat::zeros(image_frame.size(), CV_8UC1);
    if (!image_red_mask.empty()) image_color_mask += image_red_mask;
    if (!image_yellow_mask.empty()) image_color_mask += image_yellow_mask;
}

/**
 * @brief 获取色块检测结果图（用于图传）
 * @return 色块二值化图像
 * @note 红色和黄色区域为白色(255)，其他区域为黑色(0)
 */
cv::Mat get_color_mask_image(void)
{
    return image_color_mask;
}

//================================================================================
// 避障状态机相关
//================================================================================

void update_angle_yaw(float gz)
{
    // 将原始数据转换为角速度（度/秒）
    if (gz < 32768) {
        Gyro_z = -(gz / 16.4f);
    } else {
        Gyro_z = (65535 - gz) / 16.4f;
    }
    // 积分计算yaw角（假设调用周期为5ms）
    angle_yaw += Gyro_z * GYRO_INTEGRATION_DT;
}

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
void obstacle_avoidance(void)
{
    uint16_t dis;

    // dis = vl53l0x.get_vl53l0x_dis();
    // printf("VL53L0X distance = %05u, yaw = %3.2f\r\n", dis, angle_yaw);

    // 连续检测触发逻辑
    if (dis < OBSTACLE_DIS_THRESHOLD)
    {
        if (obstacle_cnt < OBSTACLE_CNT_TRIGGER)
        {
            obstacle_cnt++;
        }
    }
    else
    {
        obstacle_cnt = 0;
    }

    // 状态机
    switch (flag_obstacle)
    {
        case 0:  // 正常巡线状态
            if (obstacle_cnt >= OBSTACLE_CNT_TRIGGER)
            {
                angle_yaw = 0;      // 清零yaw角
                flag_obstacle = 1;
                obstacle_cnt = 0;
            }
            break;

        case 1:  // 检测到障碍物，开始左转
            if (angle_yaw > OBSTACLE_YAW_LEFT)
            {
                flag_obstacle = 2;
            }
            break;

        case 2:  // 左转中，等待开始右转
            if (angle_yaw < OBSTACLE_YAW_RIGHT_START)
            {
                flag_obstacle = 3;
            }
            break;

        case 3:  // 右转中
            if (angle_yaw > OBSTACLE_YAW_RIGHT_END)
            {
                flag_obstacle = 4;
            }
            break;

        case 4:  // 避障完成，回到巡线
            if (angle_yaw > -5.0f)
            {
                flag_obstacle = 0;
            }
            break;

        default:
            flag_obstacle = 0;
            break;
    }
}

/**
 * @brief 避障状态机转向输出计算
 * @param flag_obstacle 避障状态标志
 * @return 转向输出值
 */
int16_t obstacle_get_ele_out(uint8_t flag_obstacle)
{
    switch (flag_obstacle)
    {
        case 0: return OBSTACLE_ELE_NORMAL;     // 正常巡线
        case 1: return OBSTACLE_ELE_LEFT;       // 左转
        case 2: return OBSTACLE_ELE_LEFT2RIGHT; // 左转后右转
        case 3: return OBSTACLE_ELE_RIGHT;      // 右转
        case 4: return OBSTACLE_ELE_DONE;       // 完成，回到巡线
        default: return OBSTACLE_ELE_NORMAL;
    }
}


