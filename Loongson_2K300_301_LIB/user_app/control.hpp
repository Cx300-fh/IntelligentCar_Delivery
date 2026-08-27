#ifndef __CONTROL_HPP
#define __CONTROL_HPP

#include "include.hpp"
#include <atomic>

//================================================================================
// 宏定义
//================================================================================
#define MOTOR_MAX    9000   // 电机占空比限幅
#define ELE_OUT_MAX  80     // 转向环输出限幅
#define BIAS_MAX     100    // 偏差限幅
#define SERVO_MID    1370   // 舵机中值（PWM占空比）

// 主线程活性看门狗：命令快照generation超过该时间未刷新，5ms线程强制停车
#define CONTROL_WATCHDOG_MS     500
// 受控较快减速（断线/安全禁止）相对正常减速的倍率
#define CONTROL_FAST_BRAKE_GAIN 4
// 停稳判定：编码器平均值连续低于该阈值视为静止
#define CONTROL_STOP_ENC_THRESH 2
// 停稳判定：连续静止周期数（5ms/周期，40=200ms）
#define CONTROL_STOP_TICKS      40

//================================================================================
// 安全禁止原因位（可叠加）
//================================================================================
#define INHIBIT_REASON_MANUAL    (1u<<0)   // 屏幕/本地手动停止
#define INHIBIT_REASON_LINK_LOSS (1u<<1)   // 网络断开或心跳超时（阶段4接入）
#define INHIBIT_REASON_EMERGENCY (1u<<2)   // 服务器急停命令（阶段5接入）

//================================================================================
// 控制故障位
//================================================================================
#define CONTROL_FAULT_WATCHDOG   (1u<<0)   // 主线程快照停滞，看门狗触发
#define CONTROL_FAULT_INHIBITED  (1u<<1)   // 当前处于安全禁止停车状态

//================================================================================
// 停车模式
//================================================================================
enum StopMode {
    STOP_MODE_NONE       = 0,   // 正常（由motion_permitted决定加减速）
    STOP_MODE_CONTROLLED = 1,   // 受控较快减速（断线/安全禁止）
    STOP_MODE_EMERGENCY  = 2,   // 急停：输出立即归零并清理PID状态
};

//================================================================================
// 控制命令快照（主线程 -> 5ms线程的唯一控制数据通路）
//================================================================================
struct ControlCommandSnapshot {
    uint32_t generation;        // 主线程每次发布递增；5ms线程用作主线程活性看门狗
    bool     motion_permitted;  // 运动总许可（安全门，非完整控制接口）
    uint8_t  stop_mode;         // StopMode
    int32_t  action;            // ActionType：导航决策动作
    int32_t  follow_left;       // 主线程算好的循迹左边界标志（替代5ms读nav_fsm）
    double   target_speed;      // 目标速度
    double   ele_current;       // 主线程最新循迹偏差
    uint32_t mile_clear_seq;    // 清里程请求序号（变化时5ms线程清零里程）

    ControlCommandSnapshot()
        : generation(0), motion_permitted(false), stop_mode(STOP_MODE_NONE),
          action(0), follow_left(0), target_speed(0), ele_current(0),
          mile_clear_seq(0) {}
};

//================================================================================
// 控制反馈快照（5ms线程 -> 主线程的唯一反馈数据通路）
//================================================================================
struct ControlTelemetrySnapshot {
    uint32_t generation;        // 5ms线程每次发布递增
    double   left_speed;        // 左轮目标速度
    double   right_speed;       // 右轮目标速度
    double   current_speed;     // 基础速度
    double   encoder_l;         // 左轮编码器
    double   encoder_r;         // 右轮编码器
    uint32_t mile;              // 里程（5ms线程私有，主线程只经此读取）
    int32_t  uturn_stage;       // 掉头阶段：0=无 1=旋转 2=直行（5ms线程自持）
    bool     is_stopped;        // 编码器连续低速判定已停稳
    uint32_t fault_bits;        // CONTROL_FAULT_xxx

    ControlTelemetrySnapshot()
        : generation(0), left_speed(0), right_speed(0), current_speed(0),
          encoder_l(0), encoder_r(0), mile(0), uturn_stage(0),
          is_stopped(true), fault_bits(0) {}
};

//================================================================================
// 单写单读无锁快照通道（seqlock语义，POD结构专用）
//================================================================================
template <typename T>
class ControlChannel {
public:
    void publish(const T& in)
    {
        // 序号变奇=写入中
        seq_.fetch_add(1, std::memory_order_acq_rel);
        data_ = in;
        // 序号变偶=写入完成
        seq_.fetch_add(1, std::memory_order_acq_rel);
    }

    T read(void) const
    {
        T out;
        uint32_t before, after;
        do {
            before = seq_.load(std::memory_order_acquire);
            if (before & 1u) continue;   // 写入中，重试
            out = data_;
            after = seq_.load(std::memory_order_acquire);
        } while (before != after);
        return out;
    }

private:
    T data_{};
    std::atomic<uint32_t> seq_{0};
};

//================================================================================
// 控制快照API（线程边界，跨线程只允许通过这些接口传递控制数据）
//================================================================================
// ---- 主线程侧 ----
void Control_Publish_Command(bool motion_permitted, uint8_t stop_mode,
                             int32_t action, int32_t follow_left,
                             double target_speed, double ele_current);
uint32_t Control_Request_Mile_Clear(void);       // 请求清零里程（替代原mile_clear_flag）
ControlTelemetrySnapshot Control_Get_Telemetry(void);  // 读控制反馈（拷贝）
uint32_t Control_Get_Mile(void);                 // 读当前里程
bool     Control_Is_Uturning(void);              // 掉头是否进行中（5ms线程状态）
bool     Control_Is_Stopped(void);               // 是否已停稳
// ---- 安全禁止位：任何线程可置位；只有主线程在状态机允许时才能清除 ----
void     Safety_Inhibit_Set(uint32_t reason_bits);   // 任意线程
bool     Safety_Inhibit_Clear(void);                 // 仅主线程
bool     Safety_Inhibit_Active(void);
uint32_t Safety_Inhibit_Reason(void);
// ---- 安全停车（退出/致命错误时归零PWM） ----
void     Control_Safe_Shutdown(void);

//================================================================================
// 结构体定义
//================================================================================
typedef struct PID
{
    double iError;      // 本次偏差
    double LastError;   // 上次偏差
    double PrevError;   // 上上次偏差
    double KP;          // 比例系数
    double KI;          // 积分系数
    double KD;          // 微分系数
} PID;

//================================================================================
// 外部变量声明
//================================================================================
extern uint8_t follow_row;              // 循迹参考行
extern double target_speed;             // 目标速度

extern PID left_motor_pid, right_motor_pid, turn_pid_ele;         // PID结构体

extern double turn_ele[2];            // 转向环 PD 参数 [KP, KD]
extern double ele_target;             // 转向环目标偏差（0=居中）
extern double ele_current;            // 转向环当前偏差值（主线程写，已不入5ms）
extern double ele_out;                // 转向环输出值（5ms线程写，调试显示用）

extern double left_motor_param[3];    // 左轮 PID 参数 [KP, KI, KD]
extern double right_motor_param[3];   // 右轮 PID 参数 [KP, KI, KD]

extern double encoder_l, encoder_r;                             // 左右轮编码器数据
extern unsigned int encoder_ave;                                // 编码器平均值
extern double left_motor_duty, right_motor_duty;                // 左右电机占空比
extern double left_speed, right_speed, current_speed;   // 左右轮目标速度、基础速度
extern uint32_t mile;                  // 里程计数（5ms线程私有，主线程经Control_Get_Mile读取）

extern uint8_t run;                    // 运行标志（主线程维护，仅供屏幕/调试显示）
extern double speed_ramp;              // 速度缓变步长（加减速共用）
extern double target_speed;            // 目标速度

extern int follow_left;                // 循迹左边界标志位（5ms线程写，调试显示用）

//================================================================================
// 函数声明
//================================================================================
double bias_calculate();        // 偏差计算
void dir_control();             // 方向环控制
void motor_control();           // 电机控制
void Ultima_Control();          // 综合控制（5ms定时调用）

double place_pid_control(PID* sptr, double now_point, double set_point, double *turn_pid);    // 位置式PD
double pid_realize(PID* sptr, double actual_speed, double set_speed, double *motor_pid);      // 增量式PID

#endif
