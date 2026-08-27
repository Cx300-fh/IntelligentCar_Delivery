#include "border.hpp"

//================================================================================
// 变量定义
//================================================================================
uint8_t step_max = STEP_MAX;                // 最大巡线步数

const uint8_t SCAN_START_ROW = IMAGE_HEIGHT - 2;  // 扫描起始行
int top_row = IMAGE_HEIGHT - 1;                   // 顶部截止行

int left_point_count = 0;                    // 左边界点数
int right_point_count = 0;                   // 右边界点数

// 边界信息
Border_message border_msg[120];

// 起始点
uint8_t start_point_left[2] = {IMAGE_WIDTH / 2, 0};
uint8_t start_point_right[2] = {IMAGE_WIDTH / 2, 0};

// 边界线存储 (200步长足够)
uint8_t left_line_points[STEP_MAX][2];
uint8_t right_line_points[STEP_MAX][2];

// 方向记录
int8_t left_prev_dir = 0;
int8_t left_prev2_dir = 0;
int8_t right_prev_dir = 0;
int8_t right_prev2_dir = 0;

// 拐点信息
int inflection_lower_row_l = 0;
int inflection_lower_row_r = 0;
int inflection_upper_row_l = 0;
int inflection_upper_row_r = 0;
int inflection_lower_col_l = 0;
int inflection_lower_col_r = 0;
int inflection_upper_col_l = 0;
int inflection_upper_col_r = 0;

uint8_t search_start_row = 0;  // 搜索起始行

//================================================================================
// 迷宫方向数组
//================================================================================
// 前进方向: 上、右、下、左 (索引0-3)
static int8_t dir_forward[4][2] = {
    {0, -1},   // 上
    {1, 0},    // 右
    {0, 1},    // 下
    {-1, 0}    // 左
};

// 左前方
static int8_t dir_front_left[4][2] = {
    {-1, -1},  // 上时的左前
    {1, -1},   // 右时的左前
    {1, 1},    // 下时的左前
    {-1, 1}    // 左时的左前
};

// 右前方
static int8_t dir_front_right[4][2] = {
    {1, -1},   // 上时的右前
    {1, 1},    // 右时的右前
    {-1, 1},   // 下时的右前
    {-1, -1}   // 左时的右前
};

//================================================================================
// 辅助函数：获取像素值
//================================================================================
static inline uint8_t get_pixel(int row, int col)
{
    if (row < 0 || row >= IMAGE_HEIGHT || col < 0 || col >= IMAGE_WIDTH)
        return Image_WHITE;  // 越界返回白色
    return image_binary.at<uchar>(row, col);
}

//================================================================================
// 函数实现
//================================================================================

/**
 * @brief 在指定扫描行获取左右起始点
 * @param scan_row 扫描行
 * @param last_col 上一次的列位置
 * @return 1-成功找到起始点, 0-失败
 */
uint8_t get_start_point(uint8_t scan_row, uint8_t last_col)
{
    uint8_t left_found = 0;
    uint8_t right_found = 0;

    start_point_left[0] = last_col;
    start_point_left[1] = scan_row;
    start_point_right[0] = last_col;
    start_point_right[1] = scan_row;

    // 找左起始点：黑->白->白跳变
    for (int i = last_col; i > 1; --i)
    {
        if (get_pixel(scan_row, i - 1) == Image_WHITE &&
            get_pixel(scan_row, i) == Image_WHITE &&
            get_pixel(scan_row, i + 1) == Image_BLACK)
        {
            start_point_left[0] = (uint8_t)i;
            left_found = 1;
            break;
        }
    }

    // 找右起始点：黑->白->白跳变
    for (int i = last_col; i < IMAGE_WIDTH - 1; ++i)
    {
        if (get_pixel(scan_row, i - 1) == Image_BLACK &&
            get_pixel(scan_row, i) == Image_WHITE &&
            get_pixel(scan_row, i + 1) == Image_WHITE)
        {
            start_point_right[0] = (uint8_t)i;
            right_found = 1;
            break;
        }
    }

    return (left_found && right_found) ? 1 : 0;
}

/**
 * @brief 迷宫法巡线
 */
void maze_find_line(uint8_t left_start[], uint8_t right_start[],
                    uint8_t left_points[][2], uint8_t right_points[][2])
{
    int temp_top_row = SCAN_START_ROW;        // 临时顶部截止行
    int l_step = 0, l_dir = 0, l_turn = 0;  // 左边的步数、方向和转弯次数
    int r_step = 0, r_dir = 0, r_turn = 0;  // 右边的步数、方向和转弯次数

    // 当前坐标
    uint8_t lx = left_start[0], ly = left_start[1];
    uint8_t rx = right_start[0], ry = right_start[1];

    // 最高点
    uint8_t l_high_x = lx, l_high_y = ly, l_high_step = 0;
    uint8_t r_high_x = rx, r_high_y = ry, r_high_step = 0;

    // 下一步坐标
    uint8_t l_nx = 0, l_ny = 0, l_nlx = 0, l_nly = 0;  // 左边的下一步坐标和左前一步坐标
    uint8_t r_nx = 0, r_ny = 0, r_nrx = 0, r_nry = 0;  // 右边的下一步坐标和右前一步坐标

    // 初始化
    left_prev_dir = 0;
    left_prev2_dir = 0;
    right_prev_dir = 0;
    right_prev2_dir = 0;
    inflection_lower_row_l = 0;  // 左拐点下边界行号
    inflection_lower_row_r = 0;
    inflection_upper_row_l = 0;
    inflection_upper_row_r = 0;

    while (l_step < step_max && r_step < step_max)  // 有一边搜完就退出循环
    {
        // 预计算下一步坐标
        l_nx = lx + dir_forward[l_dir][0];
        l_ny = ly + dir_forward[l_dir][1];
        l_nlx = lx + dir_front_left[l_dir][0];
        l_nly = ly + dir_front_left[l_dir][1];

        r_nx = rx + dir_forward[r_dir][0];
        r_ny = ry + dir_forward[r_dir][1];
        r_nrx = rx + dir_front_right[r_dir][0];
        r_nry = ry + dir_front_right[r_dir][1];

        // ===== 左边巡线 =====
        if (ly > 0 && ly < IMAGE_HEIGHT - 1 && l_turn < 4)
        {
            if (get_pixel(l_ny, l_nx) == Image_WHITE)  // 前方白，右转（回到边界）
            {
                l_dir = (l_dir + 1) & 3;
                l_turn++;
            }
            else if (get_pixel(l_nly, l_nlx) == Image_WHITE)  // 左前方白，直走（沿边界）
            {
                left_points[l_step][0] = lx;
                left_points[l_step][1] = ly;
                l_step++;
                if (ly <= l_high_y)
                {
                    l_high_x = lx;
                    l_high_y = ly;
                    l_high_step = l_step;
                }
                lx = l_nx;
                ly = l_ny;
                l_turn = 0;
            }
            else  // 左前方黑，左转（靠近赛道边界）
            {
                left_points[l_step][0] = lx;
                left_points[l_step][1] = ly;
                l_step++;
                if (ly <= l_high_y)
                {
                    l_high_x = lx;
                    l_high_y = ly;
                    l_high_step = l_step;
                }
                lx = l_nlx;
                ly = l_nly;
                l_dir = (l_dir + 3) & 3;
                l_turn = 0;
            }
        }

        // 检测左边下拐点
        if ((l_dir == 3 || l_dir == 2) && inflection_lower_row_l == 0 &&
            left_prev_dir == 3 && left_prev2_dir == 0)
        {
            if (get_pixel(ly - 1, lx) == Image_BLACK &&
                get_pixel(ly - 1, lx + 2) == Image_BLACK &&
                get_pixel(ly - 1, lx - 2) == Image_BLACK)
            {
                if (inflection_lower_row_l < l_high_y && l_high_y != SCAN_START_ROW)
                {
                    inflection_lower_row_l = l_high_y;
                    inflection_lower_col_l = l_high_x;
                }
            }
        }

        left_prev2_dir = left_prev_dir;
        left_prev_dir = l_dir;

        // ===== 右边巡线 =====
        if (ry > 0 && ry < IMAGE_HEIGHT - 1 && r_turn < 4)
        {
            if (get_pixel(r_ny, r_nx) == Image_WHITE)  // 前方白，左转（回到边界）
            {
                r_dir = (r_dir + 3) & 3;
                r_turn++;
            }
            else if (get_pixel(r_nry, r_nrx) == Image_WHITE)  // 右前方白，直走（沿边界）
            {
                right_points[r_step][0] = rx;
                right_points[r_step][1] = ry;
                r_step++;
                if (ry <= r_high_y)
                {
                    r_high_x = rx;
                    r_high_y = ry;
                    r_high_step = r_step;
                }
                rx = r_nx;
                ry = r_ny;
                r_turn = 0;
            }
            else  // 右前方黑，右转（靠近赛道边界）
            {
                right_points[r_step][0] = rx;
                right_points[r_step][1] = ry;
                r_step++;
                if (ry <= r_high_y)
                {
                    r_high_x = rx;
                    r_high_y = ry;
                    r_high_step = r_step;
                }
                rx = r_nrx;
                ry = r_nry;
                r_dir = (r_dir + 1) & 3;
                r_turn = 0;
            }
        }

        // 检测右边下拐点
        if ((r_dir == 1 || r_dir == 2) && inflection_lower_row_r == 0 &&
            right_prev_dir == 1 && right_prev2_dir == 0)
        {
            if (get_pixel(ry - 1, rx) == Image_BLACK &&
                get_pixel(ry - 1, rx + 2) == Image_BLACK &&
                get_pixel(ry - 1, rx - 2) == Image_BLACK)
            {
                if (inflection_lower_row_r < r_high_y && r_high_y != SCAN_START_ROW)
                {
                    inflection_lower_row_r = r_high_y;
                    inflection_lower_col_r = r_high_x;
                }
            }
        }

        right_prev2_dir = right_prev_dir;
        right_prev_dir = r_dir;

        // 更新最高行坐标
        if (temp_top_row > ly) temp_top_row = ly;
        if (temp_top_row > ry) temp_top_row = ry;

        // 检查是否都结束
        if (!(ly > 0 && ly < IMAGE_HEIGHT - 1 && l_turn < 4) &&
            !(ry > 0 && ry < IMAGE_HEIGHT - 1 && r_turn < 4))
            break;
    }

    left_point_count = l_high_step;
    right_point_count = r_high_step;
    top_row = temp_top_row;
}

/**
 * @brief 边界分析，将巡线信息存入border结构体
 */
void border_analyse(void)
{
    int i = 0, row = 0;
    uint8_t left_fill = 0, right_fill = 0;

    // 初始化边界信息
    for (i = 0; i < IMAGE_HEIGHT; ++i)
    {
        border_msg[i].left_border = 0;
        border_msg[i].right_border = IMAGE_WIDTH - 1;
        border_msg[i].center_line = (IMAGE_WIDTH - 1) >> 1;
        border_msg[i].road_width = IMAGE_WIDTH - 1;
    }

    // 解析左边界线
    for (i = 0; i < left_point_count; i++)
    {
        row = left_line_points[i][1];  // 定位到某一行
        if (border_msg[row].left_border == 0 ||
            border_msg[row].left_border > left_line_points[i][0])  // 找到更左侧的点
        {
            border_msg[row].left_border = left_line_points[i][0];  // 更新左边界点
        }
        if (row < top_row + 1) break;
    }

    // 解析右边界线
    for (i = 0; i < right_point_count; i++)
    {
        row = right_line_points[i][1];  // 定位到某一行
        if (border_msg[row].right_border == IMAGE_WIDTH - 1 ||
            border_msg[row].right_border < right_line_points[i][0])  // 找到更右侧的点
        {
            border_msg[row].right_border = right_line_points[i][0];  // 更新右边界点
        }
        if (row <= top_row + 1) break;
    }

    // 补全顶部边界
    left_fill = border_msg[top_row + 2].left_border;  // 左侧填充点
    right_fill = border_msg[top_row + 2].right_border;

    for (i = top_row + 2; i >= 0; i--)
    {
        border_msg[i].left_border = left_fill;
        border_msg[i].right_border = right_fill;
    }

    // 补全底部边界：起始行以下用起始行的边界填充
    left_fill = border_msg[search_start_row].left_border;
    right_fill = border_msg[search_start_row].right_border;
    for (i = search_start_row + 1; i < IMAGE_HEIGHT; i++)
    {
        border_msg[i].left_border = left_fill;
        border_msg[i].right_border = right_fill;
    }

    // 左右边线限幅
    for (i = SCAN_START_ROW; i >= 0; --i)
    {
        border_msg[i].left_border = RANGE_LIMIT(border_msg[i].left_border, 10, 150);
        border_msg[i].right_border = RANGE_LIMIT(border_msg[i].right_border, 10, 150);
    }

    // 计算中线和宽度
    for (i = SCAN_START_ROW; i >= 0; --i)
    {
        border_msg[i].center_line = (border_msg[i].left_border + border_msg[i].right_border) >> 1;
        // border_msg[i].center_line = border_msg[i].right_border;
        if (follow_left==1)
        {
            border_msg[i].center_line = border_msg[i].left_border;
        }
        border_msg[i].road_width = border_msg[i].right_border - border_msg[i].left_border;
    }
}

/**
 * @brief 边界提取主函数
 */
void Boundary_Extract(void)
{
    left_point_count = 0;
    right_point_count = 0;
    top_row = SCAN_START_ROW;

    // 清零边界点数组，避免上一帧残留数据
    memset(left_line_points, 0, sizeof(left_line_points));
    memset(right_line_points, 0, sizeof(right_line_points));

    for (search_start_row = SCAN_START_ROW - 1; search_start_row > IMAGE_HEIGHT / 2; search_start_row--)
    {
        if (get_start_point(search_start_row, (uint8_t)((start_point_right[0] + start_point_left[0]) >> 1)))
        {
            maze_find_line(start_point_left, start_point_right,
                           left_line_points, right_line_points);
            break;
        }
    }

    border_analyse();
}
