#include "heartbeat_diag.h"
#include "stop_trace.h"
#include "vision_drive_internal.h"

#include "heartbeat_diag.h"
#include "app_link.h"
#include <stdio.h>
volatile hd_state_t hd_live;
static hd_state_t hd_frozen;
static uint32_t hd_radio_rx_at_arm,hd_radio_rx_at_stop;
void hd_arm(void) {
    app_link_stats_t s;app_link_stats(APP_LINK_RADIO,&s);
    memset((void *)&hd_live,0,sizeof(hd_live));memset(&hd_frozen,0,sizeof(hd_frozen));
    hd_radio_rx_at_arm=s.rx_bytes;hd_radio_rx_at_stop=0;
}
void hd_stop(unsigned reason,uint32_t now,uint32_t lease) {
    hd_state_t live=hd_live;app_link_stats_t s;app_link_stats(APP_LINK_RADIO,&s);
    hd_capture(&hd_frozen,&live,now,lease,reason);hd_radio_rx_at_stop=s.rx_bytes;
}
void hd_format(char *out,unsigned capacity) {
    uint32_t mask=__get_PRIMASK();hd_state_t z;uint32_t rx0,rx1;
    __disable_irq();z=hd_frozen.valid?hd_frozen:hd_live;
    rx0=hd_radio_rx_at_arm;rx1=hd_radio_rx_at_stop;__set_PRIMASK(mask);
    snprintf(out,capacity,"[HB_DIAG] frozen=%lu reason=%lu stop=%lu lease=%lu age=%lu rx_h=%lu/%lu/%lu parsed_h=%lu/%lu/%lu accepted_h=%lu/%lu/%lu radio_rx=%lu/%lu\r\n",
      (unsigned long)z.valid,(unsigned long)z.reason,(unsigned long)z.stop_ms,
      (unsigned long)z.lease_ms,(unsigned long)(z.valid?z.stop_ms-z.lease_ms:0),
      (unsigned long)z.rx.count,(unsigned long)z.rx.last_ms,(unsigned long)z.rx.max_gap_ms,
      (unsigned long)z.parsed.count,(unsigned long)z.parsed.last_ms,(unsigned long)z.parsed.max_gap_ms,
      (unsigned long)z.accepted.count,(unsigned long)z.accepted.last_ms,(unsigned long)z.accepted.max_gap_ms,
      (unsigned long)rx0,(unsigned long)rx1);
}


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
    hd_stop(reason,now,vd.motion.heartbeat_ms);
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
    if(vd.motion.phase)vd_output_apply();
    __DMB();__set_PRIMASK(mask);
}
#endif
int vision_drive_motion_start(unsigned kind,uint16_t permille,uint16_t duration,uint16_t pulse,uint32_t now)
{
#if VISION_DRIVE_ENABLE && !VISION_MIDTERM_AUTONOMOUS
    uint32_t mask;unsigned delta;int accepted;
    vd_speed_info_t admission;
    vision_drive_speed_get(&admission);
    if(!st_can_arm() || kind!=VD_MOTION_LINE || !admission.signs_confirmed ||
       !admission.samples || now-admission.sample_ms>20U ||
       admission.raw3 < -1 || admission.raw3 > 1 || admission.raw4 < -1 || admission.raw4 > 1)return 0;
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
    hd_arm();
    ++vd.motion.sequence;vd.motion.kind=(uint8_t)kind;vd.motion.phase=VD_MOTION_SETTLE;
    vd.motion.reason=VD_MOTION_NONE;vd.motion.actual_permille=0;vd.motion.requested_permille=permille;
    vd.motion.servo_us=pulse;vd.motion.duration_ms=duration;
    vd.motion.phase_ms=vd.motion.heartbeat_ms=timer_get(GPT_TIM_1);
    vd.motion_last_permille=permille;
    {vd_speed_info_t s;uint32_t cfg[ST_CONFIG_FIELDS];vision_drive_speed_get(&s);
     cfg[0]=permille;cfg[1]=duration;cfg[2]=(uint32_t)(vd.profile.control.duty_cap*1000.0f+.001f);
     cfg[3]=s.max_ticks;cfg[4]=s.kp_q8;cfg[5]=s.ki_q8;cfg[6]=(uint32_t)(int32_t)s.sign3;cfg[7]=(uint32_t)(int32_t)s.sign4;
     cfg[8]=s.enabled;cfg[9]=pulse;st_set_config(cfg);st_arm(now,s.samples,s.total3,s.total4);}
    accepted=vd_output_bench_start();
    if(!accepted)vd_motion_cancel(VD_MOTION_FAULT,timer_get(GPT_TIM_1));
    __DMB();__set_PRIMASK(mask);return accepted;
#else
    (void)kind;(void)permille;(void)duration;(void)pulse;(void)now;return 0;
#endif
}
int vision_drive_motion_reverse(uint32_t now,int allowed)
{
    (void)now;(void)allowed;return 0;
}
