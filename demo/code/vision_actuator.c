#include "vision_drive_internal.h"
#include "vision_steering.h"

#if VISION_DRIVE_ENABLE
void vd_output_zero(void)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    vd.output_authorized=0;vd.guard_running=0;
    /* Stops always write hardware. Invalidate the motion write cache so a
     * later explicit start cannot mistake cleared outputs for held PWM. */
    vd.motion_written_mask=0;
#if VISION_ENCODER_ENABLE
    vd_speed_stop();
#endif
    pwm_set_duty(MOTOR1_PWM, 0);
    pwm_set_duty(MOTOR2_PWM, 0);
    pwm_set_duty(SERVO_MOTOR_PWM, 0);
    __DMB();__set_PRIMASK(mask);
    /* Never reverse a moving motor as a substitute for braking. Direction
     * stays fixed while armed. PWM=0 is not a guaranteed stopping distance. */
}

#if VISION_ROUTE_SWITCH
uint16_t vision_drive_servo_command_us(void)
{
    uint32_t mask=__get_PRIMASK(),duty;__disable_irq();
    duty=(vd.motion_written_mask&4U)?vd.motion_written_pwm[2]:0;
    __set_PRIMASK(mask);
    return (uint16_t)(duty*1000000U/(PWM_DUTY_MAX*SERVO_MOTOR_FREQ));
}
#endif
static void write_changed(unsigned index,int channel,uint32_t duty)
{
    uint8_t bit=(uint8_t)(1U<<index);
    if(!(vd.motion_written_mask&bit)||vd.motion_written_pwm[index]!=duty) {
        pwm_set_duty(channel,duty);vd.motion_written_pwm[index]=duty;vd.motion_written_mask|=bit;
    }
}

/* All callers hold PRIMASK from target inspection through the last LDOK.
 * PIT may revoke a lease, but it never grants one. Foreground observation cannot
 * recreate a revoked lease even if its controller still says RUNNING. */
enum { VD_OUTPUT_DRIVE=1, VD_OUTPUT_MOTION, VD_OUTPUT_PROBE };
static int output_allowed(unsigned source,uint32_t now)
{
#if VISION_MIDTERM_AUTONOMOUS
    if(vd.autonomous_stopped)return 0;
#endif
    if(!vd.initialized || vd.output_authorized!=source || vd.guard_faults ||
       vd.control.output.faults || vd.mode_stop_pending ||
       app_mode_change_pending())return 0;
#if !VISION_MIDTERM_C31_AUTONOMOUS
    /* C31 autonomous has no legacy C26/C27 output interlock. */
    if(vd.switch_stop_pending || vd.switch_inhibit ||
       (vd.switch_seen && !vd.switch_closed))return 0;
#endif
    if(source==VD_OUTPUT_DRIVE) {
#if VISION_MIDTERM_C31_AUTONOMOUS
        if(!vd.control.have_frame ||
           (uint32_t)(now-vd.control.last_frame_ms)>=VISION_CONTROL_FRAME_TIMEOUT_MS) {
            /* Latch before zeroing clears guard_running and hides this from PIT. */
            vd.autonomous_stopped=1;vd.guard_faults|=VC_FAULT_FRAME;
            return 0;
        }
#endif
        if(!app_mode_allows(APP_MODE_DRIVE) || app_mode_change_pending() ||
           !vd.hardware_ready || !vd.control.configured ||
           !vision_drive_profile_ready(&vd.profile) || vd.probe_active || vd.motion.phase ||
           vd.control.output.state!=VC_RUNNING ||
           (vd.control.local_run && !vd.local_permitted) || !vd.control.have_frame ||
           (uint32_t)(now-vd.control.last_frame_ms)>=VISION_CONTROL_FRAME_TIMEOUT_MS ||
           (!vd.control.local_run && (uint32_t)(now-vd.control.last_command_ms)>=VISION_CONTROL_COMMAND_TIMEOUT_MS) ||
           (vd.guard_max_run_ms && (uint32_t)(now-vd.control.armed_ms)>=vd.guard_max_run_ms))return 0;
#if VISION_MIDTERM_AUTONOMOUS
        if(!vd.control.output.observation_valid || !vd.control.output.far_valid)return 0;
#else
        /* Switch retains its existing bounded visual recovery controller. */
        if(!vd.control.output.observation_valid && !vd.control.output.recovering)return 0;
#endif
        return 1;
    }
#if VISION_MIDTERM_AUTONOMOUS
    return 0; /* No diagnostic output authority in autonomous firmware. */
#else
    if(vd.control.output.state!=VC_DISARMED)return 0;
    if(source==VD_OUTPUT_PROBE) {
        /* Calibration probes deliberately precede profile hardware confirmation.
         * Readiness here is an explicit bounded bench request, never drive-ready. */
        uint32_t minimum=vd.probe_active==1?VISION_SERVO_PROBE_MIN_MS:VISION_MOTOR_PROBE_MIN_MS;
        uint32_t maximum=vd.probe_active==1?VISION_SERVO_PROBE_MAX_MS:VISION_MOTOR_PROBE_MAX_MS;
        return !vd.motion.phase && vd.probe_active>=1 && vd.probe_active<=3 &&
            app_mode_allows(vd.probe_active==1?APP_MODE_SERVO:APP_MODE_MOTOR) &&
            vd.control.config.cruise_duty==0 && vd.control.config.duty_cap==0 &&
            vd.probe_hold_ms>=minimum && vd.probe_hold_ms<=maximum &&
            vd.probe_duty>0 &&
            (vd.probe_active!=1 ||
             (vd.probe_duty>=(uint32_t)VISION_SERVO_PROBE_MIN_US*SERVO_MOTOR_FREQ*PWM_DUTY_MAX/1000000U &&
              vd.probe_duty<=(uint32_t)VISION_SERVO_PROBE_MAX_US*SERVO_MOTOR_FREQ*PWM_DUTY_MAX/1000000U)) &&
            (uint32_t)(now-vd.probe_started_ms)<vd.probe_hold_ms;
    }
    return source==VD_OUTPUT_MOTION && app_mode_allows(APP_MODE_DRIVE) && !vd.probe_active &&
        (vd.motion.phase==VD_MOTION_SETTLE || vd.motion.phase==VD_MOTION_FORWARD ||
         vd.motion.phase==VD_MOTION_REVERSE_SETTLE || vd.motion.phase==VD_MOTION_REVERSE) &&
        vd.profile.hardware_confirmed && vd.profile.motor_direction_confirmed &&
        vision_drive_profile_valid(&vd.profile) &&
        vd.profile.control.cruise_duty==0 && vd.profile.control.duty_cap>0 &&
        vd.motion.kind>=VD_MOTION_LINE && vd.motion.kind<=VD_MOTION_CIRCLE &&
        vd.motion.requested_permille>0 && vd.motion.requested_permille<=VISION_MOTION_MAX_PERMILLE &&
        vd.motion.requested_permille<=(uint32_t)(vd.profile.control.duty_cap*1000.0f+0.001f) &&
        vd.motion.actual_permille<=vd.motion.requested_permille &&
        vd.motion_scale1>0 && vd.motion_scale1<=1000 && vd.motion_scale2>0 && vd.motion_scale2<=1000 &&
        vd.motion.servo_us>=vd.profile.servo_min_us && vd.motion.servo_us<=vd.profile.servo_max_us &&
        vd.motion.duration_ms>=VISION_MOTION_MIN_MS &&
        vd.motion.duration_ms<=(vd.motion.kind==VD_MOTION_LINE?VISION_MOTION_LINE_MAX_MS:VISION_MOTION_MAX_MS) &&
        (uint32_t)(now-vd.motion.heartbeat_ms)<VISION_CONTROL_COMMAND_TIMEOUT_MS;
#endif
}
int vd_output_arm(uint32_t now,int local)
{
    uint32_t mask=__get_PRIMASK();int accepted=0;__disable_irq();
#if VISION_MIDTERM_AUTONOMOUS
    if(vd.autonomous_stopped || !local){__set_PRIMASK(mask);return 0;}
#endif
    if(vd.initialized && !vd.guard_faults && !vd.control.output.faults &&
       !vd.mode_stop_pending &&
#if !VISION_MIDTERM_C31_AUTONOMOUS
       !vd.switch_stop_pending && !vd.switch_inhibit &&
       !(vd.switch_seen && !vd.switch_closed) &&
#endif
       !vd.probe_active && !vd.motion.phase &&
       app_mode_allows(APP_MODE_DRIVE) && !app_mode_change_pending() &&
       vd.hardware_ready && vision_drive_profile_ready(&vd.profile) &&
       (!local || vd.local_permitted)) {
        accepted=local?vc_arm_local(&vd.control,now,vd.hardware_ready):
            vc_arm(&vd.control,now,vd.hardware_ready);
        if(accepted)vd.output_authorized=VD_OUTPUT_DRIVE;
    }
    __DMB();__set_PRIMASK(mask);return accepted;
}
#if !VISION_MIDTERM_AUTONOMOUS
int vd_output_bench_start(void)
{
    uint32_t mask=__get_PRIMASK();int accepted=0;__disable_irq();
    /* Only explicit probe/start/reverse requests call this, never poll or ISR. */
    vd.output_authorized=vd.motion.phase?VD_OUTPUT_MOTION:VD_OUTPUT_PROBE;
    if(output_allowed(vd.output_authorized,timer_get(GPT_TIM_1))) {
        vd_output_apply();accepted=vd.output_authorized!=0;
    } else vd_output_zero();
    __DMB();__set_PRIMASK(mask);return accepted;
}
#endif
static void output_commit(unsigned source,uint32_t right,uint32_t left,uint32_t servo)
{
    uint32_t cap=PWM_DUTY_MAX; /* Independent actual PWM hard limit: 100%. */
    if(!output_allowed(source,timer_get(GPT_TIM_1))) {vd_output_zero();return;}
    if(source==VD_OUTPUT_PROBE)cap=(uint32_t)VISION_MOTOR_PROBE_MAX_PERMILLE*PWM_DUTY_MAX/1000U;
    if(source==VD_OUTPUT_MOTION)cap=(uint32_t)(vd.profile.control.duty_cap*PWM_DUTY_MAX);
#if !VISION_ROUTE_SWITCH
    if(source==VD_OUTPUT_DRIVE)cap=(uint32_t)(vd.profile.control.duty_cap*PWM_DUTY_MAX);
#endif
    if(cap>PWM_DUTY_MAX)cap=PWM_DUTY_MAX;
    /* Bound PI's actual result independently of its target/speed cap. C31
     * deliberately retains its existing full physical PWM headroom. */
    if(right>cap)right=cap;if(left>cap)left=cap;
    if(servo>PWM_DUTY_MAX) {vd.guard_faults|=VC_FAULT_NUMERIC;vd_output_zero();return;}
    write_changed(2,SERVO_MOTOR_PWM,servo);
    write_changed(0,MOTOR1_PWM,right);write_changed(1,MOTOR2_PWM,left);
}
static void motion_apply(void)
{
    uint8_t phase=vd.motion.phase;
    uint32_t duty=vd.motion.actual_permille;
    int reverse=phase==VD_MOTION_REVERSE_SETTLE || phase==VD_MOTION_REVERSE;
    if(!phase || phase==VD_MOTION_WAIT_REVERSE){vd_output_zero();return;}
    if(!output_allowed(VD_OUTPUT_MOTION,timer_get(GPT_TIM_1))){vd_output_zero();return;}
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
        output_commit(VD_OUTPUT_MOTION,values[0],values[1],values[2]);
    }
}

void vd_output_apply(void)
{
    if (!vd.initialized) return;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if(vd.motion.phase) {
        vd_motion_tick(timer_get(GPT_TIM_1));
        if(vd.motion.phase)motion_apply();else vd_output_zero();
        __DMB();__set_PRIMASK(mask);return;
    }
    if (((vd.probe_active==1 && app_mode_allows(APP_MODE_SERVO)) ||
         (vd.probe_active>=2 && app_mode_allows(APP_MODE_MOTOR))) && !vd.guard_faults &&
#if !VISION_MIDTERM_C31_AUTONOMOUS
        !vd.switch_inhibit && !(vd.switch_seen && !vd.switch_closed) &&
#endif
        vd.control.output.state == VC_DISARMED) {
        /* Separate bounded bench path: at most one actuator. Camera frames
         * cannot arm driving or overwrite this explicitly requested pulse. */
        vd.guard_running = 0;
        output_commit(VD_OUTPUT_PROBE,vd.probe_active==2?vd.probe_duty:0,
            vd.probe_active==3?vd.probe_duty:0,vd.probe_active==1?vd.probe_duty:0);
    } else if (vd.guard_faults ||
#if !VISION_MIDTERM_C31_AUTONOMOUS
               vd.switch_inhibit || (vd.switch_seen && !vd.switch_closed) ||
#endif
               !app_mode_allows(APP_MODE_DRIVE) || !vd.hardware_ready || vd.control.output.state != VC_RUNNING) {
        vd.probe_active = 0;
        vd.guard_running = 0;
        vd_output_zero();
    } else {
        if(!output_allowed(VD_OUTPUT_DRIVE,timer_get(GPT_TIM_1))) {
            vd_output_zero();__set_PRIMASK(mask);return;
        }
        if(!(vd.control.output.steering>=-1 && vd.control.output.steering<=1) ||
           !(vd.control.output.duty>=0 && vd.control.output.duty<=vd.profile.control.duty_cap) ||
           !(vd.control.output.lateral>=-3 && vd.control.output.lateral<=3) ||
           !(vd.control.output.heading>=-10 && vd.control.output.heading<=10) ||
           !(vd.control.recovery_steer>=-1 && vd.control.recovery_steer<=1) ||
           !(vd.control.output.paired_support>=0 && vd.control.output.paired_support<=1) ||
           !(vd.control.output.edge_support>=0 && vd.control.output.edge_support<=1)) {
            vd.guard_faults|=VC_FAULT_NUMERIC;vc_trip(&vd.control,VC_FAULT_NUMERIC);
            vd_output_zero();__set_PRIMASK(mask);return;
        }
        float steer = vd.control.output.steering * vd.profile.right_sign;
#if VISION_ROUTE_SWITCH
        /* Keep about the previous center slope while adding bend authority:
         * for the 120us span, offset = 80*u + 40*u^3 microseconds. Continuous,
         * monotonic and symmetric; endpoints remain the validated profile. */
        steer=vs_steering_shape(vd.control.output.steering)*vd.profile.right_sign;
#endif
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
#if VISION_ROUTE_SWITCH
        /* M1 RIGHT, M2 LEFT. Let the inner wheel run slower with the steering
         * command; equal closed-loop wheel targets can fight a tight bend.
         * Never boost the outer wheel or exceed the existing duty cap. */
        {
         float offset=vd.control.output.lateral;
         if(offset<0)offset=-offset;
         if(vd.control.output.observation_valid&&offset>1.0f) {
            float slow=1.0f-.20f*(offset-1.0f);
            if(slow<.75f)slow=.75f;
            motor1*=slow;motor2*=slow;
         }
         vs_steering_mix(vs_steering_shape(vd.control.output.steering),&motor1,&motor2);}
#endif
        /* Check derived floats before any integer conversion (also rejects NaN/Inf). */
        if(!(pulse>=vd.profile.servo_min_us && pulse<=vd.profile.servo_max_us) ||
           !(motor1>=0 && motor1<=vd.profile.control.duty_cap) ||
           !(motor2>=0 && motor2<=vd.profile.control.duty_cap)) {
            vd.guard_faults|=VC_FAULT_NUMERIC;vc_trip(&vd.control,VC_FAULT_NUMERIC);
            vd_output_zero();__set_PRIMASK(mask);return;
        }
        uint32_t servo_duty = (uint32_t)(pulse * SERVO_MOTOR_FREQ * PWM_DUTY_MAX / 1000000.0f);
        uint32_t right_duty,left_duty;
#if VISION_ENCODER_ENABLE
        uint16_t right=(uint16_t)(motor1*1000.0f),left=(uint16_t)(motor2*1000.0f);
        if(!vd_speed_output(right,left,0,(uint16_t)(vd.profile.control.duty_cap*1000.0f+0.001f),&right,&left)) {
            __DMB();__set_PRIMASK(mask);return;
        }
        right_duty=(uint32_t)right*PWM_DUTY_MAX/1000U;
        left_duty=(uint32_t)left*PWM_DUTY_MAX/1000U;
#else
        /* Open-loop duty maps directly to PWM without permille quantization. */
        right_duty=(uint32_t)(motor1*PWM_DUTY_MAX);
        left_duty=(uint32_t)(motor2*PWM_DUTY_MAX);
#endif
        /* Controller outputs are bounded; vd.profile validates pulse range first.
         * This short critical section prevents foreground from re-enabling
         * PWM after a safety ISR has latched a stop. No UART/float in ISR. */
        gpio_set_level(MOTOR1_DIR, vd.profile.motor1_forward_level);
        gpio_set_level(MOTOR2_DIR, vd.profile.motor2_forward_level);
        output_commit(VD_OUTPUT_DRIVE,right_duty,left_duty,servo_duty);
        if(!vd.output_authorized){__set_PRIMASK(mask);return;}
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
