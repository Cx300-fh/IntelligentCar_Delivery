#ifndef __USER_INCLUDE_HPP
#define __USER_INCLUDE_HPP

// ==================== 常用头文件区域（请在此区域内添加）====================

// 标准库头文件
// #include <iostream>
// #include <vector>
// #include <string>
#include <cstring>
#include <chrono>
#include <cstdint>

// 其他常用头文件
#include <stdio.h>

// 包含所有底层驱动头文件
#include "lq_drv_inc.hpp"

// 包含所有工具头文件
#include "lq_common.hpp"

// 包含所有应用层头文件
#include "lq_app_inc.hpp"

// 包含所有测试程序头文件
#include "lq_all_demo.hpp"

// ==================== 通用宏定义区域（请在此区域内添加）====================

// 范围限制宏：将值限制在[min, max]范围内
#define RANGE_LIMIT(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// ==================== 项目头文件区域（请在此区域内添加）====================

// OpenCV库头文件
#include <opencv2/opencv.hpp>

// AprilTag库头文件
extern "C" {
#include "apriltag.h"
#include "tag16h5.h"
#include "common/zarray.h"
}

// 用户自定义头文件
#include "init.hpp"
#include "camera.hpp"
#include "border.hpp"
#include "observe.hpp"
#include "control.hpp"
#include "Tag_Scan.hpp"
#include "dijkstra.hpp"
#include "screen.hpp"
#include "voice.hpp"
#include "navigation.hpp"
#include "order_scheduler.hpp"
#include "wifi_manager.hpp"   // WiFi管理模块（支持屏幕配置WiFi）

#endif
