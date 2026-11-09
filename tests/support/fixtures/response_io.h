#ifndef SMARTCAR_RESPONSE_IO_H
#define SMARTCAR_RESPONSE_IO_H
/* Actual isolated candidate modules with fake counters/timer/PWM only. */
#include "speed_helpers.h"
#include "vision_drive_internal.h"
#include "stop_trace.h"
void board_buzzer_set(int on){CHECK(on==0||on==1);}
static void setup(int closed)
{
 vd_profile_t p;now=0;raw3=raw4=0;mask=0;memset(pwm,0,sizeof(pwm));
 vision_drive_init(0);app_mode_init(0,0,0);app_mode_sample(0,0,30);(void)app_mode_take_change();
 vision_drive_get_profile(&p);p.servo_center_us=1550;p.servo_min_us=1430;p.servo_max_us=1670;
 p.right_sign=1;p.hardware_confirmed=1;p.motor_direction_confirmed=1;
 p.motor1_forward_level=p.motor2_forward_level=1;p.motor1_scale=p.motor2_scale=1;
 p.control.cruise_duty=0;p.control.duty_cap=.6f;CHECK(vision_drive_configure(&p));
 tick(0,0,0);CHECK(vision_drive_encoder_signs(-1,1));
 CHECK(!vision_drive_speed_config(61,2,.1f));CHECK(!vision_drive_speed_config(23,6,.1f));
 CHECK(!vision_drive_speed_config(23,2,.3f));CHECK(vision_drive_speed_config(23,2,.1f));
 CHECK(vision_drive_speed_enable(closed));
}
static st_info_t trace_info(void){st_info_t x;st_info(&x);return x;}
static void run_ticks(unsigned count,int left,int right,int heartbeat)
{for(unsigned i=0;i<count;++i){tick(left,right,heartbeat);CHECK(pwm[0]<=6000&&pwm[1]<=6000);}}
static void freeze(void){run_ticks(220,0,0,0);CHECK(trace_info().frozen&&!trace_info().overflow);}
#endif
