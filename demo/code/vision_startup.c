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
