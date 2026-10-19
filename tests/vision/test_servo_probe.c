#include "vision_drive.h"
#include "vision_field.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t clock_ms,pwm[3],irq_mask;
static unsigned checks,nonzero_motor;
static vf_parser_t parser;
static char response[VF_RESPONSE_CAPACITY];
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL probe line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t timer_get(int c){(void)c;return clock_ms;}
uint32_t __get_PRIMASK(void){return irq_mask;}
void __disable_irq(void){irq_mask=1;}
void __set_PRIMASK(uint32_t m){irq_mask=m;}
void __DMB(void){}
void pwm_set_duty(int c,uint32_t d){CHECK(c>=0&&c<3&&d<=10000);if(c<2&&d)++nonzero_motor;pwm[c]=d;}
void gpio_set_level(int p,int v){(void)p;(void)v;}
void pit_ms_init(int c,uint32_t ms){CHECK(c==1&&ms==10);}
void interrupt_set_priority(int irq,uint8_t p){CHECK(irq==7&&p==0);}
static void command(const char *s){for(;*s;++s)CHECK(vf_feed(&parser,(uint8_t)*s,clock_ms,response)!=0);}
int main(void)
{
    vd_profile_t before,after;
    vision_result_t invalid;
    memset(&invalid,0,sizeof(invalid));
    vision_drive_init(0);vf_init(&parser);vision_drive_get_profile(&before);
    parser.probe_permitted=1; /* Simulate explicit text-mode permission. */
    CHECK(!vision_drive_servo_probe(1550,500,clock_ms)); /* First action must be nominal anchor. */
    command("@probe 1500 700 a\n");CHECK(strstr(response,"REJECT")!=NULL&&pwm[2]==0);
    command("@probe nan 700\n");CHECK(strstr(response,"REJECT")!=NULL);
    command("@probe 1500 700\n");CHECK(strstr(response,"OK")!=NULL&&pwm[2]==750);
    CHECK(pwm[0]==0&&pwm[1]==0&&!vision_drive_command('a',clock_ms,1));
    clock_ms=20;vision_drive_observe(&invalid,1,clock_ms,clock_ms);
    CHECK(pwm[2]==750&&vision_drive_output()->state==VC_DISARMED);
    clock_ms=699;vision_drive_safety_tick();CHECK(pwm[2]==750);
    CHECK(!vision_drive_command('h',clock_ms,1)); /* Drive heartbeat cannot prolong probe. */
    clock_ms=700;vision_drive_safety_tick();CHECK(pwm[2]==0); /* ISR alone, foreground blocked. */
    CHECK(!vision_drive_servo_probe(1600,700,clock_ms));
    CHECK(!vision_drive_servo_probe(1550,1001,clock_ms));
    CHECK(vision_drive_servo_probe(1550,500,clock_ms));
    vision_drive_stop();CHECK(pwm[2]==0);
    CHECK(vision_drive_servo_probe(1500,500,clock_ms));
    command("!");CHECK(pwm[2]==0);
    parser.probe_permitted=1;
    command("@probe 1500 ");parser.probe_line_permitted=0;
    command("500\n");CHECK(strstr(response,"REJECT")!=NULL&&pwm[2]==0);
    CHECK(vision_drive_servo_probe(1500,500,clock_ms));
    vision_drive_trip(VC_FAULT_EXTERNAL);CHECK(pwm[2]==0);
    CHECK(!vision_drive_servo_probe(1500,500,clock_ms));
    CHECK(vision_drive_command('c',clock_ms,1));
    CHECK(vision_drive_servo_probe(1500,500,clock_ms));
    clock_ms+=500;vision_drive_poll(clock_ms);CHECK(pwm[2]==0);
    vision_drive_get_profile(&after);
    CHECK(after.hardware_confirmed==before.hardware_confirmed&&after.servo_center_us==0);
    CHECK(after.control.calibration_confirmed==before.control.calibration_confirmed);
    after.control.duty_cap=.2f;after.control.cruise_duty=.1f;
    CHECK(vision_drive_configure(&after));CHECK(!vision_drive_servo_probe(1500,500,clock_ms));
    vision_drive_init(clock_ms);clock_ms=UINT32_MAX-300;
    CHECK(vision_drive_servo_probe(1500,500,clock_ms));
    clock_ms+=499;vision_drive_safety_tick();CHECK(pwm[2]==750);
    clock_ms+=1;vision_drive_safety_tick();CHECK(pwm[2]==0);
    CHECK(nonzero_motor==0&&irq_mask==0);
    printf("SERVO PROBE PASS: %u checks; no physical IO\n",checks);return 0;
}
