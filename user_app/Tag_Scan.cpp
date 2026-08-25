/**
 * @file Tag_Scan.cpp
 * @brief AprilTag标签识别模块
 * @details 实现Tag16h5标签检测与识别功能
 *          角度定义：正前方为0°，顺时针增加，左偏从360递减，范围0-360°
 */

#include "Tag_Scan.hpp"

/*============================================================================
 *                              全局变量定义
 *============================================================================*/
int tag_id = -1;                            // 标签ID
float tag_angle = 0.0f;                     // 标签相对角度 (度)
float tag_center_x = 0.0f;                  // 标签中心X坐标
float tag_center_y = 0.0f;                  // 标签中心Y坐标

// Tag方向信息
int tag_dir_ns = DIR_NS_PENDING;            // 南北方向
int tag_dir_ew = DIR_EW_PENDING;            // 东西方向
const char* tag_dir_ns_name = "P";          // 南北方向名称
const char* tag_dir_ew_name = "P";          // 东西方向名称

/*============================================================================
 *                              内部变量
 *============================================================================*/
static apriltag_family_t *tf = NULL;        // 标签族
static apriltag_detector_t *td = NULL;      // 检测器

// 检测到的角点坐标（用于绘制）
int det_corners[4][2] = {{0}};
bool det_found = false;                     // 是否检测到标签
static int detect_skip = 0;                 // 隔帧检测计数

// 连续帧验证（防止误检）
static int last_tag_id = -1;                // 上一次检测到的ID
static int same_id_count = 0;               // 连续检测到相同ID的次数
#define SAME_ID_THRESHOLD 2                 // 连续检测到相同ID的阈值

#define DETECT_INTERVAL 20                  // 每隔几帧检测一次

/*============================================================================
 *                              函数实现
 *============================================================================*/

/**
 * @brief   标签识别初始化
 * @note    初始化AprilTag检测器
 */
void Tag_Scan_Init(void)
{
    // 使用 Tag16h5
    tf = tag16h5_create();
    td = apriltag_detector_create();
    apriltag_detector_add_family(td, tf);

    // 设置检测参数
    td->quad_decimate = 2.0f;       // 图像降采样，大幅降低计算量
    td->quad_sigma = 0.0f;          // 高斯模糊
    td->nthreads = 1;               // 线程数
    td->debug = 0;                  // 调试模式
    td->refine_edges = 1;           // 边缘细化，提高精度

    printf("AprilTag Tag16h5 初始化完成 (ID范围: 0-29)\n");
}

/**
 * @brief   标签检测
 * @note    从图像中检测Tag36h11标签，获取ID和角度
 */
void Tag_Scan_Process(void)
{
    // 隔帧检测
    detect_skip++;
    if (detect_skip < DETECT_INTERVAL)
        return;
    detect_skip = 0;

    if (image_gray.empty())
    {
        tag_id = -1;
        tag_angle = 0.0f;
        tag_center_x = 0.0f;
        tag_center_y = 0.0f;
        det_found = false;
        return;
    }

    image_u8_t img = {
        .width = image_gray.cols,
        .height = image_gray.rows,
        .stride = image_gray.cols,
        .buf = image_gray.data
    };

    zarray_t *detections = apriltag_detector_detect(td, &img);
    int detection_count = zarray_size(detections);

    if (detection_count == 0)
    {
        same_id_count = 0;
        last_tag_id = -1;
        tag_id = -1;
        tag_angle = 0.0f;
        tag_center_x = 0.0f;
        tag_center_y = 0.0f;
        det_found = false;
        tag_dir_ns = DIR_NS_PENDING;
        tag_dir_ew = DIR_EW_PENDING;
        tag_dir_ns_name = "P";
        tag_dir_ew_name = "P";
        zarray_destroy(detections);
        return;
    }

    // 获取第一个检测到的标签
    apriltag_detection_t *det;
    zarray_get(detections, 0, &det);

    int current_id = det->id;

    // 连续帧验证：只有连续检测到相同ID才认为有效
    if (current_id == last_tag_id)
    {
        same_id_count++;
    }
    else
    {
        same_id_count = 1;
        last_tag_id = current_id;
    }

    if (same_id_count < SAME_ID_THRESHOLD)
    {
        tag_id = -1;
        tag_angle = 0.0f;
        tag_center_x = 0.0f;
        tag_center_y = 0.0f;
        det_found = false;
        tag_dir_ns = DIR_NS_PENDING;
        tag_dir_ew = DIR_EW_PENDING;
        tag_dir_ns_name = "P";
        tag_dir_ew_name = "P";
        zarray_destroy(detections);
        return;
    }

    tag_id = det->id;
    tag_center_x = det->c[0];
    tag_center_y = det->c[1];

    // 保存四个角点坐标
    for (int i = 0; i < 4; i++)
    {
        det_corners[i][0] = (int)det->p[i][0];
        det_corners[i][1] = (int)det->p[i][1];
    }
    det_found = true;

    // 计算标签角度
    // 角度定义：正前方为0°，顺时针增加，范围0-360°（无负数）
    // 右偏：从0递增（如右偏90°为90°）
    // 左偏：从360递减（如左偏90°为270°）
    double h00 = MATD_EL(det->H, 0, 0);
    double h10 = MATD_EL(det->H, 1, 0);
    tag_angle = -atan2(h10, h00) * 180.0 / M_PI;  // 取负实现顺时针
    
    // 转换为0-360范围（无负数）
    if (tag_angle < 0) {
        tag_angle += 360.0f;
    }
    // 确保范围在0-360
    while (tag_angle >= 360.0f) tag_angle -= 360.0f;
    while (tag_angle < 0.0f) tag_angle += 360.0f;

    // 计算方向（南北/东西）
    int angle_norm = (int)tag_angle % 360;
    if (angle_norm < 0) angle_norm += 360;

    // 南北方向：[0,90)∪[270,360)为北，[90,270)为南
    tag_dir_ns = (angle_norm < 90 || angle_norm >= 270) ? DIR_NS_NORTH : DIR_NS_SOUTH;
    tag_dir_ns_name = (tag_dir_ns == DIR_NS_NORTH) ? "N" : "S";

    // 东西方向：[0,180)为东，[180,360)为西
    tag_dir_ew = (angle_norm < 180) ? DIR_EW_EAST : DIR_EW_WEST;
    tag_dir_ew_name = (tag_dir_ew == DIR_EW_EAST) ? "E" : "W";

    // 释放检测结果
    for (int i = 0; i < detection_count; i++)
    {
        apriltag_detection_t *d;
        zarray_get(detections, i, &d);
        apriltag_detection_destroy(d);
    }
    zarray_destroy(detections);
}
