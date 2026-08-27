#include "control.hpp"
#include "navigation.hpp"   // 仅使用ActionType枚举值，5ms线程不访问nav_fsm运行数据

//================================================================================
// 全局变量定义
//================================================================================
uint8_t follow_row = 80;              // 循迹参考行

PID left_motor_pid = {0}, right_motor_pid = {0}, turn_pid = {0};

double turn_ele[2] = {1.5, 28.8};       // 转向环 PD 参数 [KP, KD]
double ele_target = 0;                // 目标偏差（0=居中）
double ele_current = 0;               // 当前偏差值（主线程更新，5ms经快照读取）
double ele_out = 0;                   // 转向环输出

double left_motor_param[3] = {21, 8.5, 0};     // 左轮 PID 参数 [KP, KI, KD]
double right_motor_param[3] = {21, 8.5, 0};    // 右轮 PID 参数 [KP, KI, KD]

double encoder_l = 0, encoder_r = 0;
unsigned int encoder_ave = 0;
double left_motor_duty = 0, right_motor_duty = 0;
double left_speed = 0, right_speed = 0, current_speed = 0;
uint32_t mile = 0;                   // 里程计数（5ms线程私有，主线程经Control_Get_Mile读取）

uint8_t run = 0;                     // 默认停车；主线程每圈按运动许可刷新（仅显示用）
double speed_ramp = 1;               // 速度缓变步长（加减速共用）
double target_speed = 20;            // 目标速度

// 初始化编码器
ls_encoder_pwm enc3(ENC_PWM2_PIN66, PIN_74);
ls_encoder_pwm enc4(ENC_PWM3_PIN67, PIN_75);

// 初始化电机PWM
ls_atim_pwm motor_pwm1(ATIM_PWM0_PIN81, 17000, 0);
ls_atim_pwm motor_pwm2(ATIM_PWM1_PIN82, 17000, 0);

// 初始化电机方向GPIO
ls_gpio gpio1(PIN_21, GPIO_MODE_OUT);
ls_gpio gpio2(PIN_22, GPIO_MODE_OUT);

// 初始化舵机PWM
ls_gtim_pwm servo_pwm(GTIM_PWM1_PIN88, 100, SERVO_MID);

// 转向舵机输出配置
#define TURN_LEFT_ELEOUT    55     // 左转时舵机输出值
#define TURN_RIGHT_ELEOUT   -55      // 右转时舵机输出值
#define TURN_UTURN_ELEOUT    -80     // 掉头时舵机输出值

int follow_left = 0;  // 循迹左边界标志位（5ms线程按快照刷新，调试显示用）

//================================================================================
// 控制快照架构（线程边界）
//================================================================================
static ControlChannel<ControlCommandSnapshot> g_cmd_channel;   // 主线程 -> 5ms
static ControlChannel<ControlTelemetrySnapshot> g_tel_channel; // 5ms -> 主线程
static std::atomic<uint32_t> g_inhibit_reason(0);              // 安全禁止位
static uint32_t g_cmd_generation = 0;    // 命令快照发布代数（主线程）
static uint32_t g_tel_generation = 0;    // 反馈快照发布代数（5ms）
static uint32_t g_mile_clear_seq = 0;    // 清里程请求序号（主线程）
static uint32_t g_mile_clear_done = 0;   // 已执行的清零序号（5ms）

// 5ms线程私有状态
static ControlCommandSnapshot g_last_cmd;   // 本周期命令快照缓存
static int32_t  g_uturn_stage = 0;          // 掉头阶段：0=无 1=旋转 2=直行
static uint32_t g_stop_ticks = 0;           // 连续低速计数（停稳判定）
static bool     g_watchdog_stale = false;   // 主线程看门狗状态

//--------------------------------------------------------------------------------
// 主线程侧API
//--------------------------------------------------------------------------------
void Control_Publish_Command(bool motion_permitted, uint8_t stop_mode,
                             int32_t action, int32_t follow_left_cmd,
                             double speed_cmd, double ele_cmd)
{
    ControlCommandSnapshot cmd;
    cmd.generation       = ++g_cmd_generation;
    cmd.motion_permitted = motion_permitted;
    cmd.stop_mode        = stop_mode;
    cmd.action           = action;
    cmd.follow_left      = follow_left_cmd;
    cmd.target_speed     = speed_cmd;
    cmd.ele_current      = ele_cmd;
    cmd.mile_clear_seq   = g_mile_clear_seq;
    g_cmd_channel.publish(cmd);
}

uint32_t Control_Request_Mile_Clear(void)
{
    return ++g_mile_clear_seq;   // 5ms线程看到新序号即清零里程
}

ControlTelemetrySnapshot Control_Get_Telemetry(void)
{
    return g_tel_channel.read();
}

uint32_t Control_Get_Mile(void)
{
    return g_tel_channel.read().mile;
}

bool Control_Is_Uturning(void)
{
    return g_tel_channel.read().uturn_stage != 0;
}

bool Control_Is_Stopped(void)
{
    return g_tel_channel.read().is_stopped;
}

//--------------------------------------------------------------------------------
// 安全禁止位：任何线程可置位；只有主线程在状态机允许时清除
//--------------------------------------------------------------------------------
void Safety_Inhibit_Set(uint32_t reason_bits)
{
    g_inhibit_reason.fetch_or(reason_bits, std::memory_order_acq_rel);
}

bool Safety_Inhibit_Clear(void)   // 仅主线程调用
{
    return g_inhibit_reason.exchange(0, std::memory_order_acq_rel) != 0;
}

void Safety_Inhibit_Clear_Bits(uint32_t bits)   // 仅主线程调用
{
    g_inhibit_reason.fetch_and(~bits, std::memory_order_acq_rel);
}

bool Safety_Inhibit_Active(void)
{
    return g_inhibit_reason.load(std::memory_order_acquire) != 0;
}

uint32_t Safety_Inhibit_Reason(void)
{
    return g_inhibit_reason.load(std::memory_order_acquire);
}

//--------------------------------------------------------------------------------
// 安全停车：PWM归零+舵机回中+PID状态清理（退出/致命错误时调用）
//--------------------------------------------------------------------------------
void Control_Safe_Shutdown(void)
{
    PID zero = {0};
    current_speed = 0;
    left_speed = right_speed = 0;
    left_motor_duty = right_motor_duty = 0;
    left_motor_pid = zero;
    right_motor_pid = zero;
    turn_pid = zero;
    motor_pwm1.atim_pwm_set_duty(0);
    motor_pwm2.atim_pwm_set_duty(0);
    gpio1.gpio_level_set(GPIO_LOW);
    gpio2.gpio_level_set(GPIO_LOW);
    servo_pwm.gtim_pwm_set_duty(SERVO_MID);
    printf("[CTRL] 安全停车：电机PWM归零，舵机回中\n");
}


double bias_calculate()  // 偏差计算（主线程调用）
{
    double error_sum = 0;

    for(int i = follow_row - 3; i <= follow_row + 4; i++)
    {
        error_sum += border_msg[i].center_line - IMAGE_WIDTH / 2;
    }

    return RANGE_LIMIT(error_sum / 8.0, -BIAS_MAX, BIAS_MAX);
}


/**
 * @brief 方向环控制（5ms线程）：只读命令快照，不访问nav_fsm和主线程变量
 */
void dir_control()
{
    double diff_ratio = 0;  // 差速比例
    const ControlCommandSnapshot& cmd = g_last_cmd;

    /* 转向PD：error = target(0) - current(快照中的循迹偏差) */
    ele_out = place_pid_control(&turn_pid, cmd.ele_current, ele_target, turn_ele);

    follow_left = cmd.follow_left;  // 刷新调试显示用全局

    ActionType action = (ActionType)cmd.action;

    /* 掉头阶段状态机（5ms自持，替代原is_uturning跨线程标志） */
    if (action == ACTION_UTURN) {
        if (g_uturn_stage == 0) { g_uturn_stage = 1; mile = 0; }   // 进入掉头并清里程
    } else if (g_uturn_stage != 0) {
        g_uturn_stage = 0;   // 导航动作已离开掉头
    }

    // 转向状态维持里程计数
    if (mile >= TURN_MILE_LIMIT && g_uturn_stage == 0)
    {
        action = ACTION_FOLLOW;  // 里程达到后停止转向，恢复循迹
        follow_left = 0;
    }
    // 掉头第一段（旋转）结束
    if (mile >= UTURN_MILE_LIMIT_1 && g_uturn_stage == 1)
    {
        g_uturn_stage = 2;  // 进入掉头第二阶段，向前直行一段距离
        action = ACTION_FOLLOW;
    }
    // 掉头第二段（直行）结束
    if (mile >= UTURN_MILE_LIMIT_2 && g_uturn_stage == 2)
    {
        g_uturn_stage = 0;  // 清零掉头阶段
        action = ACTION_FOLLOW;
    }

    // 特殊情况手动赋值舵机转角
    switch (action) {
        case ACTION_TURN_LEFT:
            // 左转
            ele_out = TURN_LEFT_ELEOUT;
            break;

        case ACTION_TURN_RIGHT:
            // 右转
            ele_out = TURN_RIGHT_ELEOUT;
            break;

        case ACTION_STRAIGHT:
            // 直行
            ele_out = 0;
            break;

        case ACTION_UTURN:
            // 掉头
            ele_out = TURN_UTURN_ELEOUT;  // 掉头时舵机打角
            break;

        default:
            // 正常循迹，无偏移
            break;
    }

    ele_out = RANGE_LIMIT(ele_out, -ELE_OUT_MAX, ELE_OUT_MAX);  // 舵机输出范围限制

    unsigned int servo_duty = SERVO_MID - ele_out * 2.5;

    servo_pwm.gtim_pwm_set_duty(servo_duty);

    /* 速度状态：运动许可=快照许可 且 无安全禁止 且 主线程存活 */
    bool drive = cmd.motion_permitted && !Safety_Inhibit_Active() && !g_watchdog_stale;
    if (drive)
    {
        current_speed = (current_speed < cmd.target_speed) ? current_speed + speed_ramp : cmd.target_speed;  // 速度缓加至目标速度
    }
    else
    {
        uint32_t inhibit = Safety_Inhibit_Reason();
        if (cmd.stop_mode == STOP_MODE_EMERGENCY || (inhibit & INHIBIT_REASON_EMERGENCY))
        {
            current_speed = 0;  // 急停：立即归零
        }
        else if (Safety_Inhibit_Active() || cmd.stop_mode == STOP_MODE_CONTROLLED || g_watchdog_stale)
        {
            current_speed = (current_speed > 0) ? current_speed - speed_ramp * CONTROL_FAST_BRAKE_GAIN : 0;  // 受控快停
        }
        else
        {
            current_speed = (current_speed > 0) ? current_speed - speed_ramp : 0;  // 正常缓停（业务到站）
        }
    }

    /* 差速控制 */
    if(ele_out >= 0)  // 左转：左轮减速多，右轮减速少
    {
        diff_ratio = ele_out * 0.01;
        left_speed = current_speed * (1 - diff_ratio);
        right_speed = current_speed * (1 + diff_ratio * 0.2);
    }
    else  // 右转：右轮减速多，左轮减速少
    {
        diff_ratio = (-ele_out) * 0.01;
        left_speed = current_speed * (1 + diff_ratio * 0.2);
        right_speed = current_speed * (1 - diff_ratio);
    }
    if (g_uturn_stage == 1)
    {
        left_speed = -current_speed;
        right_speed = 0;
    }
}


/**
 * @brief 电机控制（5ms线程）：速度闭环、里程累计、发布反馈快照
 */
void motor_control()
{
    /* 获取编码器值 */
    encoder_l = - enc3.encoder_get_count();
    encoder_r = enc4.encoder_get_count();
    encoder_ave = (unsigned int)((fabs(encoder_l) + fabs(encoder_r)) / 2.0);
    mile += encoder_ave;  // 里程计数（编码器平均值积分）

    left_motor_duty += pid_realize(&left_motor_pid, encoder_l, left_speed, left_motor_param);
    right_motor_duty += pid_realize(&right_motor_pid, encoder_r, right_speed, right_motor_param);

    left_motor_duty = RANGE_LIMIT(left_motor_duty, -MOTOR_MAX, MOTOR_MAX);
    right_motor_duty = RANGE_LIMIT(right_motor_duty, -MOTOR_MAX, MOTOR_MAX);

    motor_pwm1.atim_pwm_set_duty(left_motor_duty);
    motor_pwm2.atim_pwm_set_duty(right_motor_duty);

    if (left_motor_duty > 0)
    {
        gpio2.gpio_level_set(GPIO_HIGH);
        motor_pwm2.atim_pwm_set_duty(left_motor_duty);
    }
    else
    {
        gpio2.gpio_level_set(GPIO_LOW);
        motor_pwm2.atim_pwm_set_duty(-left_motor_duty);
    }

    if (right_motor_duty > 0)
    {
        gpio1.gpio_level_set(GPIO_HIGH);
        motor_pwm1.atim_pwm_set_duty(right_motor_duty);
    }
    else
    {
        gpio1.gpio_level_set(GPIO_LOW);
        motor_pwm1.atim_pwm_set_duty(-right_motor_duty);
    }

    /* 停稳判定：编码器平均值与目标速度连续一段时间接近零 */
    if (encoder_ave < CONTROL_STOP_ENC_THRESH && fabs(current_speed) < 1.0)
    {
        if (g_stop_ticks < CONTROL_STOP_TICKS) g_stop_ticks++;
    }
    else
    {
        g_stop_ticks = 0;
    }

    /* 发布控制反馈快照 */
    ControlTelemetrySnapshot tel;
    tel.generation    = ++g_tel_generation;
    tel.left_speed    = left_speed;
    tel.right_speed   = right_speed;
    tel.current_speed = current_speed;
    tel.encoder_l     = encoder_l;
    tel.encoder_r     = encoder_r;
    tel.mile          = mile;
    tel.uturn_stage   = g_uturn_stage;
    tel.is_stopped    = (g_stop_ticks >= CONTROL_STOP_TICKS);
    tel.fault_bits    = (g_watchdog_stale ? CONTROL_FAULT_WATCHDOG : 0)
                      | (Safety_Inhibit_Active() ? CONTROL_FAULT_INHIBITED : 0);
    g_tel_channel.publish(tel);
}


/**
 * @brief 综合控制（5ms定时调用）：快照读取 -> 看门狗 -> 方向环 -> 速度环 -> 反馈发布
 */
void Ultima_Control()
{
    /* 1. 读取最新命令快照 */
    g_last_cmd = g_cmd_channel.read();

    /* 2. 主线程活性看门狗：generation停滞超时强制停车，恢复推进后自动解除 */
    uint32_t now = lq_get_tick_ms();
    static uint32_t last_gen = 0;
    static uint32_t gen_change_ms = 0;
    static bool     first_tick = true;
    if (first_tick) { gen_change_ms = now; first_tick = false; }
    if (g_last_cmd.generation != last_gen)
    {
        last_gen = g_last_cmd.generation;
        gen_change_ms = now;
        g_watchdog_stale = false;
    }
    else if (now - gen_change_ms > CONTROL_WATCHDOG_MS)
    {
        if (!g_watchdog_stale)
        {
            g_watchdog_stale = true;
            printf("[CTRL] 主线程看门狗触发：generation停滞超%ums，强制停车\n",
                   (unsigned)CONTROL_WATCHDOG_MS);
        }
    }

    /* 3. 处理清里程请求 */
    if (g_last_cmd.mile_clear_seq != g_mile_clear_done)
    {
        mile = 0;
        g_mile_clear_done = g_last_cmd.mile_clear_seq;
    }

    /* 4. 控制计算 */
    dir_control();
    motor_control();
}


/**
 * @brief 位置式PD（用于转向控制）
 * @param now_point  当前测量值
 * @param set_point  目标值
 */
double place_pid_control(PID* sptr, double now_point, double set_point, double *turn_pid)
{
    sptr->KP = *turn_pid;
    sptr->KD = *(turn_pid + 1);

    sptr->iError = set_point - now_point;

    double output = sptr->KP * sptr->iError
                  + sptr->KD * (sptr->iError - sptr->LastError);

    sptr->LastError = sptr->iError;

    return output;
}


/**
 * @brief 增量式PID（用于电机闭环控制）
 * @param actual_speed  当前转速
 * @param set_speed     目标转速
 */
double pid_realize(PID* sptr, double actual_speed, double set_speed, double *motor_pid)
{
    sptr->KP = *motor_pid;
    sptr->KI = *(motor_pid + 1);
    sptr->KD = *(motor_pid + 2);

    sptr->iError = set_speed - actual_speed;

    double increase = sptr->KP * (sptr->iError - sptr->LastError)
                    + sptr->KI * sptr->iError
                    + sptr->KD * (sptr->iError - 2 * sptr->LastError + sptr->PrevError);

    sptr->PrevError = sptr->LastError;
    sptr->LastError = sptr->iError;

    return increase;
}
