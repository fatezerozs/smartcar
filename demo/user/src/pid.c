#include "pid.h"

// 电机PID初始化参数
float speed_KP = 48, speed_KI = 1.5,speed_KD = 3.0, speed_IMAX = 1000.0, speed_OUTMAX = 5000.0;
float speed_TARGET = 140.0;

pid_param_t speed_pid_l;  // 电机PID
pid_param_t speed_pid_r;  // 电机PID

/*******************************************************************************
* 函 数 名         : My_Pid_Init
* 函数功能         : PID初始化（默认使能PID）
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void My_Pid_Init(void)
{
    Pid_Param_Init(&speed_pid_l, speed_KP, speed_KI,speed_KD, speed_IMAX, speed_OUTMAX);
    Pid_Param_Init(&speed_pid_r, speed_KP, speed_KI,speed_KD, speed_IMAX, speed_OUTMAX);
    
    // 初始化时使能PID，正常循迹时工作
    speed_pid_l.enable = true;
    speed_pid_r.enable = true;
}

/*******************************************************************************
* 函 数 名         : Pid_Param_Init
* 函数功能         : PID参数初始化	
* 输    入         : imax:积分项最大值
* 输    出         : 无
*******************************************************************************/
void Pid_Param_Init(pid_param_t * pid, float kp, float ki, float kd, float imax, float outmax)
{
    pid->kp        = kp;
    pid->ki        = ki;
    pid->kd        = kd;
    pid->imax      = imax;
    pid->out_p     = 0;
    pid->out_i     = 0;
    pid->out_d     = 0;
    pid->out       = 0;
    pid->outmax    = outmax;
    pid->integrator= 0;
    pid->last_error= 0;
    pid->last_derivative   = 0;
    pid->last_t    = 0;
		pid->target = 0;                     // 目标速度
		pid->real = 0;                        // 实际速度
		pid->pwm = 0;                         // 施加在电机上的PWM占空比
		pid->enable = false;                  // 初始禁用，My_Pid_Init中启用
}

/*******************************************************************************
* 函 数 名         : PidLocCtrl
* 函数功能	   : 位置式PID控制（添加使能判断）
* 输    入         : pid, error, t
* 输    出         : float
*******************************************************************************/
float  PidLocCtrl(pid_param_t * pid, float error, float t)
{
    // PID未使能时，输出0并清空积分器
    if (!pid->enable)
    {
        pid->out = 0;
        pid->pwm = 0;
        pid->integrator = 0;
        pid->last_error = 0;
        return 0;
    }

    /* 累积误差 */
    pid->integrator += error;

    pid->out_p = pid->kp * error;
    pid->out_i = pid->ki * t * pid->integrator;
    pid->out_d = pid->kd/t * (error - pid->last_error);
	
    /* 误差限幅 */
    pid->out_i = constrain_float(pid->out_i, -pid->imax, pid->imax);

    /* 更新积分器值为限幅后的等效值（抗积分饱和） */
    if (pid->ki * t != 0)
    {
        pid->integrator = pid->out_i / (pid->ki * t);
    }
	
    pid->last_error = error;

    pid->out = pid->out_p + pid->out_i + pid->out_d;

    pid->out = constrain_float(pid->out, -pid->outmax, pid->outmax);
    
    // 关联PWM值
    pid->pwm = pid->out;

    return pid->out;
}

/*******************************************************************************
* 函 数 名         : PidIncCtrl
* 函数功能	   : 增量式PID控制（添加使能判断）
* 输    入         : pid, error, t
* 输    出         : float
*******************************************************************************/
float PidIncCtrl(pid_param_t * pid, float error, float t)
{
    // PID未使能时，输出0并清空历史值
    if (!pid->enable)
    {
        pid->out = 0;
        pid->pwm = 0;
        pid->last_error = 0;
        pid->last_derivative = 0;
        return 0;
    }

    pid->out_p = pid->kp * (error - pid->last_error);
    pid->out_i = pid->ki * error * t ;
    pid->out_d = pid->kd/t * ((error - pid->last_error) - pid->last_derivative);

    pid->last_derivative = error - pid->last_error;
    pid->last_error = error;

    pid->out += pid->out_p + pid->out_i + pid->out_d;

    pid->out = constrain_float(pid->out, -pid->outmax, pid->outmax);
    
    // 关联PWM值
    pid->pwm = pid->out;

    return pid->out;
}

/*******************************************************************************
* 函 数 名         : constrain_float
* 函数功能         : 浮点型数限幅
* 输    入         : amt,low,high
* 输    出         : float
*******************************************************************************/
float constrain_float(float amt, float low, float high)
{
    return ((amt)<(low)?(low):((amt)>(high)?(high):(amt)));
}

/*******************************************************************************
* 函 数 名         : constrain_short
* 函数功能         : 短整型数限幅
* 输    入         : amt,low,high
* 输    出         : short
*******************************************************************************/
short constrain_short(short amt, short low, short high)
{
    return ((amt)<(low)?(low):((amt)>(high)?(high):(amt)));
}