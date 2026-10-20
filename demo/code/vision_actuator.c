#include "vision_drive_internal.h"

#if VISION_DRIVE_ENABLE
void vd_output_zero(void)
{
    /* Stops always write hardware. Invalidate the motion write cache so a
     * later explicit start cannot mistake cleared outputs for held PWM. */
    vd.motion_written_mask=0;
#if VISION_ENCODER_ENABLE
    vd_speed_stop();
#endif
    pwm_set_duty(MOTOR1_PWM, 0);
    pwm_set_duty(MOTOR2_PWM, 0);
    pwm_set_duty(SERVO_MOTOR_PWM, 0);
    /* Never reverse a moving motor as a substitute for braking. Direction
     * stays fixed while armed. PWM=0 is not a guaranteed stopping distance. */
}

static void write_changed(unsigned index,int channel,uint32_t duty)
{
    uint8_t bit=(uint8_t)(1U<<index);
    if(!(vd.motion_written_mask&bit)||vd.motion_written_pwm[index]!=duty) {
        pwm_set_duty(channel,duty);vd.motion_written_pwm[index]=duty;vd.motion_written_mask|=bit;
    }
}

void vd_output_apply(void)
{
    if (!vd.initialized) return;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if(vd.motion.phase) {
        vd_motion_tick(timer_get(GPT_TIM_1));
        if(vd.motion.phase)vd_motion_output();else vd_output_zero();
        __DMB();__set_PRIMASK(mask);return;
    }
    if (((vd.probe_active==1 && app_mode_allows(APP_MODE_SERVO)) ||
         (vd.probe_active>=2 && app_mode_allows(APP_MODE_MOTOR))) && !vd.guard_faults && !vd.switch_inhibit &&
        !(vd.switch_seen && !vd.switch_closed) && vd.control.output.state == VC_DISARMED) {
        /* Separate bounded bench path: at most one actuator. Camera frames
         * cannot arm driving or overwrite this explicitly requested pulse. */
        vd.guard_running = 0;
        pwm_set_duty(MOTOR1_PWM, vd.probe_active == 2 ? vd.probe_duty : 0);
        pwm_set_duty(MOTOR2_PWM, vd.probe_active == 3 ? vd.probe_duty : 0);
        pwm_set_duty(SERVO_MOTOR_PWM, vd.probe_active == 1 ? vd.probe_duty : 0);
    } else if (vd.guard_faults || vd.switch_inhibit || (vd.switch_seen && !vd.switch_closed) ||
               !app_mode_allows(APP_MODE_DRIVE) || !vd.hardware_ready || vd.control.output.state != VC_RUNNING) {
        vd.probe_active = 0;
        vd.guard_running = 0;
        vd_output_zero();
    } else {
        float steer = vd.control.output.steering * vd.profile.right_sign;
        float pulse = vd.profile.servo_center_us + steer * (steer >= 0 ?
            vd.profile.servo_max_us - vd.profile.servo_center_us :
            vd.profile.servo_center_us - vd.profile.servo_min_us);
        float motor1, motor2;
        if (!mol_split(vd.control.output.duty, vd.profile.control.duty_cap,
                       vd.profile.motor1_scale, vd.profile.motor2_scale, &motor1, &motor2)) {
            vd.guard_faults |= VC_FAULT_NUMERIC;
            vc_trip(&vd.control, VC_FAULT_NUMERIC);
            vd.guard_running = 0;
            vd_output_zero();
            __DMB(); __set_PRIMASK(mask);
            return;
        }
        uint32_t servo_duty = (uint32_t)(pulse * SERVO_MOTOR_FREQ * PWM_DUTY_MAX / 1000000.0f);
        uint16_t right=(uint16_t)(motor1*1000.0f),left=(uint16_t)(motor2*1000.0f);
#if VISION_ENCODER_ENABLE
        if(!vd_speed_output(right,left,0,(uint16_t)(vd.profile.control.duty_cap*1000.0f+0.001f),&right,&left)) {
            __DMB();__set_PRIMASK(mask);return;
        }
#endif
        /* Controller outputs are bounded; vd.profile validates pulse range first.
         * This short critical section prevents foreground from re-enabling
         * PWM after a safety ISR has latched a stop. No UART/float in ISR. */
        gpio_set_level(MOTOR1_DIR, vd.profile.motor1_forward_level);
        gpio_set_level(MOTOR2_DIR, vd.profile.motor2_forward_level);
        write_changed(2,SERVO_MOTOR_PWM,servo_duty);
#if VISION_ENCODER_ENABLE
        write_changed(0,MOTOR1_PWM,(uint32_t)right*PWM_DUTY_MAX/1000U);
        write_changed(1,MOTOR2_PWM,(uint32_t)left*PWM_DUTY_MAX/1000U);
#else
        (void)right;(void)left;
        write_changed(0,MOTOR1_PWM,(uint32_t)(motor1 * PWM_DUTY_MAX));
        write_changed(1,MOTOR2_PWM,(uint32_t)(motor2 * PWM_DUTY_MAX));
#endif
        vd.guard_frame_ms = vd.control.last_frame_ms;
        vd.guard_command_ms = vd.control.last_command_ms;
        vd.guard_armed_ms = vd.control.armed_ms;
        vd.guard_local = vd.control.local_run;
        vd.guard_running = 1;
    }
    __DMB();
    __set_PRIMASK(mask);
}
#endif
