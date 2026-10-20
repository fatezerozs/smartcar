#include "app_mode.h"
#include "vision_drive.h"
#include "vision_field.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t clock_ms,pwm[3],irq_mask;static unsigned checks;
static int last_level;
static vf_parser_t parser;static char reply[VF_RESPONSE_CAPACITY];
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL wheel line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t timer_get(int c){(void)c;return clock_ms;}
uint32_t __get_PRIMASK(void){return irq_mask;}
void __disable_irq(void){irq_mask=1;}
void __set_PRIMASK(uint32_t x){irq_mask=x;}
void __DMB(void){}
void pwm_set_duty(int c,uint32_t d){CHECK(c>=0&&c<3&&d<=10000);pwm[c]=d;}
void gpio_set_level(int p,int v){(void)p;CHECK(v==0||v==1);last_level=v;}
void pit_ms_init(int c,uint32_t ms){CHECK(c==1&&ms==10);}
void interrupt_set_priority(int i,uint8_t p){CHECK(i==7&&p==0);}
static void command(const char *s){for(;*s;++s){int n=vf_feed(&parser,(uint8_t)*s,clock_ms,reply);CHECK(n!=0||*s=='\r'||*s=='\n');}}
int main(void)
{
    vd_profile_t cfg;vd_probe_info_t info;vision_result_t lane;int i;
    vision_drive_init(0); app_mode_init(1,0,0); app_mode_sample(1,0,30); (void)app_mode_take_change();vf_init(&parser);parser.probe_permitted=1;
    CHECK(!vision_drive_motor_probe(0,10,1,300,0));
    CHECK(!vision_drive_motor_probe(1,40,1,300,0));
    command("@wheel 1 30 1 300\r\n");CHECK(strstr(reply,"OK")!=NULL);
    CHECK(pwm[0]==300&&pwm[1]==0&&pwm[2]==0&&last_level==1);
    memset(&lane,0,sizeof(lane));clock_ms=20;vision_drive_observe(&lane,1,clock_ms,clock_ms);
    CHECK(pwm[0]==300&&pwm[1]==0&&pwm[2]==0); /* Invalid camera doesn't fabricate driving. */
    CHECK(!vision_drive_command('h',clock_ms,1));
    clock_ms=299;vision_drive_safety_tick();CHECK(pwm[0]==300);
    clock_ms=300;vision_drive_safety_tick();CHECK(pwm[0]==0);
    vision_drive_get_probe(&info);CHECK(info.sequence==1&&info.kind==2&&info.channel==1&&info.value==30&&!info.active);
    CHECK(!vision_drive_motor_probe(2,10,0,300,clock_ms)); /* Independent coast interval. */
    clock_ms=2300;CHECK(vision_drive_motor_probe(2,10,0,300,clock_ms));
    CHECK(pwm[0]==0&&pwm[1]==100&&pwm[2]==0&&last_level==0);
    vision_drive_stop();CHECK(pwm[1]==0);
    clock_ms=4600;CHECK(!vision_drive_motor_probe(1,41,1,300,clock_ms));
    CHECK(vision_drive_motor_probe(1,40,1,300,clock_ms));
    vision_drive_trip(VC_FAULT_EXTERNAL);CHECK(pwm[0]==0);
    CHECK(!vision_drive_motor_probe(1,40,1,300,clock_ms));
    CHECK(vision_drive_command('c',clock_ms,1));
    clock_ms=6900;CHECK(vision_drive_motor_probe(1,40,1,300,clock_ms));
    clock_ms+=300;vision_drive_poll(clock_ms);CHECK(pwm[0]==0);
    command("@servo 1550 1500 1600 -1\n");
    command("@camera 0.5 0.5\n");command("@motor 0.1 0.2 0.1\n");
    CHECK(!vision_drive_ready()); /* Servo calibration must not confirm motor polarity. */
    command("@motor_dir 1 0\n");CHECK(vision_drive_ready());
    CHECK(!vision_drive_motor_probe(1,40,1,300,clock_ms)); /* Positive driving profile blocks bench. */
    memset(&lane,0,sizeof(lane));lane.valid=1;lane.status=VISION_TRACK;lane.confidence=90;
    for(i=VISION_ROI_TOP;i<=VISION_ROI_BOTTOM;++i){lane.left[i]=64;lane.right[i]=123;lane.center[i]=94;lane.row_flags[i]=3;}
    for(i=2;i<=7;++i){clock_ms+=20;vision_drive_observe(&lane,(uint32_t)i,clock_ms,clock_ms);}
    CHECK(!vision_drive_command('a',clock_ms,1)); /* Fresh vision cannot bypass motor coast. */
    app_mode_init(0,0,clock_ms-30U); app_mode_sample(0,0,clock_ms); (void)app_mode_take_change();
    clock_ms=9400;
    for(i=8;i<=13;++i){clock_ms+=20;vision_drive_observe(&lane,(uint32_t)i,clock_ms,clock_ms);}
    CHECK(vision_drive_command('a',clock_ms,1));
    clock_ms+=20;vision_drive_observe(&lane,14,clock_ms,clock_ms);CHECK(last_level==0);
    vision_drive_stop();vision_drive_get_profile(&cfg);CHECK(cfg.motor_direction_confirmed&&cfg.motor2_forward_level==0);
    command("@status\n");CHECK(strstr(reply,"[MOTOR] confirmed=1 dir=1/0")!=NULL&&strstr(reply,"[BENCH]")!=NULL);
    vision_drive_init(clock_ms); app_mode_init(1,0,0); app_mode_sample(1,0,30); (void)app_mode_take_change();clock_ms=UINT32_MAX-200;
    CHECK(vision_drive_motor_probe(1,10,1,300,clock_ms));
    clock_ms+=300;vision_drive_safety_tick();CHECK(pwm[0]==0);
    vision_drive_get_profile(&cfg);CHECK(!cfg.motor_direction_confirmed);
    CHECK(irq_mask==0);
    printf("MOTOR PROBE PASS: %u checks; no physical IO\n",checks);return 0;
}
