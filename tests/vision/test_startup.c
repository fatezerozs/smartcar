#include "app_mode.h"
#include "vision_storage.h"
#include "vision_field.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t clock_ms,pwm[3],mask,flash_words[2][VS_RECORD_WORDS];
static unsigned checks,writes,sequence;
static int fail_read,fail_write,corrupt_write;
static vision_result_t lane;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL startup line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t timer_get(int channel){(void)channel;return clock_ms;}
uint32_t __get_PRIMASK(void){return mask;}
void __disable_irq(void){mask=1;}
void __set_PRIMASK(uint32_t m){mask=m;}
void __DMB(void){}
void pwm_set_duty(int c,uint32_t d){CHECK(c>=0&&c<3&&d<=10000);pwm[c]=d;}
void gpio_set_level(int p,int l){(void)p;CHECK(l==0||l==1);}
void pit_ms_init(int c,uint32_t t){CHECK(c==1&&t==10);}
void interrupt_set_priority(int i,uint8_t p){CHECK(i==7&&p==0);}
static int read_slot(unsigned slot,uint32_t *data,unsigned n)
{
    CHECK(slot<2&&n==VS_RECORD_WORDS);
    if(fail_read)return 0;
    memcpy(data,flash_words[slot],n*sizeof(*data));return 1;
}
static int replace_slot(unsigned slot,const uint32_t *data,unsigned n)
{
    CHECK(slot<2&&n==VS_RECORD_WORDS);++writes;
    memset(flash_words[slot],255,sizeof(flash_words[slot]));
    /* A failed program destroys only the inactive page, just like a power cut. */
    memcpy(flash_words[slot],data,(fail_write?5:n)*sizeof(*data));
    if(corrupt_write)flash_words[slot][10]^=1;
    return !fail_write;
}
static void storage_init(void){const vs_io_t io={read_slot,replace_slot};vs_init(&io);}
static void blank(void)
{
    memset(flash_words,255,sizeof(flash_words));fail_read=fail_write=corrupt_write=0;writes=0;storage_init();
}
static void ready_profile(vd_profile_t *p)
{
    vision_drive_get_profile(p);
    p->servo_center_us=1550;p->servo_min_us=1500;p->servo_max_us=1600;p->right_sign=-1;
    p->hardware_confirmed=p->motor_direction_confirmed=p->control.calibration_confirmed=1;
    p->control.cruise_duty=.1f;p->control.duty_cap=.2f;
    p->control.max_run_ms=5000;
    CHECK(vision_drive_profile_ready(p));
}
static uint32_t record_crc(const uint32_t *w)
{
    uint32_t c=0xFFFFFFFFU;unsigned i,b,k;
    for(i=0;i<VS_RECORD_WORDS-1;++i)for(b=0;b<4;++b){
        c^=(w[i]>>(b*8))&255U;
        for(k=0;k<8;++k)c=(c>>1)^((c&1)?0xEDB88320U:0U);
    }
    return c^0xFFFFFFFFU;
}
static void crc_update(uint32_t *w){w[VS_RECORD_WORDS-1]=record_crc(w);}

static void storage_tests(void)
{
    vd_profile_t p,q;vs_info_t info;unsigned i,b;uint32_t pristine[VS_RECORD_WORDS];
    clock_ms=0;vision_drive_init(0); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();blank();vision_drive_get_profile(&p);q=p;
    CHECK(!vs_load(&q));vs_get_info(&info);CHECK(info.error==VS_EMPTY);
    CHECK(q.servo_center_us==0);CHECK(!vs_save(&p)&&writes==0);
    ready_profile(&p);CHECK(vs_save(&p));CHECK(writes==1);
    CHECK(vs_load(&q));CHECK(q.right_sign==-1&&q.startup_delay_ms==2000&&q.autostart_enabled);
    CHECK(q.control.kp==p.control.kp&&q.motor1_scale==p.motor1_scale);
    vs_get_info(&info);CHECK(info.generation==1&&info.slot==0);
    CHECK(vs_save(&p)&&writes==1); /* No wear for identical saves. */
    p.control.kp=.7f;CHECK(vs_save(&p));vs_get_info(&info);CHECK(info.generation==2&&info.slot==1);
    flash_words[1][8]^=1;CHECK(vs_load(&q));CHECK(q.control.kp!=.7f);
    fail_write=1;p.control.kp=.8f;CHECK(!vs_save(&p));
    fail_write=0;storage_init();CHECK(vs_load(&q));CHECK(q.control.kp!=.8f);
    corrupt_write=1;CHECK(!vs_save(&p));vs_get_info(&info);CHECK(info.error==VS_VERIFY_ERROR);
    corrupt_write=0;CHECK(vs_load(&q));CHECK(q.control.kp!=.8f);
    CHECK(vs_save(&p));CHECK(vs_load(&q));CHECK(q.control.kp==.8f);
    memcpy(pristine,flash_words[1],sizeof(pristine));
    memset(flash_words[0],255,sizeof(flash_words[0]));
    /* Every single-bit error in the complete record must be rejected. */
    for(i=0;i<VS_RECORD_WORDS;++i)for(b=0;b<32;++b){
        memcpy(flash_words[1],pristine,sizeof(pristine));flash_words[1][i]^=1U<<b;
        CHECK(!vs_load(&q));
    }
    memcpy(flash_words[1],pristine,sizeof(pristine));
    flash_words[1][27]=256;crc_update(flash_words[1]);CHECK(!vs_load(&q)); /* No narrow truncation. */
    memcpy(flash_words[1],pristine,sizeof(pristine));
    flash_words[1][4]=0x7FC00000U;crc_update(flash_words[1]);CHECK(!vs_load(&q));
    memcpy(flash_words[1],pristine,sizeof(pristine));
    flash_words[1][2]=0xFFFFFFFFU;crc_update(flash_words[1]);CHECK(vs_load(&q));
    p.control.kp=.9f;CHECK(vs_save(&p));vs_get_info(&info);CHECK(info.generation==0&&info.slot==0);
    storage_init();CHECK(vs_load(&q)&&q.control.kp==.9f);
    fail_read=1;CHECK(!vs_load(&q));i=writes;CHECK(!vs_save(&p)&&writes==i);fail_read=0;
    p.motor1_scale=NAN;CHECK(!vs_save(&p));
}

static void initialize_lane(void)
{
    int y;memset(&lane,0,sizeof(lane));lane.valid=1;lane.status=VISION_TRACK;lane.confidence=90;
    for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){lane.left[y]=64;lane.right[y]=123;lane.center[y]=94;lane.row_flags[y]=3;}
}
static void tick(int closed,int frames)
{
    clock_ms+=10;vision_drive_switch_sample(closed);vision_drive_safety_tick();
    if(frames && !(clock_ms%20))vision_drive_observe(&lane,++sequence,clock_ms,clock_ms);
    vision_drive_poll(clock_ms);
}
static void ticks(unsigned count,int closed,int frames){unsigned i;for(i=0;i<count;++i)tick(closed,frames);}
static void boot(uint32_t time,int closed)
{
    vd_profile_t p;clock_ms=time;sequence=0;vision_drive_init(time); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();storage_init();
    CHECK(vs_load(&p));CHECK(vision_drive_configure(&p));
    vision_drive_set_local_allowed(1);
    vision_drive_schedule_boot(time,time);vision_drive_switch_sample(closed);
}
static void auto_tests(void)
{
    vd_profile_t p;vd_auto_info_t state;uint32_t start;
    clock_ms=0;vision_drive_init(0); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();blank();ready_profile(&p);CHECK(vs_save(&p));initialize_lane();
    /* Master input is fail-closed even before the very first PIT sample. */
    CHECK(vision_drive_configure(&p));
    CHECK(!vision_drive_servo_probe(1500,300,clock_ms));
    CHECK(!vision_drive_command('a',clock_ms,1));
    boot(0,1);ticks(199,1,1);CHECK(vision_drive_output()->state==VC_DISARMED&&pwm[0]==0);
    ticks(3,1,1);CHECK(vision_drive_is_local()&&pwm[0]>0);
    ticks(70,1,1);CHECK(vision_drive_output()->state==VC_RUNNING); /* No heartbeat. */
    /* Raw opening cuts outputs even if foreground never gets called again. */
    vision_drive_switch_sample(0);CHECK(pwm[0]==0&&pwm[1]==0&&pwm[2]==0);
    vision_drive_poll(clock_ms);CHECK(vision_drive_output()->state==VC_DISARMED);
    ticks(20,1,1);CHECK(vision_drive_output()->state==VC_DISARMED); /* Contact bounce cannot restart. */
    ticks(35,0,1);start=clock_ms;tick(1,1);ticks(198,1,1);CHECK(!vision_drive_is_local());
    ticks(3,1,1);CHECK(vision_drive_is_local() && clock_ms-start>=2000);
    vision_drive_stop();ticks(250,1,1);CHECK(!vision_drive_is_local());
    boot(0,1);lane.valid=0;ticks(220,1,1);vision_drive_get_auto(&state);CHECK(state.state==VD_AUTO_BLOCKED);
    lane.valid=1;ticks(250,1,1);CHECK(vision_drive_output()->state==VC_DISARMED); /* No delayed launch. */
    boot(0,0);ticks(250,0,1);CHECK(!vision_drive_is_local());
    CHECK(!vision_drive_command('a',clock_ms,1));
    ticks(202,1,1);CHECK(vision_drive_is_local());
    ticks(11,1,0);CHECK(vision_drive_output()->state==VC_FAULT);
    CHECK(vision_drive_output()->faults&VC_FAULT_FRAME);
    CHECK(!(vision_drive_output()->faults&VC_FAULT_COMMAND));
    CHECK(pwm[0]==0&&pwm[2]==0);
    ticks(400,1,1);CHECK(vision_drive_output()->state==VC_FAULT);
    boot(0,1);ticks(199,1,1);vision_drive_stop();ticks(120,1,1);CHECK(!vision_drive_is_local());
    boot(0,1);ticks(100,1,1);vision_drive_switch_sample(0);
    vision_drive_poll(clock_ms);ticks(120,1,1);CHECK(!vision_drive_is_local());
    boot(0,1);ticks(202,1,1);ticks(500,1,1);
    CHECK(vision_drive_output()->faults&VC_FAULT_RUNTIME);
    boot(0,0);vision_drive_set_local_allowed(0);ticks(35,0,1);ticks(220,1,1);
    CHECK(!vision_drive_is_local());
    vision_drive_set_local_allowed(1);ticks(250,1,1);CHECK(!vision_drive_is_local());
    boot(UINT32_MAX-999,1);ticks(202,1,1);CHECK(vision_drive_is_local());
    /* Saving an explicitly disabled boot policy retains remote heartbeat. */
    vision_drive_stop();ready_profile(&p);p.autostart_enabled=0;CHECK(vs_save(&p));
    boot(0,1);ticks(250,1,1);CHECK(!vision_drive_is_local());
    CHECK(vision_drive_command('a',clock_ms,1));tick(1,1);ticks(60,1,1);
    CHECK(vision_drive_output()->faults&VC_FAULT_COMMAND);
    /* A valid RAM profile alone is not a restored boot permission. */
    clock_ms=0;vision_drive_init(0); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();ready_profile(&p);CHECK(vision_drive_configure(&p));
    ticks(250,1,1);CHECK(vision_drive_output()->state==VC_DISARMED);
}
static void protocol_tests(void)
{
    vf_parser_t parser;char response[VF_RESPONSE_CAPACITY];const char *s;vs_info_t info;
    clock_ms=0;vision_drive_init(0); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();blank();vf_init(&parser);
    s="@save\r\n";while(*s)vf_feed(&parser,(uint8_t)*s++,clock_ms,response);
    CHECK(strstr(response,"REJECT")!=NULL&&writes==0);
    s="@startup 1 2000\n";while(*s)vf_feed(&parser,(uint8_t)*s++,clock_ms,response);
    CHECK(strstr(response,"OK")!=NULL);
    s="@startup 2 2000\n";while(*s)vf_feed(&parser,(uint8_t)*s++,clock_ms,response);
    CHECK(strstr(response,"REJECT")!=NULL);
    s="@startup 1 2000 a\n";while(*s)vf_feed(&parser,(uint8_t)*s++,clock_ms,response);
    CHECK(strstr(response,"REJECT")!=NULL);
    s="@status\n";while(*s)vf_feed(&parser,(uint8_t)*s++,clock_ms,response);
    CHECK(strstr(response,"[STARTUP]")!=NULL && strstr(response,"[STORE]")!=NULL);
    vs_get_info(&info);CHECK(!info.valid);
}
int main(void)
{
    storage_tests();auto_tests();protocol_tests();CHECK(mask==0);
    printf("STARTUP + STORAGE PASS: %u checks; no physical IO\n",checks);return 0;
}
