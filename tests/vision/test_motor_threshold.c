#include "main.h"
#include "app_mode.h"
#include "vision_drive.h"
#include "vision_field.h"
#include <stdlib.h>
#undef printf
static uint32_t clock_ms,pwm[3],mask;
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"threshold:%d %s\n",__LINE__,#x);exit(1);} } while(0)
uint32_t timer_get(int n){(void)n;return clock_ms;}
uint32_t __get_PRIMASK(void){return mask;}
void __disable_irq(void){mask=1;}
void __set_PRIMASK(uint32_t n){mask=n;}
void __DMB(void){}
void pwm_set_duty(int c,uint32_t n){CHECK(c>=0&&c<3&&n<=10000);pwm[c]=n;}
void gpio_set_level(int p,int v){CHECK(p==MOTOR1_DIR||p==MOTOR2_DIR);CHECK(v==0||v==1);CHECK(!pwm[0]&&!pwm[1]);}
void pit_ms_init(int c,uint32_t n){CHECK(c==1&&n==10);}
void interrupt_set_priority(int i,uint8_t p){CHECK(i==7&&p==0);}
static void init(void)
{
    clock_ms=0;mask=0;memset(pwm,0,sizeof(pwm));vision_drive_init(0);
    app_mode_init(1,0,0);app_mode_sample(1,0,30);(void)app_mode_take_change();
    CHECK(!pwm[0]&&!pwm[1]&&!pwm[2]);
}
static int pulse(unsigned duty,unsigned ms)
{return vision_drive_motor_probe(1,(uint16_t)duty,1,(uint16_t)ms,clock_ms);}
static void field(vf_parser_t *p,const char *s,char *r)
{while(*s)(void)vf_feed(p,(uint8_t)*s++,clock_ms,r);}
int main(void)
{
    vf_parser_t parser;char reply[VF_RESPONSE_CAPACITY];
    unsigned start=VISION_MOTOR_PROBE_INITIAL_PERMILLE;
    unsigned duration=VISION_MOTOR_PROBE_MAX_MS;
    unsigned motion_max=VISION_MOTION_MAX_PERMILLE;
    CHECK(motion_max==100U); /* Whole-vehicle limit unchanged. */
#if VISION_MOTOR_THRESHOLD_DIAG
    CHECK(start==100&&duration==1500&&VISION_MOTOR_PROBE_MAX_PERMILLE==200);
#else
    CHECK(start==30&&duration==500&&VISION_MOTOR_PROBE_MAX_PERMILLE==100);
#endif
    init();CHECK(!pulse(start+10,duration));CHECK(!pulse(start,duration+1));
    CHECK(pulse(start,duration));CHECK(pwm[0]==start*10U&&!pwm[1]&&!pwm[2]);
    clock_ms=duration-1;vision_drive_safety_tick();CHECK(pwm[0]==start*10U);
    clock_ms=duration;vision_drive_safety_tick();CHECK(!pwm[0]);
    clock_ms=duration+VISION_MOTOR_PROBE_COAST_MS-1;CHECK(!pulse(start,duration));
    ++clock_ms;CHECK(!pulse(start+11,duration));
    for(unsigned duty=start+10;duty<=VISION_MOTOR_PROBE_MAX_PERMILLE;duty+=10){
        CHECK(pulse(duty,duration));CHECK(pwm[0]==duty*10U&&!pwm[1]&&!pwm[2]);
        clock_ms+=duration;vision_drive_safety_tick();CHECK(!pwm[0]);
        clock_ms+=VISION_MOTOR_PROBE_COAST_MS;
    }
    CHECK(!pulse(VISION_MOTOR_PROBE_MAX_PERMILLE+1,duration));
    CHECK(!vision_drive_motor_probe(2,(uint16_t)(start+10),0,(uint16_t)duration,clock_ms));
    CHECK(vision_drive_motor_probe(2,(uint16_t)start,0,(uint16_t)duration,clock_ms));
    CHECK(!pwm[0]&&pwm[1]==start*10U&&!pwm[2]);vision_drive_stop();CHECK(!pwm[1]);
    init();vf_init(&parser);parser.probe_permitted=1;
#if VISION_MOTOR_THRESHOLD_DIAG
    field(&parser,"@wheel 1 100 1 1500\n",reply);CHECK(strstr(reply,"OK")!=NULL&&pwm[0]==1000);
    field(&parser,"!",reply);CHECK(!pwm[0]);
    {vd_profile_t p;vd_auto_info_t a;vision_drive_get_profile(&p);p.autostart_enabled=1;
     CHECK(vision_drive_configure(&p));vision_drive_schedule_boot(0,0);vision_drive_get_auto(&a);
     CHECK(a.state!=VD_AUTO_WAITING&&a.state!=VD_AUTO_STARTED);}
#else
    field(&parser,"@wheel 1 100 1 1500\n",reply);CHECK(strstr(reply,"REJECT")!=NULL&&!pwm[0]);
#endif
    init();CHECK(pulse(start,duration));vision_drive_trip(VC_FAULT_EXTERNAL);CHECK(!pwm[0]);
    clock_ms=10000;CHECK(!pulse(start,duration));
    init();CHECK(pulse(start,duration));app_mode_init(0,0,0);app_mode_sample(0,0,30);
    vision_drive_safety_tick();CHECK(!pwm[0]);
    init();clock_ms=UINT32_MAX-200U;CHECK(pulse(start,duration));
    clock_ms+=duration;vision_drive_safety_tick();CHECK(!pwm[0]);
    clock_ms+=10000;vision_drive_poll(clock_ms);CHECK(!pwm[0]&&!pwm[1]&&!pwm[2]&&!mask);
    printf("THRESHOLD diag=%u PASS %u checks; mock IO only\n",(unsigned)VISION_MOTOR_THRESHOLD_DIAG,checks);
    return 0;
}
