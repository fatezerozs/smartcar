#include "vision_drive_internal.h"

int vision_drive_motion_active(void)
{
#if VISION_DRIVE_ENABLE
    return vd.motion.phase!=VD_MOTION_IDLE;
#else
    return 0;
#endif
}
void vision_drive_motion_get(vd_motion_info_t *out)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask=__get_PRIMASK();__disable_irq();*out=vd.motion;__set_PRIMASK(mask);
#else
    memset(out,0,sizeof(*out));
#endif
}
#if VISION_DRIVE_ENABLE
void vd_motion_cancel(unsigned reason,uint32_t now)
{
    if(!vd.motion.phase)return;
    vd.motion.phase=VD_MOTION_IDLE;vd.motion.actual_permille=0;
    vd.motion.reason=(uint8_t)reason;vd.motion.stopped_ms=now;
    vd.motion_have_stopped=1;vd_output_zero();
}
/* Integer-only: PIT1 may stop or enter a zero-output wait, never start motors. */
void vd_motion_tick(uint32_t now)
{
    uint8_t phase=vd.motion.phase;
    if(!phase)return;
    if(!app_mode_allows(APP_MODE_DRIVE)) {vd_motion_cancel(VD_MOTION_MODE,now);return;}
    if(vd.guard_faults || vd.control.output.state!=VC_DISARMED || vd.switch_inhibit ||
       (vd.switch_seen && !vd.switch_closed)) {vd_motion_cancel(VD_MOTION_FAULT,now);return;}
    if((uint32_t)(now-vd.motion.heartbeat_ms)>=VISION_CONTROL_COMMAND_TIMEOUT_MS) {
        vd_motion_cancel(VD_MOTION_HEARTBEAT,now);return;
    }
    if(phase==VD_MOTION_WAIT_REVERSE) {
        if((uint32_t)(now-vd.motion.phase_ms)>=VISION_MOTION_REVERSE_WAIT_MS)
            vd_motion_cancel(VD_MOTION_WAIT_TIMEOUT,now);
    } else if((phase==VD_MOTION_FORWARD || phase==VD_MOTION_REVERSE) &&
              (uint32_t)(now-vd.motion.phase_ms)>=vd.motion.duration_ms) {
        if(phase==VD_MOTION_FORWARD && vd.motion.kind==VD_MOTION_SHUTTLE) {
            vd.motion.actual_permille=0;vd_output_zero();
            vd.motion.phase_ms=now;vd.motion.phase=VD_MOTION_WAIT_REVERSE;
        } else vd_motion_cancel(VD_MOTION_DONE,now);
    }
}
void vd_motion_output(void)
{
    uint8_t phase=vd.motion.phase;
    uint32_t duty=vd.motion.actual_permille;
    int reverse=phase==VD_MOTION_REVERSE_SETTLE || phase==VD_MOTION_REVERSE;
    if(!phase || phase==VD_MOTION_WAIT_REVERSE){vd_output_zero();return;}
    /* A new direction is selected only from zero PWM after the coast gate. */
    gpio_set_level(MOTOR1_DIR,vd.profile.motor1_forward_level^(uint8_t)reverse);
    gpio_set_level(MOTOR2_DIR,vd.profile.motor2_forward_level^(uint8_t)reverse);
    /* pwm_set_duty clears LDOK before updating buffered registers, then
     * requests a full-cycle reload. Do not cancel/requeue identical values
     * at foreground-loop frequency. In particular the 50Hz servo must keep
     * its pending pulse while the motor ramp changes every ~10ms.
     * Caller masks IRQs; every stop invalidates this per-channel cache. */
    {
        uint32_t values[3]={duty*vd.motion_scale1/1000U*PWM_DUTY_MAX/1000U,
            duty*vd.motion_scale2/1000U*PWM_DUTY_MAX/1000U,
            (uint32_t)vd.motion.servo_us*SERVO_MOTOR_FREQ*PWM_DUTY_MAX/1000000U};
#if VISION_ENCODER_ENABLE
        uint16_t right,left;
        if(!vd_speed_output((uint16_t)(duty*vd.motion_scale1/1000U),(uint16_t)(duty*vd.motion_scale2/1000U),reverse,
            (uint16_t)(vd.profile.control.duty_cap*1000.0f+0.001f),&right,&left))return;
        values[0]=(uint32_t)right*PWM_DUTY_MAX/1000U;values[1]=(uint32_t)left*PWM_DUTY_MAX/1000U;
#endif
        const int channels[3]={MOTOR1_PWM,MOTOR2_PWM,SERVO_MOTOR_PWM};
        for(unsigned i=0;i<3;++i) {
            uint8_t bit=(uint8_t)(1U<<i);
            if(!(vd.motion_written_mask&bit) || vd.motion_written_pwm[i]!=values[i]) {
                pwm_set_duty(channels[i],values[i]);
                vd.motion_written_pwm[i]=values[i];vd.motion_written_mask|=bit;
            }
        }
    }
}
void vd_motion_poll(uint32_t now)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    vd_motion_tick(now);
    if(vd.motion.phase==VD_MOTION_SETTLE || vd.motion.phase==VD_MOTION_REVERSE_SETTLE) {
        if((uint32_t)(now-vd.motion.phase_ms)>=VISION_MOTION_SETTLE_MS) {
            vd.motion.phase=(uint8_t)(vd.motion.phase==VD_MOTION_SETTLE?VD_MOTION_FORWARD:VD_MOTION_REVERSE);
            vd.motion.phase_ms=now;
        }
    }
    if(vd.motion.phase==VD_MOTION_FORWARD || vd.motion.phase==VD_MOTION_REVERSE) {
        uint32_t ramp=(uint32_t)(now-vd.motion.phase_ms)*VISION_MOTION_RISE_PERMILLE_S/1000U;
        vd.motion.actual_permille=(uint16_t)(ramp>vd.motion.requested_permille?vd.motion.requested_permille:ramp);
    }
    if(vd.motion.phase)vd_motion_output();
    __DMB();__set_PRIMASK(mask);
}
#endif
int vision_drive_motion_start(unsigned kind,uint16_t permille,uint16_t duration,uint16_t pulse,uint32_t now)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask;unsigned delta;
    vision_drive_poll(now);
    delta=pulse>vd.profile.servo_center_us?pulse-vd.profile.servo_center_us:vd.profile.servo_center_us-pulse;
    if(!vd.initialized || !app_mode_allows(APP_MODE_DRIVE) || vd.motion.phase || vd.probe_active ||
       vd.guard_faults || vd.control.output.state!=VC_DISARMED || vd.switch_inhibit ||
       (vd.switch_seen && !vd.switch_closed) || !vision_drive_profile_valid(&vd.profile) ||
       !vd.profile.hardware_confirmed || !vd.profile.motor_direction_confirmed ||
       vd.profile.servo_min_us>=vd.profile.servo_center_us || vd.profile.servo_center_us>=vd.profile.servo_max_us ||
       vd.profile.motor1_scale<=0 || vd.profile.motor2_scale<=0 || vd.profile.control.cruise_duty!=0 ||
       vd.profile.control.duty_cap<=0 || kind<VD_MOTION_LINE || kind>VD_MOTION_CIRCLE ||
       !permille || permille>VISION_MOTION_MAX_PERMILLE ||
       permille>(uint32_t)(vd.profile.control.duty_cap*1000.0f+0.001f) ||
       duration<VISION_MOTION_MIN_MS || duration>(kind==VD_MOTION_LINE?VISION_MOTION_LINE_MAX_MS:VISION_MOTION_MAX_MS) ||
       pulse<vd.profile.servo_min_us || pulse>vd.profile.servo_max_us ||
       (kind!=VD_MOTION_CIRCLE && pulse!=vd.profile.servo_center_us) ||
       (kind==VD_MOTION_CIRCLE && (!delta || delta>VISION_MOTION_STEER_DELTA_US)) ||
       (!vd.motion_last_permille && permille>VISION_MOTION_INITIAL_PERMILLE) ||
       (vd.motion_last_permille && permille>vd.motion_last_permille+VISION_MOTOR_PROBE_STEP_PERMILLE) ||
       (vd.motion_have_stopped && (uint32_t)(now-vd.motion.stopped_ms)<VISION_MOTION_COAST_MS) ||
       (vd.have_motor_probe && (uint32_t)(now-vd.motor_probe_started_ms)<vd.motor_probe_hold_ms+VISION_MOTOR_PROBE_COAST_MS))return 0;
    mask=__get_PRIMASK();__disable_irq();
    if(!app_mode_allows(APP_MODE_DRIVE) || vd.guard_faults || vd.control.output.state!=VC_DISARMED || vd.motion.phase) {
        __set_PRIMASK(mask);return 0;
    }
    vd_output_zero();vd.guard_running=0;vd.switch_boot_pending=vd.switch_rearm=0;
    if(vd.auto_info.state==VD_AUTO_WAITING)vd.auto_info.state=VD_AUTO_CANCELLED;
    vd.motion_scale1=(uint16_t)(vd.profile.motor1_scale*1000.0f);
    vd.motion_scale2=(uint16_t)(vd.profile.motor2_scale*1000.0f);
    ++vd.motion.sequence;vd.motion.kind=(uint8_t)kind;vd.motion.phase=VD_MOTION_SETTLE;
    vd.motion.reason=VD_MOTION_NONE;vd.motion.actual_permille=0;vd.motion.requested_permille=permille;
    vd.motion.servo_us=pulse;vd.motion.duration_ms=duration;
    vd.motion.phase_ms=vd.motion.heartbeat_ms=timer_get(GPT_TIM_1);
    vd.motion_last_permille=permille;vd_motion_output();
    __DMB();__set_PRIMASK(mask);return 1;
#else
    (void)kind;(void)permille;(void)duration;(void)pulse;(void)now;return 0;
#endif
}
int vision_drive_motion_reverse(uint32_t now,int allowed)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask;
    vision_drive_poll(now);mask=__get_PRIMASK();__disable_irq();
    if(!allowed || !app_mode_allows(APP_MODE_DRIVE) || vd.motion.phase!=VD_MOTION_WAIT_REVERSE ||
       (uint32_t)(now-vd.motion.phase_ms)<VISION_MOTION_COAST_MS) {__set_PRIMASK(mask);return 0;}
    vd_output_zero();vd.motion.phase=VD_MOTION_REVERSE_SETTLE;vd.motion.phase_ms=timer_get(GPT_TIM_1);
    vd_motion_output();__DMB();__set_PRIMASK(mask);return 1;
#else
    (void)now;(void)allowed;return 0;
#endif
}
