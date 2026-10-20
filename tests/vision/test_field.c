#include "app_mode.h"
#include "vision_field.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t clock_ms, pwm[3], irq_mask;
static unsigned checks;
static vf_parser_t parser;
static char response[VF_RESPONSE_CAPACITY];
static vision_result_t lane;
#define CHECK(x) do {++checks; if(!(x)){fprintf(stderr,"FAIL field line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t timer_get(int c) {(void)c;return clock_ms;}
uint32_t __get_PRIMASK(void) {return irq_mask;}
void __disable_irq(void) {irq_mask=1;}
void __set_PRIMASK(uint32_t x) {irq_mask=x;}
void __DMB(void) {}
void pwm_set_duty(int c,uint32_t d) {CHECK(c>=0&&c<3&&d<=10000);pwm[c]=d;}
void gpio_set_level(int p,int v) {(void)p;CHECK(v==1);}
void pit_ms_init(int c,uint32_t p) {CHECK(c==1&&p==10);}
void interrupt_set_priority(int i,uint8_t p) {CHECK(i==7&&p==0);}

static void command(const char *text)
{
    response[0]=0;
    for (;*text;++text) {int consumed=vf_feed(&parser,(uint8_t)*text,clock_ms,response);CHECK(consumed!=0);}
}
static void frame(unsigned sequence)
{
    clock_ms+=20;vision_drive_observe(&lane,sequence,clock_ms,clock_ms);
}
int main(void)
{
    vd_profile_t cfg,saved;int y;unsigned i;
    vf_init(&parser);vision_drive_init(0); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();
    memset(&lane,0,sizeof(lane));lane.valid=1;lane.status=VISION_TRACK;lane.confidence=90;
    for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){lane.left[y]=64;lane.right[y]=123;lane.center[y]=94;lane.row_flags[y]=3;}
    CHECK(!vision_drive_ready());
    command("@status\n");CHECK(strstr(response,VISION_FIELD_BUILD)!=NULL);
    command("@camera 0.5 0.5\n");CHECK(strstr(response,"OK")!=NULL);
    command("@servo 1500 1200 1800 1\r");CHECK(strstr(response,"OK")!=NULL);
    command("@motor 0.1 0.2 0.1\n");CHECK(!vision_drive_ready());
    command("@motor_dir 1 2\n");CHECK(strstr(response,"REJECT")!=NULL&&!vision_drive_ready());
    command("@motor_dir 1 1\n");CHECK(vision_drive_ready());
    command("@balance 1 0.8\n");CHECK(strstr(response,"OK")!=NULL && vision_drive_ready());
    command("@status\n");CHECK(strstr(response,"scale=1.0000/0.8000")!=NULL);
    CHECK(strstr(response,"active=0\r\n")!=NULL); /* Full readback fits, no truncation. */
    vision_drive_get_profile(&saved);
    command("@balance 1.01 1\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@balance 1 -0.1\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@balance nan 1\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@balance 1 inf\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@balance 1 1 a\n");CHECK(strstr(response,"REJECT")!=NULL);
    vision_drive_get_profile(&cfg);CHECK(cfg.motor1_scale==saved.motor1_scale && cfg.motor2_scale==saved.motor2_scale);
    command("@balance 0 1\n");CHECK(strstr(response,"OK")!=NULL && !vision_drive_ready());
    command("@balance 1 0.8\n");CHECK(vision_drive_ready());
    command("@bands 0.86 0.68 0.035\n");CHECK(strstr(response,"OK")!=NULL);
    CHECK(!vision_drive_ready()); /* Changing rows invalidates the old camera axis. */
    vision_drive_get_profile(&saved);
    CHECK(saved.control.near_y==.86f && saved.control.far_y==.68f);
    command("@status\n");CHECK(strstr(response,"[BANDS] near=0.8600 far=0.6800")!=NULL);
    command("@bands 0.68 0.86 0.035\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@bands 0.90 0.68 0.10\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@bands 0.86 0.84 0.035\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@bands 0.86 nan 0.035\n");CHECK(strstr(response,"REJECT")!=NULL);
    vision_drive_get_profile(&cfg);CHECK(cfg.control.near_y==saved.control.near_y && cfg.control.far_y==saved.control.far_y);
    CHECK(!cfg.control.calibration_confirmed);
    command("@camera 0.5 0.5\n");CHECK(vision_drive_ready());
    CHECK(vision_drive_output()->state==VC_DISARMED&&pwm[0]==0&&pwm[2]==0);
    vision_drive_get_profile(&saved);
    command("@motor 0.3 0.2 0.1\n");CHECK(strstr(response,"REJECT")!=NULL);
    vision_drive_get_profile(&cfg);CHECK(cfg.control.cruise_duty==saved.control.cruise_duty);
    command("@camera nan 0.5\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@gain 0.8 0.02 0.2 a\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@servo 1500.5 1200 1800 1\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@servo 1500 1600 1800 1\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@duration 30000\n");CHECK(strstr(response,"OK")!=NULL);
    vision_drive_get_profile(&cfg);CHECK(cfg.control.max_run_ms==30000);
    for(i=1;i<=5;++i)frame(i);
    CHECK(vision_drive_command('a',clock_ms,1));frame(6);CHECK(pwm[0]>0);
    CHECK(pwm[1]==(uint32_t)(vision_drive_output()->duty*.8f*10000));
    command("@balance 0.9 1\n");CHECK(strstr(response,"OK")!=NULL && pwm[0]==0 && pwm[1]==0);
    CHECK(!vision_drive_command('a',clock_ms,1));
    /* Restore without reusing pre-change arm evidence. */
    command("@balance 1 0.8\n");
    CHECK(vf_feed(&parser,'@',clock_ms,response)==1);CHECK(pwm[0]==0&&pwm[2]==0);
    command("gain 0.8 0.02 0.2\n");CHECK(strstr(response,"OK")!=NULL);
    CHECK(!vision_drive_command('a',clock_ms,1)); /* 参数更新清除旧帧资格。 */
    for(i=7;i<=11;++i)frame(i);
    CHECK(vision_drive_command('a',clock_ms,1));frame(12);
    /* @duration 的新值必须同时到达前台控制器和 PIT1，不能仍在 5 秒误停。 */
    for(i=13;i<=312;++i) {
        CHECK(vision_drive_command('h',clock_ms,1)); frame(i);
        vision_drive_safety_tick(); CHECK(vision_drive_output()->state==VC_RUNNING);
    }
    cfg.control.kp=.9f;CHECK(!vision_drive_configure(&cfg));CHECK(pwm[0]>0);
    CHECK(vf_feed(&parser,'!',clock_ms,response)==2);CHECK(pwm[0]==0);
    command("@gain 0.8");clock_ms+=VISION_FIELD_TIMEOUT_MS;
    command(" 0.02 0.2 a\n");CHECK(strstr(response,"REJECT")!=NULL);
    CHECK(vision_drive_output()->state==VC_DISARMED);
    command("@");for(i=0;i<150;++i)CHECK(vf_feed(&parser,'a',clock_ms,response)==1);
    command("\n");CHECK(strstr(response,"REJECT")!=NULL);CHECK(pwm[0]==0);
    vision_drive_trip(VC_FAULT_EXTERNAL);
    command("@balance 1 1\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@gain 0.5 0.02 0.2\n");CHECK(strstr(response,"REJECT")!=NULL);
    CHECK(vision_drive_output()->state==VC_FAULT);
    CHECK(vision_drive_command('c',clock_ms,1));
    vision_drive_get_profile(&cfg);cfg.control.frame_timeout_ms=200;
    CHECK(!vision_drive_configure(&cfg));
    command("@gain 0.5 0.02 0.2\n");CHECK(strstr(response,"OK")!=NULL);
    CHECK(irq_mask==0);
    vision_drive_init(clock_ms); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();CHECK(!vision_drive_ready());
    vision_drive_get_profile(&cfg);CHECK(!cfg.control.calibration_confirmed&&cfg.control.cruise_duty==0);
    CHECK(cfg.motor1_scale==1 && cfg.motor2_scale==1 && !cfg.motor_direction_confirmed);
    printf("FIELD PASS: %u checks; no physical IO\n",checks);return 0;
}
