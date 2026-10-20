#include "main.h"
#include "app_mode.h"
#include "vision_drive.h"
#include "vision_speed.h"
#include "wheel_speed.h"
#include "vision_field.h"
#include "board_encoder.h"
#include <stdlib.h>
#undef printf
static uint32_t now,pwm[3],mask;
static uint16_t raw3,raw4;static int dir[2];static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"speed:%d %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t timer_get(int n){(void)n;return now;}
uint32_t __get_PRIMASK(void){return mask;}
void __disable_irq(void){mask=1;}
void __set_PRIMASK(uint32_t n){mask=n;}
void __DMB(void){}
void pwm_set_duty(int c,uint32_t d){CHECK(c>=0&&c<3&&d<=10000);pwm[c]=d;}
void gpio_set_level(int pin,int v){CHECK(pin==MOTOR1_DIR||pin==MOTOR2_DIR);dir[pin==MOTOR1_DIR?0:1]=v;}
void pit_ms_init(int c,uint32_t ms){CHECK(c==1&&ms==10);}
void interrupt_set_priority(int n,uint8_t p){CHECK(n==7&&p==0);}
void board_encoder_init(void){}
void board_encoder_read(uint16_t *l,uint16_t *r){*l=raw3;*r=raw4;}
void board_encoder_capture(board_encoder_diag_t *out){memset(out,0,sizeof(*out));out->ctrl3=out->ctrl4=5U<<13;for(unsigned i=0;i<4;++i)out->daisy[i]=1;}
static vd_speed_info_t info(void){vd_speed_info_t s;vision_drive_speed_get(&s);return s;}
static void tick(int left,int right,int heartbeat)
{
    raw3=(uint16_t)(raw3+left);raw4=(uint16_t)(raw4+right);now+=10;
    if(heartbeat&&(now%100U==0))(void)vision_drive_command('h',now,1);
    vision_drive_safety_tick();vision_drive_poll(now);
}
static void field(vf_parser_t *p,const char *s,char *r)
{while(*s)(void)vf_feed(p,(uint8_t)*s++,now,r);}
static void init(void)
{
    vd_profile_t p;now=0;raw3=raw4=0;mask=0;memset(pwm,0,sizeof(pwm));
    vision_drive_init(0);app_mode_init(0,0,0);app_mode_sample(0,0,30);(void)app_mode_take_change();
    vision_drive_get_profile(&p);p.servo_center_us=1550;p.servo_min_us=1500;p.servo_max_us=1600;
    p.right_sign=1;p.hardware_confirmed=1;p.motor_direction_confirmed=1;
    p.motor1_forward_level=p.motor2_forward_level=1;p.control.cruise_duty=0;p.control.duty_cap=0.15f;
    CHECK(vision_drive_configure(&p));tick(0,0,0);
}
static void enable(void)
{
    CHECK(vision_drive_encoder_signs(1,-1)); /* Encoder3 left +, encoder4 right -. */
    CHECK(vision_drive_speed_config(10,20.0f,1.0f));CHECK(vision_drive_speed_enable(1));
    CHECK(!pwm[0]&&!pwm[1]);
}
int main(void)
{
    ws_pi_t pi;vf_parser_t parser;char reply[VF_RESPONSE_CAPACITY];
    ws_pi_reset(&pi);
    for(unsigned i=0;i<1000;++i){uint16_t d=ws_pi_step(&pi,10,pi.output/15,5*256,256,150,1);CHECK(d<=150);}
    CHECK(pi.output>=135&&pi.output<=150);
    for(unsigned i=0;i<100;++i)CHECK(ws_pi_step(&pi,10,0,100*256,10*256,150,1)==150);
    CHECK(pi.integral_q8<=150*256);CHECK(ws_pi_step(&pi,0,0,256,256,150,1)==0&&pi.integral_q8==0);
    init();CHECK(!info().enabled&&!info().signs_confirmed);CHECK(!vision_drive_speed_enable(1));
    CHECK(!vision_drive_encoder_signs(0,1));CHECK(!vision_drive_speed_config(0,1,1));
    CHECK(!vision_drive_speed_config(1001,1,1));CHECK(!vision_drive_speed_config(10,-1,1));
    /* Reproduce real foreground/PIT phase ordering: after steering settle,
     * request becomes nonzero just AFTER the most recent encoder sample. */
    init();enable();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    for(unsigned i=0;i<20;++i)tick(0,0,1);
    now+=9;vision_drive_poll(now); /* Still zero ramp, consumes latest sample. */
    ++now;vision_drive_poll(now);  /* Ramp=1, no fresh sample yet. */
    ++now;vision_drive_safety_tick();
    CHECK(!info().faults&&vision_drive_motion_active());
    vision_drive_stop();
    tick(7,-9,0);CHECK(info().raw3==7&&info().raw4==-9&&info().total3==7&&info().total4==-9);
    /* Counter rollover preserves signed deltas without clearing hardware. */
    init();tick(-4,5,0);CHECK(info().raw3==-4&&info().raw4==5);
    enable();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    for(unsigned i=0;i<40;++i)tick(0,0,1);
    CHECK(pwm[0]>0&&pwm[1]>0); /* Zero feedback generates bounded drive. */
    tick(12,-15,1);CHECK(info().measured_right==15&&info().measured_left==12);
    CHECK(info().pwm_right<=150&&info().pwm_left<=150);
    vf_init(&parser);parser.probe_permitted=1;field(&parser,"!",reply);CHECK(!pwm[0]&&!pwm[1]);
    for(unsigned i=0;i<300;++i)tick(0,0,1);CHECK(!pwm[0]&&!pwm[1]);
    /* Wrong sign is latched rather than increasing power on bad feedback. */
    init();enable();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    for(unsigned i=0;i<50;++i)tick(-10,10,1);
    CHECK(info().faults==VC_FAULT_ENCODER&&!pwm[0]&&!pwm[1]);
    CHECK(info().fault_detail==VS_FAULT_REVERSE_RIGHT);
    CHECK(!vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    init();enable();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    for(unsigned i=0;i<220;++i)tick(0,0,1);
    CHECK(info().faults==VC_FAULT_ENCODER&&!pwm[0]&&!pwm[1]); /* No-count at high PWM. */
    CHECK(info().fault_detail==VS_FAULT_NOCOUNT_RIGHT||info().fault_detail==VS_FAULT_NOCOUNT_LEFT);
    /* Foreground stalled, safety ISR still cancels a stale speed output. */
    init();enable();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    for(unsigned i=0;i<40;++i)tick(1,-1,1);
    for(unsigned i=0;i<5;++i){now+=10;vision_drive_safety_tick();}
    CHECK(info().faults==VC_FAULT_ENCODER&&!pwm[0]&&!pwm[1]);
    CHECK(info().fault_detail==VS_FAULT_CONTROL_GAP&&info().fault_control_age_ms>=50);
    /* A missed sampling deadline remains a separate protection. */
    init();enable();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    for(unsigned i=0;i<40;++i)tick(1,-1,1);
    now+=21;vision_drive_safety_tick();
    CHECK(info().faults==VC_FAULT_ENCODER&&!pwm[0]&&!pwm[1]);
    CHECK(info().fault_detail==VS_FAULT_SAMPLE_GAP&&info().fault_interval_ms==21);
    /* Closed loop and inherited motion lease both expire without heartbeat. */
    init();enable();CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,2500,1550,now));
    for(unsigned i=0;i<60;++i)tick(1,-1,0);
    CHECK(!pwm[0]&&!pwm[1]);
    init();enable();vf_init(&parser);field(&parser,"@encoder_status\n",reply);
    CHECK(strstr(reply,"enc3=LEFT")&&strstr(reply,"enc4=RIGHT"));
    CHECK(strstr(reply,"detail=none")&&strstr(reply,"control_age_ms=0"));
    CHECK(strstr(reply,"mode=PULSE_DIR"));field(&parser,"@encoder_hw\n",reply);
    CHECK(strstr(reply,"CM3=5 CM4=5")&&strstr(reply,"daisy=1/1/1/1"));
    field(&parser,"@speed_cfg 10 nan 1\n",reply);CHECK(strstr(reply,"REJECT"));
    {vd_speed_info_t before=info();CHECK(!vision_drive_speed_config(20,0.0001f,1));
     CHECK(info().max_ticks==before.max_ticks&&info().kp_q8==before.kp_q8);}
    field(&parser,"@speed_mode 0\n",reply);CHECK(strstr(reply,"OK")&&!info().enabled);
    /* Same closed-loop configuration supports an explicitly confirmed reverse. */
    init();enable();CHECK(vision_drive_motion_start(VD_MOTION_SHUTTLE,150,2500,1550,now));
    for(unsigned i=0;i<270;++i)tick(1,-1,1);
    {vd_motion_info_t m;vision_drive_motion_get(&m);CHECK(m.phase==VD_MOTION_WAIT_REVERSE);}
    CHECK(!pwm[0]&&!pwm[1]);for(unsigned i=0;i<200;++i)tick(0,0,1);
    CHECK(vision_drive_motion_reverse(now,1));CHECK(dir[0]==0&&dir[1]==0);
    for(unsigned i=0;i<270;++i)tick(-1,1,1);
    {vd_motion_info_t m;vision_drive_motion_get(&m);CHECK(!m.phase&&m.reason==VD_MOTION_DONE);}
    CHECK(!info().faults&&!pwm[0]&&!pwm[1]);
    /* Unequal motor plant gains must produce different PWM for equal speed. */
    init();CHECK(vision_drive_encoder_signs(1,-1));CHECK(vision_drive_speed_config(8,5,1));
    CHECK(vision_drive_speed_enable(1));CHECK(vision_drive_motion_start(VD_MOTION_LINE,150,3000,1550,now));
    for(unsigned i=0;i<290;++i)tick((int)(pwm[1]/100U),-(int)(pwm[0]/150U),1);
    CHECK(info().target_right==8&&info().target_left==8);
    CHECK(info().measured_right>=7&&info().measured_right<=9&&info().measured_left>=7&&info().measured_left<=9);
    CHECK(info().pwm_right>info().pwm_left&&!info().faults);vision_drive_stop();
    /* No baseline bias: balanced speed mode requires equal target scales. */
    init();{vd_profile_t p;vision_drive_get_profile(&p);p.motor2_scale=0.667f;CHECK(vision_drive_configure(&p));}
    CHECK(vision_drive_encoder_signs(1,1));CHECK(vision_drive_speed_config(10,1,1));CHECK(!vision_drive_speed_enable(1));
    /* Extended line test retains the old limits for arcs and shuttle legs. */
    init();enable();CHECK(!vision_drive_motion_start(VD_MOTION_LINE,150,6001,1550,now));
    CHECK(!vision_drive_motion_start(VD_MOTION_SHUTTLE,150,3001,1550,now));
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,150,3001,1500,now));
    vf_init(&parser);parser.probe_permitted=1;
    field(&parser,"@motion line 150 6001\n",reply);CHECK(strstr(reply,"REJECT"));
    field(&parser,"@motion shuttle 150 3001\n",reply);CHECK(strstr(reply,"REJECT"));
    field(&parser,"@motion circle 150 3001 1500\n",reply);CHECK(strstr(reply,"REJECT"));
    field(&parser,"@motion line 150 6000\n",reply);CHECK(strstr(reply,"STARTED"));
    for(unsigned i=0;i<619;++i)tick(1,-1,1);
    CHECK(vision_drive_motion_active()&&!info().faults);
    tick(1,-1,1);
    {vd_motion_info_t m;vision_drive_motion_get(&m);CHECK(!m.phase&&m.reason==VD_MOTION_DONE);}
    CHECK(!pwm[0]&&!pwm[1]);
    /* Count totals and sample timing work across uint32_t uptime wrap. */
    init();now=UINT32_MAX-35U;vision_drive_init(now);tick(-7,9,0);
    tick(-7,9,0);tick(-7,9,0);tick(-7,9,0);CHECK(info().total3==-28&&info().total4==36);
    CHECK(!mask);printf("SPEED PASS %u checks; synthetic IO only\n",checks);return 0;
}
