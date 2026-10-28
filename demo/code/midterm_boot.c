#include "vision_drive_internal.h"
#include "midterm_boot.h"
#include "board_io.h"

#if VISION_MIDTERM_AUTONOMOUS
/* One attempt per MCU boot. No UART, allocation, delay, or GPIO key logic. */
static mt_boot_state_t state;
static uint32_t boot_time, signal_time;
static uint8_t speed_prepared;
#if VISION_MIDTERM_C31_AUTONOMOUS
static uint8_t track_active,track_seen;
static uint32_t track_start,track_start_frame,track_frame,track_sequence,track_now;
static void track_reset(void)
{
    track_active=0;
    track_start=track_start_frame=track_frame=track_now=0;
    /* Keep the sequence: a poll of the same frame cannot begin a new wait. */
}
/* Only distinct, fresh, stable control observations advance this clock.
 * Both capture-time and foreground-time gaps revoke continuity. */
static int track_ready(uint32_t now)
{
    vc_controller_t *c=&vd.control;
    uint32_t timeout=c->config.frame_timeout_ms;
    if(!c->have_frame || !c->output.observation_valid || !c->output.far_valid ||
       c->stable_frames<c->config.arm_frames ||
       (uint32_t)(now-c->last_frame_ms)>=timeout) {
        track_reset();return 0;
    }
    if(track_seen && c->last_sequence==track_sequence)return 0;
    if(!track_active || (uint32_t)(c->last_frame_ms-track_frame)>=timeout ||
       (uint32_t)(now-track_now)>=timeout) {
        track_reset();
        track_start=now;track_start_frame=c->last_frame_ms;track_active=1;
    }
    track_seen=1;track_sequence=c->last_sequence;track_frame=c->last_frame_ms;track_now=now;
    /* Processing latency must not substitute for time covered by new frames. */
    return (uint32_t)(now-track_start)>=MIDTERM_BOOT_DELAY_MS &&
        (uint32_t)(c->last_frame_ms-track_start_frame)>=MIDTERM_BOOT_DELAY_MS;
}
#endif
static void terminal(mt_boot_state_t next, uint32_t now)
{
#if VISION_MIDTERM_C31_AUTONOMOUS
    track_reset();
#endif
    state=next;signal_time=now;vision_drive_stop();board_buzzer_set(0);
}
void midterm_boot_init(uint32_t boot_ms,uint32_t now)
{
#if VISION_MIDTERM_C31_AUTONOMOUS
    /* Called exactly once by app_runtime on MCU boot; host tests simulate resets. */
#endif
    boot_time=boot_ms;signal_time=now;speed_prepared=0;state=MT_BOOT_WAIT;
    board_buzzer_set(0);
#if VISION_MIDTERM_C31_AUTONOMOUS
    track_reset();track_seen=0;track_sequence=0;
    (void)boot_ms;
#else
    if((uint32_t)(now-boot_ms)>=MIDTERM_BOOT_DELAY_MS)
        terminal(MT_BOOT_BLOCKED,now);
#endif
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
    if(vd.autonomous_stopped && (state==MT_BOOT_WAIT || state==MT_BOOT_RUNNING)) {
        terminal(MT_BOOT_STOPPED,now);return;
    }
#if VISION_MIDTERM_C31_AUTONOMOUS
    if(state==MT_BOOT_WAIT || state==MT_BOOT_RUNNING) {
        vision_drive_speed_get(&s);
        if(vd.guard_faults || vd.control.output.faults || s.faults ||
           vd.control.output.state==VC_FAULT) {
            terminal(state==MT_BOOT_RUNNING?MT_BOOT_STOPPED:MT_BOOT_BLOCKED,now);return;
        }
    }
#endif
    if(state==MT_BOOT_RUNNING &&
       (vd.control.output.state!=VC_RUNNING || vd.guard_faults))
        terminal(MT_BOOT_STOPPED,now);
    if(state==MT_BOOT_WAIT) {
        vision_drive_speed_get(&s);
#if VISION_MIDTERM_C31_AUTONOMOUS
        /* route_speed_pending owns C31 signs, 132 ticks and PI setup. */
        speed_prepared=(uint8_t)(MIDTERM_SPEED_CONFIRMED && s.samples && s.enabled && !s.faults);
#else
        if(!speed_prepared && MIDTERM_SPEED_CONFIRMED && s.samples && !s.faults &&
           (uint32_t)(now-s.sample_ms)<=VD_SPEED_STARTUP_SAMPLE_MAX_AGE_MS) {
            speed_prepared=(uint8_t)(vision_drive_encoder_signs(MIDTERM_ENCODER_LEFT_SIGN,MIDTERM_ENCODER_RIGHT_SIGN) &&
                vision_drive_speed_config(MIDTERM_SPEED_MAX_TICKS,MIDTERM_SPEED_KP,MIDTERM_SPEED_KI) &&
                vision_drive_speed_enable(1));
        }
#endif
#if VISION_MIDTERM_C31_AUTONOMOUS
        if(track_ready(now)) {
#else
        if(elapsed>=MIDTERM_BOOT_DELAY_MS) {
#endif
            uint32_t mask=__get_PRIMASK();int accepted;
            __disable_irq();vision_drive_speed_get(&s);
            accepted=MIDTERM_VISUAL_REVIEWED && speed_prepared && s.enabled && !s.faults &&
                (uint32_t)(now-s.sample_ms)<=VD_SPEED_STARTUP_SAMPLE_MAX_AGE_MS && app_mode_allows(APP_MODE_DRIVE) &&
                vd.local_permitted && !vd.guard_faults && !vd.probe_active && !vd.motion.phase &&
                vd.hardware_ready &&
#if VISION_MIDTERM_C31_AUTONOMOUS
                !vd.control.output.faults && vd.control.output.far_valid &&
                vd.control.output.state==VC_DISARMED &&
#endif
                vd_output_arm(now,1);
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
