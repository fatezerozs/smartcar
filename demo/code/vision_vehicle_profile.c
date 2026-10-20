#include "vision_drive_internal.h"

int vision_drive_profile_valid(const vd_profile_t *p)
{
    float m1, m2;
    if (!p || p->hardware_confirmed>1 || p->motor_direction_confirmed>1 ||
        p->motor1_forward_level>1 || p->motor2_forward_level>1 || p->autostart_enabled>1 ||
        p->startup_delay_ms<1000 || p->startup_delay_ms>10000 ||
        !vc_config_valid(&p->control) ||
        !mol_split(0,p->control.duty_cap,p->motor1_scale,p->motor2_scale,&m1,&m2)) return 0;
    if ((p->servo_min_us || p->servo_center_us || p->servo_max_us || p->right_sign) &&
        (p->servo_min_us<500 || p->servo_max_us>2500 ||
         p->servo_min_us>=p->servo_center_us || p->servo_center_us>=p->servo_max_us ||
         p->servo_max_us>=1000000U/SERVO_MOTOR_FREQ ||
         (p->right_sign!=1 && p->right_sign!=-1))) return 0;
    return p->control.frame_timeout_ms==VISION_CONTROL_FRAME_TIMEOUT_MS &&
           p->control.command_timeout_ms==VISION_CONTROL_COMMAND_TIMEOUT_MS;
}

int vision_drive_profile_ready(const vd_profile_t *p)
{
    return vision_drive_profile_valid(p) && p->hardware_confirmed==1 &&
        p->motor_direction_confirmed==1 && p->control.calibration_confirmed==1 &&
        p->servo_min_us>=500 && p->servo_min_us<p->servo_center_us &&
        p->servo_center_us<p->servo_max_us && (p->right_sign==1 || p->right_sign==-1) &&
        p->control.duty_cap>0 && p->control.cruise_duty>0 &&
        p->motor1_scale>0 && p->motor2_scale>0;
}

void vision_drive_get_profile(vd_profile_t *out)
{
    *out = vd.profile;
    out->control = vd.control.config;
}

int vision_drive_configure(const vd_profile_t *candidate)
{
#if VISION_DRIVE_ENABLE
    if (!candidate || !vd.initialized || vd.motion.phase || vd.guard_faults || vd.control.output.state != VC_DISARMED ||
        !vision_drive_profile_valid(candidate)) return 0;
    /* 允许未标定的全零脉宽配置，用于先做视觉；非零配置必须自洽。
     * 帧/心跳租约不开放修改，单次运行上限可在合法范围内停车设置。 */
    if ((candidate->servo_min_us || candidate->servo_center_us || candidate->servo_max_us || candidate->right_sign) &&
        (candidate->servo_min_us < 500 || candidate->servo_max_us > 2500 ||
         candidate->servo_min_us >= candidate->servo_center_us || candidate->servo_center_us >= candidate->servo_max_us ||
         (candidate->right_sign != 1 && candidate->right_sign != -1))) return 0;
    if (candidate->control.frame_timeout_ms != VISION_CONTROL_FRAME_TIMEOUT_MS ||
        candidate->control.command_timeout_ms != VISION_CONTROL_COMMAND_TIMEOUT_MS) return 0;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (vd.auto_info.state==VD_AUTO_WAITING) vd.auto_info.state=VD_AUTO_CANCELLED;
    vd.probe_active = 0; vd.guard_running = 0; vd_output_zero();
    if (!vc_reconfigure(&vd.control, &candidate->control)) { __set_PRIMASK(mask); return 0; }
    vd.profile = *candidate;
    vd.guard_max_run_ms = candidate->control.max_run_ms;
    vd.hardware_ready = (uint8_t)vision_drive_profile_ready(&vd.profile);
    __DMB(); __set_PRIMASK(mask);
    return 1;
#else
    (void)candidate; return 0;
#endif
}
