/* Real scheduler, vision, speed PI and link modules; simulated peripherals only. */
#define VISION_TEST_MIDTERM_RUNTIME 1
void vision_test_runtime_tick(void);
#define main legacy_scenarios_main
#define timer_get legacy_timer_get
#define board_camera_poll legacy_board_camera_poll
#include "test_debug.c"
#undef board_camera_poll
#undef timer_get
#undef main
#include "app_transport.h"
#include "app_link.h"
#include "midterm_boot.h"
#include "board_encoder.h"
#include "vision_drive_internal.h"
#include <assert.h>
static unsigned experiment,observed_running,stop_injected;
static uint16_t counter;
void board_buzzer_set(int on){(void)on;}
void board_encoder_read(uint16_t *l,uint16_t *r){*l=*r=counter;}
void board_encoder_capture(board_encoder_diag_t *out){memset(out,0,sizeof(*out));}
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned ch,uint8_t c){(void)ch;(void)c;return 0;} /* Radio always busy. */
int board_camera_poll(int requested,uint32_t at)
{
    /* Dual UART reads real link queues, so advance the fixture once per loop here. */
    uint8_t unused;
    (void)mock_receive(&unused,0);
    (void)at;return requested;
}
void vision_test_runtime_tick(void)
{
    /* One encoder sample and safety interrupt per mock_receive 10 ms step. */
    counter=(uint16_t)(counter+2);
    vision_drive_safety_tick();
}
uint32_t timer_get(int index)
{
    (void)index;
    if(elapsed>100 && vision_drive_is_local()) {
        assert(elapsed>=3000);observed_running=1;
    }
    if(!stop_injected && ((experiment==1&&elapsed>=1000)||(experiment==3&&elapsed>=3600))) {
        stop_injected=1;
        app_link_rx_isr(1,'!');
    }
    return ticks_base+elapsed;
}
int main(void)
{
    for(experiment=0;experiment<4;++experiment) {
        elapsed=0;counter=0;observed_running=0;stop_injected=0;finish_at=4200;
        feed_until=experiment==2 ? 3200U : finish_at;
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
