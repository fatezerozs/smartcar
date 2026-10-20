/* Reuse the existing camera/output model; replace its clock/entry only.
 * The legacy scenario entry is not run by this executable. */
#define main legacy_scenarios_main
#define timer_get legacy_timer_get
#include "test_debug.c"
#undef timer_get
#undef main
#include "app_transport.h"
#include "app_link.h"
#include <assert.h>
static unsigned experiment, tx_count[2], stalled, observed_running;
static void inject_link(unsigned ch,const char *text)
{while(*text)app_link_rx_isr(ch,(uint8_t)*text++);}
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned ch,uint8_t c)
{(void)c;if(ch==1 && stalled)return 0;++tx_count[ch];return 1;}
uint32_t timer_get(int index)
{
    (void)index;++elapsed;
    if(elapsed>=finish_at)longjmp(stop_loop,1);
    if(elapsed%10==0) {
        fill_camera(0,0);mt9v03x_image=camera[0];mt9v03x_finish_flag=1;
    }
    if(vision_drive_output()->state==VC_RUNNING)observed_running=1;
    if(experiment==0) {
        if(elapsed==100)inject_link(1,"x");
        if(elapsed==200)inject_link(1,"a");
        if(elapsed>=250 && elapsed%50==0)inject_link(0,"h");
    } else if(experiment==1) {
        if(elapsed==200){inject_link(0,"a");inject_link(1,"!");}
    } else if(experiment==2) {
        if(elapsed==200)inject_link(0,"a");
        if(elapsed>=250 && elapsed%50==0)inject_link(0,"h");
        if(elapsed==350)inject_link(1,"x"); /* Must reject takeover while running. */
    } else {
        if(elapsed==50) {
            vd_profile_t p;vision_drive_get_profile(&p);p.control.cruise_duty=0;p.control.duty_cap=0.03f;
            assert(vision_drive_configure(&p));
        }
        if(elapsed==100)inject_link(1,"x");
        if(elapsed==200) {
            inject_link(1,"@motion line 30 2000\n");
            if(experiment==5)inject_link(0,"!");
        }
        if(elapsed>=300 && elapsed%50==0)inject_link(experiment==3?0U:1U,"h");
        if(experiment==4 && elapsed==650)inject_link(0,"!");
        if(experiment==6 && elapsed==650)inject_link(0,"x");
    }
    return elapsed;
}
int main(void)
{
    for(experiment=0;experiment<7;++experiment) {
        elapsed=0;finish_at=1000;observed_running=0;moved=servo_moved=0;
        barriers=0;irq_mask=0;current_case=TEXT;case_name="dual runtime";
        memset(pwm,0,sizeof(pwm));memset(tx_count,0,sizeof(tx_count));
        stalled=experiment==2 || experiment==6;mt9v03x_finish_flag=0;mt9v03x_image=NULL;
        app_mode_init(0,0,0);app_mode_sample(0,0,40);(void)app_mode_take_change();
        vs_init(NULL);app_transport_init();
        if(setjmp(stop_loop)==0)vision_debug_run();
        if(experiment==0) {
            assert(app_transport_owner()==1 && observed_running);
            assert(vision_drive_output()->state!=VC_RUNNING); /* USB h did not renew radio lease. */
        } else if(experiment==1)assert(!observed_running && !moved);
        else if(experiment==2) {
            assert(app_transport_owner()==0 && observed_running && tx_count[0]>0 && !tx_count[1]);
            assert(vision_drive_output()->state==VC_RUNNING); /* Busy radio did not block camera/control. */
        } else {
            vd_motion_info_t m;vision_drive_motion_get(&m);
            assert(app_transport_owner()==1);
            if(experiment==3)assert(moved && !m.phase && m.reason==VD_MOTION_HEARTBEAT && !pwm[0]&&!pwm[1]);
            if(experiment==4)assert(moved && !m.phase && m.reason==VD_MOTION_STOP && !pwm[0]&&!pwm[1]);
            if(experiment==5)assert(!moved && !m.phase); /* STOP forbids fragmented start across RX batches. */
            if(experiment==6)assert(moved && m.phase && pwm[0]>0 && tx_count[0]>0 && !tx_count[1]);
        }
    }
    puts("DUAL RUNTIME PASS: vision + motion loop, foreign-heartbeat timeout, cross-port STOP, fragmented start fencing, rejected moving takeover, RTS busy continuity.");
    return failures?1:0;
}
