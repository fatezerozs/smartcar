#define main original_motion_main
#include "C:/shengchanli/smartCar/smartcar/.build/phase2-calibration-20261108/stop-trace-20261108T163350483315Z/test_motion_copy.c"
#undef main

int main(void) {
 original_motion_main();
 vd_profile_t p;
 diagnostic_init();vision_drive_get_profile(&p);p.control.duty_cap=0.20f;p.motor2_scale=1.0f;CHECK(vision_drive_configure(&p));
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,201,3000,1550,clock_ms));
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

 diagnostic_init();
 {vf_parser_t parser;char reply[VF_RESPONSE_CAPACITY];st_info_t trace;
 vf_init(&parser);parser.probe_permitted=1;field(&parser,"@stop_beep\n",reply);CHECK(strstr(reply,"BEEP OK")&&strstr(reply,"\r\n"));CHECK(st_marker(clock_ms));
 CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,clock_ms));advance(2700,1);st_info(&trace);CHECK(trace.stop_seen&&!trace.frozen);
 for(unsigned i=10;i<=2000;i+=10)st_sample(clock_ms+i,(clock_ms+i)/10,0,0,0,0,0,1);
 clock_ms+=2000;st_info(&trace);CHECK(trace.frozen&&!st_can_arm());CHECK(!vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,clock_ms));
 field(&parser,"@stop_info\n",reply);CHECK(strstr(reply,"[STOP_INFO]")&&strstr(reply,"frozen=1")&&strstr(reply,"\r\n"));
 field(&parser,"@stop_row 0\n",reply);CHECK(strstr(reply,"[STOP_ROW]")&&strstr(reply,"i=0 ")&&strstr(reply,"\r\n"));
 field(&parser,"@stop_release 99999\n",reply);CHECK(strstr(reply,"REJECT"));
 char command[80];snprintf(command,sizeof(command),"@stop_release %lu\n",(unsigned long)trace.generation);field(&parser,command,reply);CHECK(strstr(reply,"RELEASE OK")&&st_can_arm());
 CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,clock_ms));vision_drive_stop();}
 printf("startup200 bounds and original motion regression PASS: %u checks\n",checks);return 0;
}

void board_buzzer_set(int on){(void)on;}
