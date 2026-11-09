#include "../support/fixtures/response_io.h"
int main(void)
{
 st_row_t row;vf_parser_t parser;char reply[VF_RESPONSE_CAPACITY];unsigned peak=0;
 setup(0);
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,450,3000,1550,now));
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,201,3000,1550,now));
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,200,3001,1550,now));
 CHECK(!vision_drive_motion_start(VD_MOTION_SHUTTLE,200,3000,1550,now));
 CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,3000,1600,now));
 CHECK(!vision_drive_servo_probe(1550,700,now));CHECK(!vision_drive_motor_probe(1,200,1,700,now));
 CHECK(!vision_drive_motion_reverse(now,1));CHECK(!vd_output_arm(now,0));
 CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));
 for(unsigned i=0;i<325;++i){tick(-1,1,1);if(pwm[0]>peak)peak=pwm[0];CHECK(pwm[0]<=2000&&pwm[1]<=2000);}
 CHECK(peak==2000&&!vision_drive_motion_active()&&!pwm[0]&&!pwm[1]);freeze();
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,250,1000,1550,now));
 CHECK(!st_release(trace_info().generation+1));CHECK(st_release(trace_info().generation));
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,251,1000,1550,now));
 CHECK(vision_drive_motion_start(VD_MOTION_LINE,250,1000,1550,now));vision_drive_stop();freeze();
 /* Full requested ceiling is tested only by a test-controlled prior-stage
  * fixture. No UART/runtime route can skip the <=50permille admission gate. */
 setup(1);vd.motion_last_permille=550;
 CHECK(!vision_drive_motion_start(VD_MOTION_LINE,601,3000,1550,now));
 CHECK(vision_drive_motion_start(VD_MOTION_LINE,600,3000,1550,now));
 run_ticks(250,-10,10,1);CHECK(!info().faults);CHECK(info().target_right==23&&info().target_left==23);
 CHECK(pwm[0]<=6000&&pwm[1]<=6000);vision_drive_stop();CHECK(!pwm[0]&&!pwm[1]);freeze();
 CHECK(st_row(100,&row));CHECK(row.enabled&&row.pi_seq<=row.seq&&row.interval==10);
 CHECK(row.pi_ms<=row.ms&&row.target_r>=0&&row.target_r<=23);
 {FILE *f=fopen("response-native.bin","wb");uint32_t head[5]={0x31505352U,trace_info().generation,trace_info().count,trace_info().stop_ms,st_crc()};CHECK(f!=NULL);
  CHECK(fwrite(head,sizeof(head),1,f)==1);
  {uint32_t cfg[ST_CONFIG_FIELDS];st_get_config(cfg);CHECK(fwrite(cfg,sizeof(cfg),1,f)==1);}
  for(unsigned i=0;i<trace_info().count;++i){uint32_t v[ST_FIELDS];CHECK(st_row(i,&row));st_values(&row,v);CHECK(fwrite(v,sizeof(v),1,f)==1);}
  CHECK(fclose(f)==0);}
 vf_init(&parser);for(const char *s="@response_info\n";*s;++s)(void)vf_feed(&parser,(uint8_t)*s,now,reply);
 CHECK(strstr(reply,"[RESPONSE_INFO]")&&strstr(reply,"crc="));
 for(const char *s="@response_row 100\n";*s;++s)(void)vf_feed(&parser,(uint8_t)*s,now,reply);
 CHECK(strstr(reply,"[RESPONSE_ROW]")&&strstr(reply,"i=100 v="));
 /* Closed-loop output cannot exceed a lower run cap despite profile=.6. */
 setup(1);CHECK(vision_drive_speed_config(60,5,.2f));CHECK(vision_drive_speed_enable(1));
 CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));peak=0;
 for(unsigned i=0;i<120;++i){tick(-1,1,1);if(pwm[0]>peak)peak=pwm[0];CHECK(pwm[0]<=2000&&pwm[1]<=2000);}
 CHECK(peak==2000&&info().target_right==60&&!info().faults);vision_drive_stop();freeze();
 setup(0);vd.motion_last_permille=550;CHECK(vision_drive_motion_start(VD_MOTION_LINE,600,3000,1550,now));peak=0;
 for(unsigned i=0;i<325;++i){tick(-1,1,1);if(pwm[0]>peak)peak=pwm[0];CHECK(pwm[0]<=6000&&pwm[1]<=6000);}
 CHECK(peak==6000);freeze();
 setup(0);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(25,0,0,1);
 tick(-121,121,1);CHECK(info().fault_detail==12&&!pwm[0]&&!pwm[1]);freeze();
 setup(0);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(25,0,0,1);
 run_ticks(3,5,-5,1);CHECK(info().faults&&!pwm[0]&&!pwm[1]);freeze();
 setup(0);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(160,0,0,1);
 CHECK(info().faults&&!pwm[0]&&!pwm[1]);freeze();
 setup(0);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(25,0,0,1);
 run_ticks(115,-100,100,1);CHECK(!vision_drive_motion_active()&&!pwm[0]&&!pwm[1]);CHECK(trace_info().reason==12);freeze();
 setup(1);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(25,-2,2,1);
 now+=21;vision_drive_safety_tick();CHECK(info().faults&&!pwm[0]&&!pwm[1]);freeze();
 setup(1);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(25,-2,2,1);
 for(unsigned i=0;i<5;++i){now+=10;vision_drive_safety_tick();}CHECK(info().faults&&!pwm[0]&&!pwm[1]);freeze();
 setup(0);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(60,0,0,0);
 CHECK(!vision_drive_motion_active()&&!pwm[0]&&!pwm[1]);freeze();
 setup(0);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));run_ticks(25,0,0,1);
 app_mode_sample(1,1,now);vision_drive_safety_tick();vision_drive_poll(now);
 CHECK(!pwm[0]&&!pwm[1]);freeze();
 CHECK(!mask);printf("RESPONSE PASS %u assertions; simulated IO, not physical response\n",checks);return 0;
}
