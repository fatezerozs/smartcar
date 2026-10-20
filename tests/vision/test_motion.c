#include "main.h"
#include "app_mode.h"
#include "vision_drive.h"
#include "vision_field.h"
#include <stdlib.h>
#undef printf
static uint32_t clock_ms,pwm[3],mask;
static unsigned writes[3];
static int dirs[2];static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL motion:%d %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t timer_get(int n){(void)n;return clock_ms;}
uint32_t __get_PRIMASK(void){return mask;}
void __disable_irq(void){mask=1;}
void __set_PRIMASK(uint32_t n){mask=n;}
void __DMB(void){}
void pwm_set_duty(int ch,uint32_t value){CHECK(ch>=0&&ch<3&&value<=10000);pwm[ch]=value;++writes[ch];}
void gpio_set_level(int pin,int value)
{
    unsigned ch=pin==MOTOR1_DIR?0U:1U;
    CHECK(pin==MOTOR1_DIR || pin==MOTOR2_DIR);CHECK(value==0 || value==1);
    if(dirs[ch]!=value)CHECK(pwm[0]==0 && pwm[1]==0); /* Never reverse under power. */
    dirs[ch]=value;
}
void pit_ms_init(int ch,uint32_t ms){CHECK(ch==1&&ms==10);}
void interrupt_set_priority(int irq,uint8_t priority){CHECK(irq==7&&priority==0);}
static vd_motion_info_t info(void){vd_motion_info_t m;vision_drive_motion_get(&m);return m;}
static void init(void)
{
    vd_profile_t p;
    clock_ms=0;memset(pwm,0,sizeof(pwm));memset(writes,0,sizeof(writes));memset(dirs,0,sizeof(dirs));mask=0;
    vision_drive_init(0);app_mode_init(0,0,0);app_mode_sample(0,0,30);(void)app_mode_take_change();
    vision_drive_get_profile(&p);
    p.servo_center_us=1550;p.servo_min_us=1500;p.servo_max_us=1600;p.right_sign=-1;
    p.hardware_confirmed=1;p.motor_direction_confirmed=1;p.motor1_forward_level=0;p.motor2_forward_level=1;
    p.control.cruise_duty=0;p.control.duty_cap=0.03f;
    CHECK(vision_drive_configure(&p));CHECK(!vision_drive_ready()); /* Camera deliberately uncalibrated. */
}
static void advance(unsigned ms,int heartbeat)
{
    for(unsigned i=0;i<ms;i+=10) {
        clock_ms+=10;
        if(heartbeat && clock_ms%100U==0)(void)vision_drive_command('h',clock_ms,1);
        vision_drive_poll(clock_ms);vision_drive_safety_tick();
    }
}
static void field(vf_parser_t *parser,const char *text,char *reply)
{while(*text)(void)vf_feed(parser,(uint8_t)*text++,clock_ms,reply);}
#if VISION_MOTION_DIAG
static void diagnostic_init(void)
{
    vf_parser_t p;char r[VF_RESPONSE_CAPACITY];
    init();vf_init(&p);p.probe_permitted=1;
    field(&p,"@servo 1550 1500 1600 1\n",r);CHECK(strstr(r,"OK"));
    field(&p,"@motor_dir 1 1\n",r);CHECK(strstr(r,"OK"));
    field(&p,"@balance 1 0.667\n",r);CHECK(strstr(r,"OK"));
    field(&p,"@motor 0 0.15 0.1\n",r);CHECK(strstr(r,"OK"));
    CHECK(!pwm[0]&&!pwm[1]);
}
static void diagnostic_tests(void)
{
    vf_parser_t p;char r[VF_RESPONSE_CAPACITY];
    diagnostic_init();CHECK(!vision_drive_motion_start(VD_MOTION_LINE,151,2500,1550,0));
    vf_init(&p);p.probe_permitted=1;field(&p,"@motion line 150 2500\n",r);CHECK(strstr(r,"STARTED"));
    {unsigned servo_writes=writes[2];
     advance(1700,1);CHECK(pwm[0]==1500&&pwm[1]==1000&&pwm[2]==775);
     CHECK(writes[2]==servo_writes); /* Motor ramp must not reload the 50Hz servo. */
     unsigned right_writes=writes[0],left_writes=writes[1];
     advance(200,1);CHECK(writes[0]==right_writes&&writes[1]==left_writes&&writes[2]==servo_writes);}
    CHECK(dirs[0]==1&&dirs[1]==1);advance(800,1);
    CHECK(info().reason==VD_MOTION_DONE&&!pwm[0]&&!pwm[1]&&!pwm[2]);
    advance(2000,1);CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,clock_ms));
    advance(1700,1);CHECK(pwm[0]==1500&&pwm[1]==1000&&pwm[2]==775);vision_drive_stop();
    diagnostic_init();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,0));advance(2000,1);
    CHECK(pwm[0]==1500&&pwm[1]==1000);vf_init(&p);field(&p,"!",r);
    CHECK(!pwm[0]&&!pwm[1]&&info().reason==VD_MOTION_STOP);
    advance(3000,1);CHECK(!pwm[0]&&!pwm[1]);
    diagnostic_init();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,0));advance(2000,1);
    clock_ms+=499;vision_drive_safety_tick();CHECK(pwm[0]==1500&&pwm[1]==1000);
    ++clock_ms;vision_drive_safety_tick();CHECK(!pwm[0]&&!pwm[1]&&info().reason==VD_MOTION_HEARTBEAT);
    diagnostic_init();CHECK(vision_drive_motion_start(VD_MOTION_SHUTTLE,150,2500,1550,0));advance(2700,1);
    CHECK(info().phase==VD_MOTION_WAIT_REVERSE&&!pwm[0]&&!pwm[1]);
    CHECK(!vision_drive_motion_reverse(clock_ms,1));advance(2000,1);
    CHECK(vision_drive_motion_reverse(clock_ms,1));CHECK(dirs[0]==0&&dirs[1]==0&&!pwm[0]&&!pwm[1]);
    advance(1700,1);CHECK(pwm[0]==1500&&pwm[1]==1000);advance(1000,1);CHECK(!pwm[0]&&!pwm[1]);
    for(unsigned pulse=1500;pulse<=1600;pulse+=100){
        diagnostic_init();CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,150,2500,(uint16_t)pulse,0));
        advance(1700,1);CHECK(pwm[0]==1500&&pwm[1]==1000&&pwm[2]==pulse/2);
        app_mode_sample(1,1,clock_ms);vision_drive_safety_tick();CHECK(!pwm[0]&&!pwm[1]&&!pwm[2]);
    }
}
#endif
int main(void)
{
    vd_profile_t p;vf_parser_t parser;char reply[VF_RESPONSE_CAPACITY];
    init();vision_drive_get_profile(&p);p.motor_direction_confirmed=0;
    CHECK(vision_drive_configure(&p));CHECK(!vision_drive_motion_start(VD_MOTION_LINE,30,500,1550,0));
    p.motor_direction_confirmed=1;p.hardware_confirmed=0;CHECK(vision_drive_configure(&p));
    CHECK(!vision_drive_motion_start(VD_MOTION_LINE,30,500,1550,0));
    init();CHECK(!vision_drive_motion_start(VD_MOTION_LINE,31,500,1550,0));
    CHECK(!vision_drive_motion_start(VD_MOTION_LINE,30,3001,1550,0));
    CHECK(!vision_drive_motion_start(VD_MOTION_LINE,30,500,1525,0));
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,30,500,1550,0));
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,30,500,1499,0));
    CHECK(vision_drive_motion_start(VD_MOTION_LINE,30,500,1550,0));
    CHECK(!pwm[0]&&!pwm[1]&&pwm[2]>0);advance(500,1);
    CHECK(pwm[0]==300 && pwm[1]==300 && info().phase==VD_MOTION_FORWARD);
    CHECK(dirs[0]==0 && dirs[1]==1);CHECK(!vision_drive_command('a',clock_ms,1));
    CHECK(!vision_drive_can_store());CHECK(!vision_drive_motion_start(VD_MOTION_LINE,30,500,1550,clock_ms));
    advance(200,1);CHECK(info().phase==VD_MOTION_IDLE && info().reason==VD_MOTION_DONE);
    CHECK(!pwm[0]&&!pwm[1]&&!pwm[2]);CHECK(!vision_drive_motion_start(VD_MOTION_LINE,30,500,1550,clock_ms));
    advance(2000,1);CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,30,500,1525,clock_ms));
    advance(500,1);CHECK(pwm[0]>0 && pwm[2]==762 && info().servo_us==1525);
    vision_drive_stop();CHECK(!pwm[0]&&!pwm[1]&&!pwm[2]&&info().reason==VD_MOTION_STOP);
    (void)vision_drive_command('h',clock_ms,1);advance(300,1);CHECK(!pwm[0]&&!pwm[1]);
    init();CHECK(vision_drive_motion_start(VD_MOTION_SHUTTLE,30,300,1550,0));
    advance(500,1);CHECK(info().phase==VD_MOTION_WAIT_REVERSE && !pwm[0]&&!pwm[1]&&!pwm[2]);
    CHECK(!vision_drive_motion_reverse(clock_ms,1));
    advance(1990,1);CHECK(!vision_drive_motion_reverse(clock_ms,1));
    advance(10,1);CHECK(!vision_drive_motion_reverse(clock_ms,0));
    CHECK(vision_drive_motion_reverse(clock_ms,1));CHECK(dirs[0]==1 && dirs[1]==0 && !pwm[0]&&!pwm[1]);
    advance(500,1);CHECK(info().phase==VD_MOTION_IDLE&&info().reason==VD_MOTION_DONE&&!pwm[0]&&!pwm[1]);
    CHECK(!vision_drive_motion_reverse(clock_ms,1));
    init();CHECK(vision_drive_motion_start(VD_MOTION_LINE,30,3000,1550,0));advance(490,0);
    CHECK(pwm[0]>0);clock_ms=500;vision_drive_safety_tick();
    CHECK(!pwm[0]&&!pwm[1]&&!pwm[2]&&info().reason==VD_MOTION_HEARTBEAT);
    CHECK(!vision_drive_command('h',clock_ms,1));vision_drive_poll(clock_ms);CHECK(!pwm[0]);
    init();CHECK(vision_drive_motion_start(VD_MOTION_LINE,30,100,1550,0));advance(250,1);
    CHECK(pwm[0]>0 && pwm[1]>0);
    clock_ms=300;vision_drive_safety_tick(); /* Foreground frozen: PIT still enforces leg limit. */
    CHECK(info().reason==VD_MOTION_DONE&&!pwm[0]&&!pwm[1]&&!pwm[2]);
    init();CHECK(vision_drive_motion_start(VD_MOTION_LINE,30,500,1550,0));advance(400,1);
    app_mode_sample(1,1,clock_ms);vision_drive_safety_tick();
    CHECK(!pwm[0]&&!pwm[1]&&info().reason==VD_MOTION_MODE);
    app_mode_sample(0,0,clock_ms+50);vision_drive_poll(clock_ms+50);CHECK(!pwm[0]);
    init();vf_init(&parser);parser.probe_permitted=1;
    field(&parser,"@motion line 30 500 extra\n",reply);CHECK(strstr(reply,"REJECT") && !pwm[0]);
    field(&parser,"@motion circle 30 500 nan\n",reply);CHECK(strstr(reply,"REJECT") && !pwm[0]);
    field(&parser,"@motion line 30 500\n",reply);CHECK(strstr(reply,"STARTED"));advance(400,1);
    field(&parser,"!",reply);CHECK(strstr(reply,"STOP")&&!pwm[0]&&!pwm[1]);
    field(&parser,"@motion_status\n",reply);CHECK(strstr(reply,"reason=2"));
    init();CHECK(vision_drive_motion_start(VD_MOTION_SHUTTLE,30,300,1550,0));advance(8500,1);
    CHECK(info().reason==VD_MOTION_WAIT_TIMEOUT && !info().phase && !pwm[0]&&!pwm[1]);
    CHECK(dirs[0]==0 && dirs[1]==1 && !vision_drive_motion_reverse(clock_ms,1));
    init();vision_drive_get_profile(&p);p.motor1_scale=0.5f;CHECK(vision_drive_configure(&p));
    CHECK(vision_drive_motion_start(VD_MOTION_LINE,30,1000,1550,0));advance(500,1);
    CHECK(pwm[0]==150 && pwm[1]==300);
    vision_drive_trip(VC_FAULT_EXTERNAL);CHECK(!pwm[0]&&!pwm[1] && info().reason==VD_MOTION_FAULT);
    CHECK(!vision_drive_motion_start(VD_MOTION_LINE,30,500,1550,clock_ms));
    init();clock_ms=UINT32_MAX-250U;CHECK(vision_drive_motion_start(VD_MOTION_LINE,30,100,1550,clock_ms));
    advance(300,1);CHECK(info().reason==VD_MOTION_DONE&&!pwm[0]);
    CHECK(mask==0);
#if VISION_MOTION_DIAG
    diagnostic_tests();CHECK(mask==0);
#endif
    printf("MOTION PASS: %u checks; synthetic IO only, no physical distance/speed claim.\n",checks);return 0;
}
