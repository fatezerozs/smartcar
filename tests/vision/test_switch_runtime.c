#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
/* Actual scheduler/transport/camera path, no UART start or heartbeat. */
#define main legacy_scenarios_main
#define timer_get legacy_timer_get
#include "test_debug.c"
#undef timer_get
#undef main
#include "app_transport.h"
#include "app_link.h"
#include "board_encoder.h"
#include "vision_drive_internal.h"
static unsigned experiment,observed_running,in_tick;
static uint32_t last_tick;
static char trace_wire[131072];
static unsigned trace_wire_size;
#undef assert
#define assert(x) do{if(!(x)){fprintf(stderr,"runtime:%d %s\n",__LINE__,#x);exit(1);}}while(0)
static uint16_t left_counter,right_counter;
void board_encoder_read(uint16_t *l,uint16_t *r){*l=left_counter;*r=right_counter;}
void board_encoder_capture(board_encoder_diag_t *out){memset(out,0,sizeof(*out));}
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned ch,uint8_t c){
    if(experiment==6&&ch==APP_LINK_RADIO&&trace_wire_size<sizeof(trace_wire)-1U){
        trace_wire[trace_wire_size++]=(char)c;trace_wire[trace_wire_size]=0;return 1;}
    return 0;
}
uint32_t timer_get(int index)
{
    (void)index;uint32_t at=++elapsed;
    if(at>=finish_at)longjmp(stop_loop,1);
    if(at%20U==0&&(experiment!=4||at<2800)&&(experiment!=5||at<2800||at>=3200)){
        fill_camera(0,0);mt9v03x_image=camera[0];mt9v03x_finish_flag=1;
    }
    if(at>100&&vision_drive_is_local())observed_running=1;
    if(experiment==3&&at==2800)app_link_rx_isr(1,'!');
    if(!in_tick&&!irq_mask&&at>100&&(uint32_t)(at-last_tick)>=10U){
        int a=(experiment==0||at>=1000)?0:1;
        if((experiment==2||experiment==6)&&at>=2800)a=1;
        in_tick=1;last_tick=at;app_mode_sample(a,(at/100U)%2U,at);
        left_counter=(uint16_t)(left_counter-15);right_counter=(uint16_t)(right_counter+15);
        vision_drive_safety_tick();in_tick=0;
    }
    return elapsed;
}
int main(void)
{
    for(experiment=0;experiment<7;++experiment){
        elapsed=0;last_tick=0;left_counter=right_counter=0;observed_running=in_tick=0;finish_at=4200;
        trace_wire_size=0;trace_wire[0]=0;if(experiment==6)finish_at=6200;
        moved=servo_moved=0;irq_mask=0;current_case=TEXT;case_name="C26 runtime";
        memset(pwm,0,sizeof(pwm));mt9v03x_finish_flag=0;mt9v03x_image=NULL;
        app_mode_init(experiment==0?0:1,1,0);vs_init(NULL);app_transport_init();
        if(setjmp(stop_loop)==0)vision_debug_run();
        if(experiment==0||experiment==1||experiment==5){assert(observed_running&&moved&&vision_drive_is_local());}
        else{assert(observed_running&&!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);}
        if(experiment==6){
            assert(strstr(trace_wire,"[TRACE_BEGIN] run=1 count=")!=NULL);
            assert(strstr(trace_wire,"[TRACE] run=1 i=0 ")!=NULL);
            assert(strstr(trace_wire,"[TRACE_END] run=1 count=")!=NULL);
        }
    }
    puts("C26 LEVEL RUNTIME PASS: ON start, OFF/STOP/camera-loss stop, stream recovery resumes, C27 ignored, no UART heartbeat; trace automatically exported while stopped.");
    return failures?1:0;
}
