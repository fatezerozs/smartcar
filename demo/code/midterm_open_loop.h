#ifndef MIDTERM_OPEN_LOOP_H
#define MIDTERM_OPEN_LOOP_H
#include <stdint.h>

/* 中期赛道：电机开环 PWM，摄像头转向仍由 vision_control 闭环计算。
 * 不读取编码器，不把 PWM 当作实测车速，不按时间/圈数预编程转弯。
 * 所有量均为无量纲比例，dt_s 为秒；配置由车辆实测后集中提供。 */
typedef struct {
    float lateral_weight, heading_weight, steering_weight;
    float missing_far_scale;
} mol_config_t;
void mol_default_config(mol_config_t *config);
int mol_config_valid(const mol_config_t *config);
/* 返回 1=有效计算，0=非法输入（*duty 保持零，应由上层锁存故障）。
 * cruise/cap/previous/quality 取 [0,1]，横向偏差以道路半宽归一化。
 * 加速限斜率；减速立即生效；有效帧/通信租约/人工解锁由上层负责。
 * 该函数不能检测堵转、轮胎打滑、电池欠压，不能提供速度闭环保证。 */
int mol_step(const mol_config_t *config, float lateral, float heading,
             float steering, float quality, int far_valid, float cruise,
             float cap, float rise_per_s, float previous, float dt_s, float *duty);
#endif
