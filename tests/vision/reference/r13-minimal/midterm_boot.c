#include "vision_drive_internal.h"
#include "midterm_boot.h"
#include "board_io.h"

#if VISION_MIDTERM_AUTONOMOUS
/* One attempt per MCU boot. No UART, allocation, delay, or GPIO key logic. */
static mt_boot_state_t state;
static uint32_t boot_time, signal_time;
static uint8_t speed_prepared;
static void terminal(mt_boot_state_t next, uint32_t now)
{
    state=next;signal_time=now;vision_drive_stop();board_buzzer_set(0);
}
void midterm_boot_init(uint32_t boot_ms,uint32_t now)
{
    boot_time=boot_ms;signal_time=now;speed_prepared=0;state=MT_BOOT_WAIT;
    board_buzzer_set(0);
    if((uint32_t)(now-boot_ms)>=MIDTERM_BOOT_DELAY_MS)
        terminal(MT_BOOT_BLOCKED,now);
}
void midterm_boot_cancel(uint32_t now)
{
    if(state==MT_BOOT_WAIT || state==MT_BOOT_RUNNING)terminal(MT_BOOT_STOPPED,now);
}
mt_boot_state_t midterm_boot_state(void){return state;}
void midterm_boot_poll(uint32_t now)
{
    vd_speed_info_t s;
    uint32_t elapsed=(uint32_t)(now-boot_time);
    if(state==MT_BOOT_RUNNING &&
       (vd.control.output.state!=VC_RUNNING || vd.guard_faults))
        terminal(MT_BOOT_STOPPED,now);
    if(state==MT_BOOT_WAIT) {
        vision_drive_speed_get(&s);
        if(!speed_prepared && MIDTERM_SPEED_CONFIRMED && s.samples && !s.faults &&
           (uint32_t)(now-s.sample_ms)<=20U) {
            speed_prepared=(uint8_t)(vision_drive_encoder_signs(MIDTERM_ENCODER_LEFT_SIGN,MIDTERM_ENCODER_RIGHT_SIGN) &&
                vision_drive_speed_config(MIDTERM_SPEED_MAX_TICKS,MIDTERM_SPEED_KP,MIDTERM_SPEED_KI) &&
                vision_drive_speed_enable(1));
        }
        if(elapsed>=MIDTERM_BOOT_DELAY_MS) {
            uint32_t mask=__get_PRIMASK();int accepted;
            __disable_irq();vision_drive_speed_get(&s);
            accepted=MIDTERM_VISUAL_REVIEWED && speed_prepared && s.enabled && !s.faults &&
                (uint32_t)(now-s.sample_ms)<=20U && app_mode_allows(APP_MODE_DRIVE) &&
                vd.local_permitted && !vd.guard_faults && !vd.probe_active && !vd.motion.phase &&
                vd.hardware_ready && vc_arm_local(&vd.control,now,vd.hardware_ready);
            if(accepted){state=MT_BOOT_RUNNING;signal_time=now;vd_output_apply();}
            else terminal(MT_BOOT_BLOCKED,now);
            __set_PRIMASK(mask);
        }
    }
    if(state==MT_BOOT_WAIT)board_buzzer_set(elapsed%1000U<100U);
    else if(state==MT_BOOT_RUNNING)board_buzzer_set((uint32_t)(now-signal_time)<200U);
    else {
        uint32_t t=(uint32_t)(now-signal_time);
        board_buzzer_set(t<600U && t%200U<100U);
    }
}
#endif
