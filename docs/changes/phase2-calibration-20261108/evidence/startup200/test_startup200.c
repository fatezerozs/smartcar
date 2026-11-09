#define main original_motion_main
#include "C:/shengchanli/smartCar/smartcar/tests/vision/test_motion.c"
#undef main

int main(void) {
 original_motion_main();
 vd_profile_t p;
 diagnostic_init();vision_drive_get_profile(&p);p.control.duty_cap=0.20f;p.motor2_scale=1.0f;CHECK(vision_drive_configure(&p));
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,151,3000,1550,clock_ms));
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,150,3001,1550,clock_ms));
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,201,3000,1550,clock_ms));
 for(unsigned request=150;request<=200;request+=10) {
  if(request>150)advance(2000,1);
  CHECK(vision_drive_motion_start(VD_MOTION_LINE,(uint16_t)request,3000,1550,clock_ms));
  advance(2300,1);CHECK(pwm[0]==request*10U&&pwm[1]==request*10U);
  advance(900,1);CHECK(info().phase==0&&info().reason==VD_MOTION_DONE&&!pwm[0]&&!pwm[1]&&!pwm[2]);
 }
 advance(2000,1);CHECK(!vision_drive_motion_start(VD_MOTION_LINE,201,3000,1550,clock_ms));
 CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,clock_ms));advance(2300,1);
 vision_drive_stop();CHECK(info().reason==VD_MOTION_STOP&&!pwm[0]&&!pwm[1]&&!pwm[2]);
 advance(2000,1);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,clock_ms));advance(2300,1);
 clock_ms+=500;vision_drive_safety_tick();CHECK(info().reason==VD_MOTION_HEARTBEAT&&!pwm[0]&&!pwm[1]&&!pwm[2]);
 advance(2000,1);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,clock_ms));advance(2300,1);
 app_mode_sample(1,1,clock_ms);vision_drive_safety_tick();CHECK(!pwm[0]&&!pwm[1]&&!pwm[2]);
 printf("startup200 bounds and original motion regression PASS: %u checks\n",checks);return 0;
}
