/********************************************************************************
 * @file            main.cpp
 * @brief           本文件是 LQ_2K300_301_LIB 软件开源库文件的一部分
 * @copyright       版权所有 (C) 2025-2026 北京龙邱科技有限公司
 * @website         http://www.lqist.cn
 * @taobao          https://longqiu.taobao.com
 *
 * @description     龙邱科技 LS2K300/301 核心板驱动开源库声明
 *
 * 本文件遵循 GPL-3.0 开源协议发布，旨在为龙芯 2K300/301 平台提供快速上手开发基于龙芯 2K300/301 平台的应用程序的参考实现.
 * 商业用途(包括单位使用)需提前联系作者获取授权
 *
 * GPL-3.0 许可证声明摘要:
 * 1. 允许自由使用、修改、分发本软件
 * 2. 分发修改后的版本时，必须以相同许可证发布
 * 3. 必须保留原始版权声明和许可证信息
 * 4. 不提供任何担保，使用风险自负
 * 5. 完整协议文本请参见项目根目录 LICENSE 文件
 *
 * @author          龙邱科技-012
 * @email           chiusir@163.com
 * @version         V2.1.0
 * @update          2026-04-21
 *
 * @note            使用本开源库前, 请确认板卡型号与 PMON 版本是否匹配.
 *                  龙邱 2K301 或已根据群文件升级系统的2K300可直接使用.
 *                  旧版本 2K300 需要修改时钟频率, 请到 lq_clock.hpp 文件修改 CONFIG_USE_PMON.
 *
 * @note            本库已重载 CTRL+C 信号, 按下 CTRL+C 时，会设置 ls_system_running 为 false，
 *                  从而退出所有正在运行的 demo 例程.
 * @note            如果用户在主程序写了自定义的循环，需要在循环中判断 ls_system_running.load() 是否为 true，
 *                  否则会导致程序无法退出.
 ********************************************************************************/

#include "main.hpp"
#include "include.hpp"


lq_timer timer_0;


// 定时器回调函数
void my_timer_0_callback(void)
{
    Ultima_Control();  // 调用综合控制函数

    // // 获取陀螺仪姿态传感器数据
    // lsm6dsr.get_lsm6dsr_gyro(&ax, &ay, &az, &gx, &gy, &gz);
    
    // // 更新yaw角（偏航角速度积分）
    // update_angle_yaw((float)gz);

    // obstacle_avoidance(); // 避障状态机
}


int main()
{
    All_Init(); // 初始化

    // 导航状态机初始化
    nav_fsm.init();

    // 配送协调器初始化（读配置文件决定配送/本地模式）并启动网关线程
    delivery.Init();
    delivery.Start();

    // 定时器初始化（Ctrl+C退出时：先停定时器，再归零PWM安全停车）
    lq_signal_set_exit_cb([](){ timer_0.stop(); Control_Safe_Shutdown(); });
    timer_0.set_seconds_ms(5, my_timer_0_callback);     // 设置定时器回调，5ms执行一次

    while (ls_system_running.load())
    {
        Calculate_FPS();            // 计算FPS（每秒更新一次fps_observe变量，不打印）

        // 仅在摄像头初始化成功时执行图像处理
        if (image_init_ok) {
            Image_Process();            // 图像总采集处理
            Boundary_Extract();         // 迷宫扫线提取边界并计算循迹偏差
            // midline_offset();           // 转向动作处理（暂时的中线偏置或固定舵机输出）(未使用，采用舵机固定输出)
            ele_current = bias_calculate();  // 计算循迹偏差
            Tag_Scan_Process();         // 标签识别处理
            detect_obstacle(image_frame);    // 色块识别（红/黄色块）
            Draw_RGB();                 // 绘制所有可视化内容（边线、Tag等）
            Send_Image(1);              // 图传发送图像 (0-不发送, 1-原图, 2-灰度图, 3-二值化图, 4-合并色块图, 5-红色块图, 6-黄色块图)
        } else {
            // 摄像头初始化失败：程序不退出（Kevin验收项）——停车保持+网络在线，
            // 无视觉时导航位置未知，任何goto_stop都会被POSITION_UNKNOWN拒绝（安全）
            static uint32_t cam_warn_ms = 0;
            if (lq_get_tick_ms() - cam_warn_ms >= 5000) {
                cam_warn_ms = lq_get_tick_ms();
                printf("[CAM] 摄像头未就绪：保持停车与网络在线，人工恢复后重启程序\n");
            }
            usleep(1000 * 50);   // 无图像时节流，避免主循环空转
        }

        // 至此，图像处理完成，可用数据：
        // ├─ 图像:   RGB原图、灰度图、二值化图
        // ├─ 边界:   border_msg(边界信息), ele_current(循迹偏差)
        // ├─ AprilTag: tag_id, tag_angle, tag_dir_ns/ew, tag_center_x/y, tag_dir_ns/ew_name
        // └─ 帧率:   fps_observe

        // 导航状态机更新
        // ┌─────────────┬─────────────────────────────────────────┐
        // │ 状态        │ 说明                                    │
        // ├─────────────┼─────────────────────────────────────────┤
        // │ IDLE        │ 等待指令，小车静止                      │
        // │ SEARCHING   │ 盲走阶段，循迹寻找第一个Tag             │
        // │ AT_NODE     │ 到达节点，决策（走错路/路口/查表转向）  │
        // │ WAITING     │ 等待3秒，播报位置                       │
        // │ EXECUTING   │ 执行动作（循迹/转向/直行/掉头）         │
        // │ ARRIVED     │ 到达终点，停车                          │
        // └─────────────┴─────────────────────────────────────────┘
        // 配送协调器：消费服务器命令队列+推进协调状态机+到站观察+心跳
        // （必须在nav_fsm.update()之前：goto_stop先安装导航段，update随后执行）
        delivery.Process();

        nav_fsm.update();

        // ==================== 控制命令快照发布（每圈必发，兼作主线程心跳）====================
        // 运动许可：配送模式=配送协调器（服务器命令驱动）；本地模式=沿用should_run()
        bool motion_permitted = delivery.Enabled() ? delivery.Motion_Permitted()
                                                   : should_run();
        run = motion_permitted ? 1 : 0;   // 兼容屏幕/调试显示

        // 本地模式安全管理者：屏幕STOP置位的安全禁止，在导航空闲且已停稳后由主线程清除
        // （配送模式下清除职责在配送协调器：LINK_LOSS位重连同步后清、EMERGENCY位resume后清）
        if (!delivery.Enabled() &&
            Safety_Inhibit_Active() &&
            nav_fsm.get_state() == NAV_STATE_IDLE &&
            Control_Is_Stopped())
        {
            Safety_Inhibit_Clear();
        }

        // 循迹左边界判定（原dir_control中的逻辑移至主线程，5ms不再读nav_fsm）
        int follow_left_cmd = compute_follow_left();

        Control_Publish_Command(motion_permitted, STOP_MODE_NONE,
                                (int32_t)nav_fsm.get_action(), follow_left_cmd,
                                target_speed, ele_current);

        // 获取当前导航动作（用于运动控制）
        // ┌───────────────────┬────────────────────────────────┐
        // │ 动作              │ 控制逻辑                       │
        // ├───────────────────┼────────────────────────────────┤
        // │ ACTION_FOLLOW     │ 循迹行驶（使用ele_current偏差）│
        // │ ACTION_STRAIGHT   │ 直行过路口                     │
        // │ ACTION_TURN_LEFT  │ 左转                           │
        // │ ACTION_TURN_RIGHT │ 右转                           │
        // │ ACTION_UTURN      │ 掉头                           │
        // │ ACTION_STOP       │ 停车                           │
        // └───────────────────┴────────────────────────────────┘
        // TODO: 根据current_action实现对应的方向环与速度环控制（舵机、差速、电机）
        ActionType current_action = nav_fsm.get_action();

        // 屏幕指令控制导航（处理逻辑在 screen.cpp Screen_Rx_Process()）
        // ┌─────────────┬─────────────────────────────────────────┐
        // │ 指令        │ 功能                                    │
        // ├─────────────┼─────────────────────────────────────────┤
        // │ AIM,map,tgt │ 设定地图和目标点（仅保存，不启动）      │
        // │ START       │ 使用已设定的地图和目标点启动导航        │
        // │ STOP/PAUSE  │ 急停/取消当前任务                       │
        // └─────────────┴─────────────────────────────────────────┘

        // 每6帧发送一次屏幕数据（避免串口过载）
        static int screen_send_cnt = 0;
        if (++screen_send_cnt >= 10) {
            Screen_Send_All();
            screen_send_cnt = 0;
        }

        // 陶晶驰屏幕心跳包：每200ms发送一次，保持屏幕在线状态
        // 防止屏幕认为龙芯未启动而跳转初始化界面并清空地图/目标点
        static uint32_t last_hb_time = 0;
        uint32_t current_time = lq_get_tick_ms();
        if (current_time - last_hb_time >= 200) {  // 200ms间隔
            Screen_Send_Heartbeat();
            last_hb_time = current_time;
        }

        // WiFi状态定时检查（每3秒检查一次连接状态）
        WiFi_Periodic_Check();

        // 实时调试信息（需要时取消注释）
        // Print_Data();              // 打印传感器/导航数据到调试串口

        // usleep(1000 * 1);           // 主循环休眠，避免占用过多 CPU
    }

    delivery.Stop();   // 网关线程安全退出（shutdown+join）

    return 0;
}
