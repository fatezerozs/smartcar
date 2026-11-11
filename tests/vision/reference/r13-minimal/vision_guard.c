#include "vision_drive_internal.h"

void vision_drive_safety_tick(void)
{
#if VISION_DRIVE_ENABLE
#if VISION_ENCODER_ENABLE
    vd_speed_sample_tick(timer_get(GPT_TIM_1));
#endif
    vd_motion_tick(timer_get(GPT_TIM_1));
    if (vd.initialized && ((vd.guard_running && !app_mode_allows(APP_MODE_DRIVE)) ||
        (vd.probe_active==1 && !app_mode_allows(APP_MODE_SERVO)) ||
        (vd.probe_active>=2 && !app_mode_allows(APP_MODE_MOTOR)))) {
        vd.mode_stop_pending=1;vd.guard_running=0;vd.probe_active=0;vd_output_zero();
    }
    if (vd.initialized && vd.probe_active &&
        (uint32_t)(timer_get(GPT_TIM_1) - vd.probe_started_ms) >= vd.probe_hold_ms) {
        vd.probe_active = 0;
        vd_output_zero(); /* Integer-only, also works if foreground/UART blocks. */
    }
    if (vd.initialized && vd.guard_running) {
        uint32_t now = timer_get(GPT_TIM_1), faults = 0;
        if ((uint32_t)(now - vd.guard_frame_ms) >= VISION_CONTROL_FRAME_TIMEOUT_MS) faults |= VC_FAULT_FRAME;
        if (!vd.guard_local && (uint32_t)(now - vd.guard_command_ms) >= VISION_CONTROL_COMMAND_TIMEOUT_MS) faults |= VC_FAULT_COMMAND;
        if (vd.guard_max_run_ms && (uint32_t)(now - vd.guard_armed_ms) >= vd.guard_max_run_ms) faults |= VC_FAULT_RUNTIME;
        if (faults) {
            vd.guard_faults |= faults;
            vd.guard_running = 0;
            vd_output_zero();
        }
    }
#endif
}

void vision_drive_switch_sample(int closed)
{
#if VISION_DRIVE_ENABLE
    uint32_t now;int changed;
    if(!vd.initialized)return;
    now=timer_get(GPT_TIM_1);
    changed=!vd.switch_seen || vd.switch_closed!=(uint8_t)(closed!=0);
    if(changed) {
        vd.switch_changed_ms=now;vd.switch_closed=(uint8_t)(closed!=0);vd.switch_seen=1;
    }
    if(!closed) {
        if(changed || !vd.switch_inhibit || vd.guard_running || vd.probe_active) {
            vd.switch_stop_pending=1;vd.guard_running=0;vd.probe_active=0;vd_output_zero();
        }
        vd.switch_inhibit=1;
    }
#else
    (void)closed;
#endif
}
