/* Real scheduler, vision, speed PI and link modules; simulated peripherals only. */
#define main legacy_scenarios_main
#define timer_get legacy_timer_get
#include "test_debug.c"
#undef timer_get
#undef main
#include "app_transport.h"
#include "app_link.h"
#include "midterm_boot.h"
#include "board_encoder.h"
#include "vision_drive_internal.h"
#include <assert.h>
static unsigned experiment,observed_running,in_tick;
static uint16_t counter;
void board_buzzer_set(int on){(void)on;}
void board_encoder_read(uint16_t *l,uint16_t *r){*l=*r=counter;}
void board_encoder_capture(board_encoder_diag_t *out){memset(out,0,sizeof(*out));}
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned ch,uint8_t c){(void)ch;(void)c;return 0;} /* Radio always busy. */
uint32_t timer_get(int index)
{
    (void)index;uint32_t at=++elapsed;
    if(elapsed>=finish_at)longjmp(stop_loop,1);
    if(at%20U==0 && (experiment!=2 || elapsed<3200)) {
        fill_camera(0,0);mt9v03x_image=camera[0];mt9v03x_finish_flag=1;
    }
    if(elapsed>100 && vision_drive_is_local()) {
        assert(elapsed>=3000);observed_running=1;
    }
    if((experiment==1&&at==1000)||(experiment==3&&at==3600))
        app_link_rx_isr(1,'!');
    if(!in_tick && !irq_mask && at>100 && at%10U==0) {
        in_tick=1;counter=(uint16_t)(counter+2);vision_drive_safety_tick();in_tick=0;
    }
    return elapsed;
}
int main(void)
{
    for(experiment=0;experiment<4;++experiment) {
        elapsed=0;counter=0;observed_running=0;in_tick=0;finish_at=4200;
        moved=servo_moved=0;irq_mask=0;current_case=TEXT;case_name="autonomous runtime";
        memset(pwm,0,sizeof(pwm));mt9v03x_finish_flag=0;mt9v03x_image=NULL;
        app_mode_init(1,1,0);vs_init(NULL);app_transport_init();
        if(setjmp(stop_loop)==0)vision_debug_run();
        if(experiment==0) {
            assert(observed_running&&moved&&vision_drive_is_local());
        } else if(experiment==1) {
            assert(!observed_running&&!moved&&midterm_boot_state()==MT_BOOT_STOPPED);
        } else {
            assert(observed_running&&midterm_boot_state()==MT_BOOT_STOPPED&&!pwm[0]&&!pwm[1]);
        }
    }
    puts("AUTO RUNTIME PASS: actual camera/controller/speed loop, no UART heartbeat, busy radio, pre/post-start STOP, camera loss.");
    return failures?1:0;
}
