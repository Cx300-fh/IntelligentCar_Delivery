#include "control.hpp"

//================================================================================
// 全局变量定义
//================================================================================
uint8_t follow_row = 40;              // 循迹参考行

PID left_motor_pid = {0}, right_motor_pid = {0}, turn_pid = {0};

double turn_ele[2] = {1.5, 28.8};       // 转向环 PD 参数 [KP, KD]
double ele_target = 0;                // 目标偏差（0=居中）
double ele_current = 0;               // 当前偏差值（由 bias_calculate 更新）
double ele_out = 0;                   // 转向环输出

double left_motor_param[3] = {21, 8.5, 0};     // 左轮 PID 参数 [KP, KI, KD]
double right_motor_param[3] = {21, 8.5, 0};    // 右轮 PID 参数 [KP, KI, KD]

double encoder_l = 0, encoder_r = 0;
unsigned int encoder_ave = 0;
double left_motor_duty = 0, right_motor_duty = 0;
double left_speed = 0, right_speed = 0, current_speed = 0;
uint32_t mile = 0;                   // 里程计数（编码器平均值积分）

uint8_t run = 0;
double speed_ramp = 1;               // 速度缓变步长（加减速共用）
double target_speed = 20;             // 目标速度

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


double bias_calculate()  // 偏差计算
{
    double error_sum = 0;

    for(int i = follow_row - 3; i <= follow_row + 4; i++)
    {
        error_sum += border_msg[i].center_line - IMAGE_WIDTH / 2;
    }

    return RANGE_LIMIT(error_sum / 8.0, -BIAS_MAX, BIAS_MAX);
}


/**
 * @brief 方向环控制，计算左右轮目标速度
 */
void dir_control()
{
    double diff_ratio = 0;  // 差速比例

    /* 转向PD：error = target(0) - current(bias) */
    ele_out = place_pid_control(&turn_pid, ele_current, ele_target, turn_ele);
    ele_out = RANGE_LIMIT(ele_out, -80, 80);

    unsigned int servo_duty = SERVO_MID - ele_out * 2.5;

    // servo_pwm.gtim_pwm_set_duty(servo_duty);
    servo_pwm.gtim_pwm_set_duty(SERVO_MID);

    /* 速度设置 */
    if(run == 1)
    {
        current_speed = (current_speed < target_speed) ? current_speed + speed_ramp : target_speed;  // 速度缓加至目标速度
    }
    else  // 停止
    {
        current_speed = (current_speed > 0) ? current_speed - speed_ramp : 0;  // 速度缓减至0
        // current_speed = 0;  // 速度直接置零
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
}


/**
 * @brief 电机控制，左右轮速度闭环
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

    // left_motor_duty = -1000;
    // right_motor_duty = -0;

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
}


/**
 * @brief 综合控制，在中断中调用
 */
void Ultima_Control()
{
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
