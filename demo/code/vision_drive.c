#include "vision_drive_internal.h"

/* Lifecycle and command facade; IO, guard, startup, probing and profile
 * validation live in their own implementation units. */
vd_context_t vd;

void vision_drive_init(uint32_t now)
{
    vc_config_t cfg;
    (void)now;
    vc_default_config(&cfg);
    vc_init(&vd.control, &cfg);
    memset(&vd.profile, 0, sizeof(vd.profile));
    vd.profile.control = cfg;
    vd.profile.servo_center_us = VISION_SERVO_CENTER_US;
    vd.profile.servo_min_us = VISION_SERVO_MIN_US;
    vd.profile.servo_max_us = VISION_SERVO_MAX_US;
    vd.profile.right_sign = VISION_SERVO_RIGHT_SIGN;
    vd.profile.hardware_confirmed = VISION_DRIVE_PROFILE_CONFIRMED;
    vd.profile.motor_direction_confirmed = VISION_MOTOR_DIRECTION_CONFIRMED;
    vd.profile.motor1_forward_level = MOTOR1_FORWARD_DIR_LEVEL;
    vd.profile.motor2_forward_level = MOTOR2_FORWARD_DIR_LEVEL;
    vd.profile.motor1_scale = VISION_MOTOR1_SCALE;
    vd.profile.motor2_scale = VISION_MOTOR2_SCALE;
    vd.profile.autostart_enabled = VISION_AUTO_START_ENABLE;
    vd.profile.startup_delay_ms = VISION_AUTO_START_DELAY_MS;
    memset(&vd.probe_info,0,sizeof(vd.probe_info));
    memset(&vd.auto_info,0,sizeof(vd.auto_info));
#if VISION_DRIVE_ENABLE
    memset((void *)&vd.motion,0,sizeof(vd.motion));
    vd.motion_last_permille=0;vd.motion_have_stopped=0;
    vd.guard_running = 0; vd.guard_faults = 0; vd.guard_local = 0; vd.mode_stop_pending=0;
    vd.switch_seen=0;vd.switch_closed=0;vd.switch_inhibit=VISION_START_SWITCH_ENABLE;vd.switch_stop_pending=0;
    vd.switch_rearm=0;vd.switch_boot_pending=0;vd.switch_changed_ms=now;
    vd.local_permitted=0;
    vd.probe_active = 0; vd.probe_last_us = 0;
    memset(vd.motor_last_permille,0,sizeof(vd.motor_last_permille)); vd.have_motor_probe = 0;
    vd.guard_max_run_ms = cfg.max_run_ms;
    vd.hardware_ready = (uint8_t)vision_drive_profile_ready(&vd.profile);
    vd.initialized = 1;
    vd_output_zero();
    /* board_init 已将 PWM 初始化为零，PIT0 已无编码器/PID 输出路径。
     * 保留 PIT1 为独立 10ms 安全租约；不得挪作阻塞 UART/视觉计算。 */
    pit_ms_init(PIT_CH1, VISION_CONTROL_SAFETY_TICK_MS);
    interrupt_set_priority(PIT_IRQn, 0);
#endif
}

void vision_drive_poll(uint32_t now)
{
#if VISION_DRIVE_ENABLE
    if (vd.mode_stop_pending) {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        vd.mode_stop_pending=0;__set_PRIMASK(mask);
        vision_drive_stop();
    }
    vd_startup_poll_switch(now);
    vd_probe_poll(now);
    vd_motion_poll(now);
    if (vd.guard_faults) vc_trip(&vd.control, vd.guard_faults);
    vc_poll(&vd.control, now);
    vd_startup_poll_deadline(now);
    if (vd.control.output.state != VC_RUNNING && vd.guard_running) vd_output_apply();
#else
    (void)now;
#endif
}

void vision_drive_observe(const vision_result_t *r, uint32_t sequence, uint32_t frame_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE
    vision_drive_poll(now);
    vc_observe(&vd.control, r, sequence, frame_ms, now);
    vd_output_apply();
#else
    (void)r; (void)sequence; (void)frame_ms; (void)now;
#endif
}

void vision_drive_stop(void)
{
    if (vd.auto_info.state==VD_AUTO_WAITING) vd.auto_info.state=VD_AUTO_CANCELLED;
    vc_stop(&vd.control);
#if VISION_DRIVE_ENABLE
    vd.switch_boot_pending=0;vd.switch_rearm=0;
    {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        vd_motion_cancel(VD_MOTION_STOP,timer_get(GPT_TIM_1));__set_PRIMASK(mask);
    }
    vd.probe_active = 0;
    vd_output_apply();
#endif
}

void vision_drive_trip(uint32_t reason)
{
    if (vd.auto_info.state==VD_AUTO_WAITING) vd.auto_info.state=VD_AUTO_CANCELLED;
    vc_trip(&vd.control, reason | VC_FAULT_EXTERNAL);
#if VISION_DRIVE_ENABLE
    {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        vd_motion_cancel(VD_MOTION_FAULT,timer_get(GPT_TIM_1));__set_PRIMASK(mask);
    }
    vd.probe_active = 0;
    vd_output_apply();
#endif
}

int vision_drive_command(char command, uint32_t now, int text_mode)
{
#if VISION_DRIVE_ENABLE
    vision_drive_poll(now);
    if (command == 's') { vision_drive_stop(); return 1; }
    if (command == 'h') {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        if(vd.motion.phase) {vd.motion.heartbeat_ms=now;__set_PRIMASK(mask);return 1;}
        __set_PRIMASK(mask);
        vc_heartbeat(&vd.control, now);
        /* Heartbeat extends command lease, never the camera/update lease. */
        if (vd.control.output.state == VC_RUNNING) vd.guard_command_ms = vd.control.last_command_ms;
        return vd.control.output.state == VC_RUNNING;
    }
    if (command == 'c' && vd.control.output.state != VC_RUNNING && !vd.motion.phase) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        vd.probe_active = 0; vd.guard_running = 0; vd_output_zero(); vd.guard_faults = 0;
        __set_PRIMASK(mask);
        vc_clear_fault(&vd.control);
        return 1;
    }
    if (command == 'a' && app_mode_allows(APP_MODE_DRIVE) && text_mode && !vd.guard_faults && !vd.probe_active && !vd.motion.phase &&
        (!vd.motion_have_stopped || (uint32_t)(now-vd.motion.stopped_ms)>=VISION_MOTION_COAST_MS) &&
        !vd.switch_inhibit &&
        !(vd.switch_seen && (!vd.switch_closed || vd.switch_inhibit)) &&
        (!vd.have_motor_probe || (uint32_t)(now-vd.motor_probe_started_ms) >= vd.motor_probe_hold_ms+VISION_MOTOR_PROBE_COAST_MS))
        return vc_arm(&vd.control, now, vd.hardware_ready);
#else
    (void)command; (void)now; (void)text_mode;
#endif
    return 0;
}

const vc_output_t *vision_drive_output(void) { return &vd.control.output; }
int vision_drive_ready(void)
{
#if VISION_DRIVE_ENABLE
    return vd.initialized && vd.hardware_ready && vd.control.configured;
#else
    return 0;
#endif
}

int vision_drive_can_store(void)
{
#if VISION_DRIVE_ENABLE
    return vd.initialized && !vd.guard_faults && !vd.probe_active && !vd.motion.phase && vd.control.output.state==VC_DISARMED;
#else
    return 0;
#endif
}
