#include "zf_common_headfile.h"
#include "img_process.h"
#include "Camera.h"
#include "Island.h" 

// 编码器引脚信息
// 请确保两轮编码器前进时回传值都为正数，若回传为负请isr.c中encoder_get_count前修改正负号
// 左轮编码器
#define ENCODER_1 (QTIMER1_ENCODER1)
#define ENCODER_1_A (QTIMER1_ENCODER1_CH1_C0)
#define ENCODER_1_B (QTIMER1_ENCODER1_CH2_C1)
// 右轮编码器
#define ENCODER_2 (QTIMER1_ENCODER2)
#define ENCODER_2_A (QTIMER1_ENCODER2_CH1_C2)
#define ENCODER_2_B (QTIMER1_ENCODER2_CH2_C24)

// 电机参数设置
// !!! 请确保dir和pwm脚不要弄反了，否则电机会疯转 !!!
// 当前样机实际接线：M1 对应右轮，M2 对应左轮
// 这里保留现有实际映射，不做软件重排；若需要调整前后方向，只改 FORWARD_DIR_LEVEL
// 右轮电机（当前实际接线：PWM=C8，DIR=C9）
#define MOTOR1_PWM (PWM2_MODULE0_CHA_C6)
#define MOTOR1_DIR (C7)
// 右轮电机前进需要的DIR脚的电平 (GPIO_HIGH or GPIO_LOW) 请自行测试
#define MOTOR1_FORWARD_DIR_LEVEL (GPIO_HIGH)
// 左轮电机（当前实际接线：PWM=C6，DIR=C7）
#define MOTOR2_PWM (PWM2_MODULE1_CHA_C8)
#define MOTOR2_DIR (C9)
// 左轮电机前进需要的DIR脚的电平 (GPIO_HIGH or GPIO_LOW) 请自行测试
#define MOTOR2_FORWARD_DIR_LEVEL (GPIO_HIGH)


// 舵机参数设置
#define SERVO_MOTOR_PWM (PWM4_MODULE2_CHA_C30) // 定义主板上舵机对应引脚
#define SERVO_MOTOR_FREQ (50)                  // 定义主板上舵机频率  请务必注意范围 50-300
#define SERVO_MOTOR_L_MAX (103)  // 定义主板上舵机活动范围(左打方向的极限值) 角度 自行标定
#define SERVO_MOTOR_R_MAX (78) // 定义主板上舵机活动范围(右打方向的极限值) 角度 自行标定
#define SERVO_DIR (SERVO_MOTOR_L_MAX>SERVO_MOTOR_R_MAX?-1.f:1.f) // 根据左右duty的大小自动决定舵机方向

extern float theta;

//常用基本变量
extern uint8 image_two_value[MT9V03X_H][MT9V03X_W];//二值化后的原数组
extern uint8_t Left_Line[MT9V03X_H]; //左边线数组
extern uint8_t Right_Line[MT9V03X_H];//右边线数组
extern uint8_t Mid_Line[MT9V03X_H];//中线数组
extern uint8_t Road_Wide[MT9V03X_H]; //赛宽数组
extern uint8_t White_Column[MT9V03X_W];    //每列白列长度
extern uint8_t Search_Stop_Line;     //搜索截止行
extern uint8_t Boundry_Start_Left;   //左右边界起始点
extern uint8_t Boundry_Start_Right;  //第一个非丢线点,常规边界起始点
extern uint8_t Left_Lost_Time;       //边界丢线数
extern uint8_t Right_Lost_Time; 
extern uint8_t Both_Lost_Time;//两边同时丢线数
//十字
extern uint8_t Cross_Flag; 
//环岛
extern uint8_t Island_State;     //环岛状态标志
extern uint8_t Left_Island_Flag; //左右环岛标志
extern uint8_t Right_Island_Flag;//左右环岛标志

extern uint8_t Zebra_Flag;

// 以下宏在初步测试例程时不可更改，后面若需更改舵机算法可以自行更改
// ------------------ 舵机占空比计算方式 ------------------
//
// 舵机对应的 0-180 活动角度对应 控制脉冲的 0.5ms-2.5ms 高电平
//
// 那么不同频率下的占空比计算方式就是
// PWM_DUTY_MAX/(1000/freq)*(1+Angle/180) 在 50hz 时就是 PWM_DUTY_MAX/(1000/50)*(1+Angle/180)
//
// 那么 100hz 下 90度的打角 即高电平时间1.5ms 计算套用为
// PWM_DUTY_MAX/(1000/100)*(1+90/180) = PWM_DUTY_MAX/10*1.5
//
// ------------------ 舵机占空比计算方式 ------------------
#define SERVO_MOTOR_DUTY(x) ((float)PWM_DUTY_MAX / (1000.0 / (float)SERVO_MOTOR_FREQ) * (0.5 + (float)(x) / 90.0))

#if (SERVO_MOTOR_FREQ < 50 || SERVO_MOTOR_FREQ > 300)
#error "SERVO_MOTOR_FREQ ERROE!"
#endif
