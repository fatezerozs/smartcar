#include "vision_drive_internal.h"

void vision_drive_schedule_boot(uint32_t boot_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE
    /* Not re-schedulable within a boot, including after STOP/clear/configure. */
    if (vd.auto_info.restored) return;
#if VISION_MOTOR_THRESHOLD_DIAG
    /* Ignore saved automatic-start settings in the separate bench build. */
    vd.profile.autostart_enabled=0;
#endif
    vd.auto_info.restored=1; vd.auto_info.boot_ms=boot_ms;
    vd.auto_info.delay_ms=vd.profile.startup_delay_ms;
    vd.switch_boot_pending=vd.profile.autostart_enabled;
    if(vd.profile.autostart_enabled)vd.auto_info.state=VD_AUTO_WAITING;
    if (vd.profile.autostart_enabled && (uint32_t)(now-boot_ms)>=vd.profile.startup_delay_ms) {
        vd.switch_boot_pending=0;vd.auto_info.state=VD_AUTO_BLOCKED;
    }
#else
    (void)boot_ms; (void)now;
#endif
}

void vision_drive_get_auto(vd_auto_info_t *out)
{
    *out=vd.auto_info;
#if VISION_DRIVE_ENABLE
    out->switch_seen=vd.switch_seen;out->switch_closed=vd.switch_closed;
#endif
}

void vision_drive_set_local_allowed(int allowed)
{
#if VISION_DRIVE_ENABLE
    vd.local_permitted=(uint8_t)(allowed!=0);
    if(!allowed && (vd.auto_info.state==VD_AUTO_WAITING || vd.control.local_run))vision_drive_stop();
#else
    (void)allowed;
#endif
}

int vision_drive_is_local(void) { return vd.control.output.state==VC_RUNNING && vd.control.local_run; }

#if VISION_DRIVE_ENABLE
void vd_startup_poll_switch(uint32_t now)
{
    if (vd.switch_stop_pending) {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        vd.switch_stop_pending=0;__set_PRIMASK(mask);
        vision_drive_stop();
    }
    if (vd.switch_seen && !vd.switch_closed &&
        (uint32_t)(now-vd.switch_changed_ms)>=VISION_START_SWITCH_REARM_MS) {
        vd.switch_rearm=1;
    }
    if (vd.switch_seen && vd.switch_closed &&
        (uint32_t)(now-vd.switch_changed_ms)>=VISION_START_SWITCH_DEBOUNCE_MS) {
        vd.switch_inhibit=0;
#if !VISION_MOTOR_THRESHOLD_DIAG
        if (vd.auto_info.restored && vd.profile.autostart_enabled && (vd.switch_boot_pending || vd.switch_rearm)) {
            uint32_t base=vd.switch_boot_pending?vd.auto_info.boot_ms:vd.switch_changed_ms;
            vd.switch_boot_pending=0;vd.switch_rearm=0;
            vd.auto_info.boot_ms=base;vd.auto_info.delay_ms=vd.profile.startup_delay_ms;
            vd.auto_info.state=vd.local_permitted && vd.hardware_ready && vd.control.output.state==VC_DISARMED &&
                (uint32_t)(now-base)<vd.auto_info.delay_ms ? VD_AUTO_WAITING:VD_AUTO_BLOCKED;
        }
#endif
    }
}
void vd_startup_poll_deadline(uint32_t now)
{
    if (vd.auto_info.state==VD_AUTO_WAITING &&
        (uint32_t)(now-vd.auto_info.boot_ms)>=vd.auto_info.delay_ms) {
        /* A blocked deadline is terminal for this boot: never creep into
         * motion later when a person moves away or a new scene becomes valid. */
        int accepted=0;
#if !VISION_MOTOR_THRESHOLD_DIAG
        accepted=app_mode_allows(APP_MODE_DRIVE) && vd.local_permitted && vd.switch_seen && vd.switch_closed && !vd.switch_inhibit &&
            !vd.guard_faults && !vd.probe_active && vd.hardware_ready &&
            vc_arm_local(&vd.control,now,vd.hardware_ready);
#endif
        vd.auto_info.state=accepted?VD_AUTO_STARTED:VD_AUTO_BLOCKED;
        vd_output_apply(); /* First command is zero duty; next fresh frame ramps. */
    }
}
#endif

#if VISION_ROUTE_SWITCH
/* C26 level authority: valid lane starts/restarts; OFF always revokes drive.
 * Explicit UART STOP/pause still inhibits until a deliberate OFF cycle. */
static uint8_t route_operator_stop,route_released;
void vd_route_switch_init(void){route_operator_stop=route_released=0;}
void vd_route_switch_cancel(void){route_operator_stop=1;}
void vd_route_switch_mode_stop(void)
{
    uint8_t stopped=route_operator_stop;
    vision_drive_stop();route_operator_stop=stopped;
}
void vd_route_switch_poll(uint32_t now)
{
    vd_speed_info_t s;
    if(!app_mode_allows(APP_MODE_DRIVE)) {
        /* STOP/query while already OFF must not inhibit the next ON. */
        if(app_mode_allows(APP_MODE_OBSERVE))route_operator_stop=0;
        if(app_mode_allows(APP_MODE_OBSERVE) && !route_released) {
            vision_drive_speed_get(&s);
            if(s.samples && (uint32_t)(now-s.sample_ms)<=20U) {
                uint32_t mask=__get_PRIMASK();__disable_irq();
                vd.guard_running=0;vd.probe_active=0;vd_output_zero();vd.guard_faults=0;
                vc_clear_fault(&vd.control);__set_PRIMASK(mask);
                (void)vision_drive_speed_enable(1);
                route_released=1;route_operator_stop=0;
            }
        }
        return;
    }
    route_released=0;
    if(app_mode_change_pending() || route_operator_stop)return;
    /* Missing vision is a zero-output wait, not a permanent start lock.
     * Do not auto-clear numeric/configuration/encoder/offset/runtime faults. */
    if(vd.control.output.state==VC_FAULT && vd.control.output.faults &&
       !(vd.control.output.faults&~(VC_FAULT_VISION|VC_FAULT_FRAME))) {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        vd.guard_running=0;vd_output_zero();
        vd.guard_faults&=~(uint32_t)(VC_FAULT_VISION|VC_FAULT_FRAME);
        vc_clear_fault(&vd.control);__set_PRIMASK(mask);
        return; /* Reacquire five new valid frames before starting. */
    }
    if(vd.control.output.state!=VC_DISARMED)return;
    vision_drive_speed_get(&s);
    if(!vd.local_permitted || !s.enabled || s.faults || (uint32_t)(now-s.sample_ms)>20U ||
       vd.guard_faults || vd.probe_active || vd.motion.phase || !vd.hardware_ready)return;
    {
        uint32_t mask=__get_PRIMASK();int accepted;__disable_irq();
        accepted=app_mode_allows(APP_MODE_DRIVE) && !app_mode_change_pending() &&
            vc_arm_local(&vd.control,now,vd.hardware_ready);
        if(accepted)vd_output_apply();
        __set_PRIMASK(mask);
    }
}
#endif
