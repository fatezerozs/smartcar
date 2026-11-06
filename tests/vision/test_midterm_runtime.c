/* Real scheduler, vision, speed PI and link modules; simulated peripherals only. */
#define VISION_TEST_MIDTERM_RUNTIME 1
void vision_test_runtime_tick(void);
#define timer_get legacy_timer_get
#define board_camera_poll legacy_board_camera_poll
#include "../support/fixtures/runtime_io.h"
#undef board_camera_poll
#undef timer_get
#include "app_transport.h"
#include "app_link.h"
#include "midterm_boot.h"
#include "board_encoder.h"
#include "vision_drive_internal.h"
#include <assert.h>
static unsigned experiment,observed_running,stop_injected;
static unsigned error_injected,last_repeat;
static uint32_t first_running_ms;
static uint16_t counter;
void board_buzzer_set(int on){(void)on;}
void board_encoder_read(uint16_t *l,uint16_t *r){
#if VISION_MIDTERM_C31_AUTONOMOUS
    *l=(uint16_t)(0U-counter);*r=counter; /* Compiled C31 left=-1, right=+1. */
#else
    *l=*r=counter;
#endif
}
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
        assert(elapsed>=MIDTERM_BOOT_DELAY_MS);
        if(!observed_running)first_running_ms=elapsed;
        observed_running=1;
    }
    if(!stop_injected && ((experiment==1&&elapsed>=MIDTERM_BOOT_DELAY_MS/2U)||
                         (experiment==3&&elapsed>=MIDTERM_BOOT_DELAY_MS+600U))) {
        stop_injected=1;
        app_link_rx_isr(1,'!');
    }
#if VISION_MIDTERM_C31_AUTONOMOUS
    if(experiment==4 && elapsed>=500U && error_injected==0) {
        app_link_error_isr(APP_LINK_USB);++error_injected;
    }
    if(experiment==4 && elapsed>=900U && error_injected==1) {
        /* The board IRQ rejects tagged corrupt data; no valid RX activity. */
        app_link_reject_rx_isr(APP_LINK_RADIO,0x2000U);
        app_link_uart_error_isr(APP_LINK_RADIO,0x20000U);++error_injected;
    }
    if(experiment==5 && elapsed>=500U && elapsed/100U!=last_repeat) {
        last_repeat=elapsed/100U;app_link_error_isr(APP_LINK_RADIO);++error_injected;
    }
    if(experiment>=6 && !error_injected &&
       elapsed>=(experiment==6?MIDTERM_BOOT_DELAY_MS+600U:500U)) {
        app_link_error_isr(APP_LINK_RADIO);++error_injected;
        if(experiment==7)app_link_rx_isr(APP_LINK_RADIO,'!');
        if(experiment==8)app_link_rx_isr(APP_LINK_RADIO,'\n');
    }
#endif
    return ticks_base+elapsed;
}
int main(void)
{
    for(experiment=0;experiment<
#if VISION_MIDTERM_C31_AUTONOMOUS
        9U
#else
        4U
#endif
        ;++experiment) {
        elapsed=0;counter=0;observed_running=0;stop_injected=0;error_injected=last_repeat=0;first_running_ms=0;
        finish_at=MIDTERM_BOOT_DELAY_MS+1200U;
        feed_until=experiment==2 ? MIDTERM_BOOT_DELAY_MS+600U : finish_at;
        moved=servo_moved=0;irq_mask=0;current_case=TEXT;case_name="autonomous runtime";
        memset(pwm,0,sizeof(pwm));mt9v03x_finish_flag=0;mt9v03x_image=NULL;
        app_mode_init(1,1,0);vs_init(NULL);app_transport_init();
        if(setjmp(stop_loop)==0)vision_debug_run();
        if(experiment==0 || experiment==4) {
            assert(observed_running&&moved&&vision_drive_is_local());
            if(experiment==4) {
                app_link_stats_t rx;app_link_stats(APP_LINK_RADIO,&rx);
                assert(error_injected==2 && first_running_ms>=900U+MIDTERM_BOOT_DELAY_MS);
                assert(rx.rx_bytes==0 && rx.rx_corrupt==1 && rx.last_bad_word==0x2000U);
            }
        } else if(experiment==5) {
            assert(error_injected>10 && !observed_running&&!moved&&midterm_boot_state()==MT_BOOT_WAIT);
        } else if(experiment==1 || experiment==7 || experiment==8) {
            assert(!observed_running&&!moved&&midterm_boot_state()==MT_BOOT_STOPPED);
        } else {
            assert(observed_running&&midterm_boot_state()==MT_BOOT_STOPPED&&!pwm[0]&&!pwm[1]);
        }
    }
    puts("AUTO RUNTIME PASS: camera/control/speed loop, busy radio, STOP, camera loss; C31 boot RX errors restart qualification, repeated errors inhibit, running errors/actual bytes stop.");
    return failures?1:0;
}
