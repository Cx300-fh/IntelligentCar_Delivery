#ifndef __CONTROL_HPP
#define __CONTROL_HPP

#include "include.hpp"

//================================================================================
// 宏定义
//================================================================================
#define MOTOR_MAX    9000   // 电机占空比限幅
#define ELE_OUT_MAX  65     // 转向环输出限幅
#define BIAS_MAX     100    // 偏差限幅
#define SERVO_MID    1470   // 舵机中值（PWM占空比）

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
extern double ele_current;            // 转向环当前偏差值
extern double ele_out;                // 转向环输出值

extern double left_motor_param[3];    // 左轮 PID 参数 [KP, KI, KD]
extern double right_motor_param[3];   // 右轮 PID 参数 [KP, KI, KD]

extern double encoder_l, encoder_r;                             // 左右轮编码器数据
extern unsigned int encoder_ave;                                // 编码器平均值
extern double left_motor_duty, right_motor_duty;                // 左右电机占空比
extern double left_speed, right_speed, current_speed;   // 左右轮目标速度、基础速度
extern uint32_t mile;                  // 里程计数（编码器平均值积分）

extern uint8_t run;                    // 运行标志
extern double speed_ramp;              // 速度缓变步长（加减速共用）
extern double target_speed;            // 目标速度

//================================================================================
// 函数声明
//================================================================================
double bias_calculate();        // 偏差计算
void dir_control();             // 方向环控制
void motor_control();           // 电机控制
void Ultima_Control();          // 综合控制（中断调用）

double place_pid_control(PID* sptr, double now_point, double set_point, double *turn_pid);    // 位置式PD
double pid_realize(PID* sptr, double actual_speed, double set_speed, double *motor_pid);      // 增量式PID

#endif
