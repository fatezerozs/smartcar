#include "vision_drive_internal.h"

int vision_drive_servo_probe(uint16_t pulse_us, uint16_t hold_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE && !VISION_MIDTERM_AUTONOMOUS
    uint32_t mask;int accepted;
    unsigned difference = pulse_us > vd.probe_last_us ? pulse_us - vd.probe_last_us : vd.probe_last_us - pulse_us;
    vision_drive_poll(now);
    if (!app_mode_allows(APP_MODE_SERVO) || !vd.initialized || vd.motion.phase || vd.guard_faults || vd.control.output.state != VC_DISARMED ||
        vd.switch_inhibit || (vd.switch_seen && !vd.switch_closed) ||
        vd.control.config.cruise_duty != 0 || vd.control.config.duty_cap != 0 ||
        pulse_us < VISION_SERVO_PROBE_MIN_US || pulse_us > VISION_SERVO_PROBE_MAX_US ||
        hold_ms < VISION_SERVO_PROBE_MIN_MS || hold_ms > VISION_SERVO_PROBE_MAX_MS ||
        (!vd.probe_last_us && pulse_us != VISION_SERVO_PROBE_INITIAL_US) ||
        (vd.probe_last_us && difference > VISION_SERVO_PROBE_STEP_US)) return 0;
    mask = __get_PRIMASK(); __disable_irq();
    /* Recheck after masking so a concurrent safety trip cannot be bypassed.
     * Deadlines start at the actual register write, not stale parse time. */
    if (!app_mode_allows(APP_MODE_SERVO) || vd.guard_faults || vd.control.output.state != VC_DISARMED) { __set_PRIMASK(mask); return 0; }
    vd.guard_running = 0; vd_output_zero();
    vd.probe_duty = (uint32_t)((uint64_t)pulse_us * SERVO_MOTOR_FREQ * PWM_DUTY_MAX / 1000000U);
    vd.probe_hold_ms = hold_ms; vd.probe_started_ms = timer_get(GPT_TIM_1);
    vd.probe_last_us = pulse_us; vd.probe_active = 1;
    ++vd.probe_info.sequence;vd.probe_info.kind=1;vd.probe_info.channel=0;vd.probe_info.level=0;
    vd.probe_info.value=pulse_us;vd.probe_info.hold_ms=hold_ms;
    accepted=vd_output_bench_start();
    if(!accepted)vd.probe_active=0;
    __DMB(); __set_PRIMASK(mask);
    return accepted;
#else
    (void)pulse_us; (void)hold_ms; (void)now;
    return 0;
#endif
}

int vision_drive_motor_probe(uint8_t channel, uint16_t duty_permille, uint8_t level,
                             uint16_t hold_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE && !VISION_MIDTERM_AUTONOMOUS
    uint32_t mask;uint16_t last;int accepted;
    vision_drive_poll(now);
    if (!app_mode_allows(APP_MODE_MOTOR) || !vd.initialized || vd.motion.phase || channel<1 || channel>2 || level>1 || vd.guard_faults ||
        (vd.motion_have_stopped && (uint32_t)(now-vd.motion.stopped_ms)<VISION_MOTION_COAST_MS) ||
        vd.switch_inhibit || (vd.switch_seen && !vd.switch_closed) ||
        vd.control.output.state!=VC_DISARMED || vd.probe_active ||
        vd.control.config.cruise_duty!=0 || vd.control.config.duty_cap!=0 ||
        duty_permille<1 || duty_permille>VISION_MOTOR_PROBE_MAX_PERMILLE ||
        hold_ms<VISION_MOTOR_PROBE_MIN_MS || hold_ms>VISION_MOTOR_PROBE_MAX_MS ||
        (vd.have_motor_probe && (uint32_t)(now-vd.motor_probe_started_ms)<vd.motor_probe_hold_ms+VISION_MOTOR_PROBE_COAST_MS)) return 0;
    last=vd.motor_last_permille[channel-1];
    if ((!last && duty_permille>VISION_MOTOR_PROBE_INITIAL_PERMILLE) ||
        (last && duty_permille>last+VISION_MOTOR_PROBE_STEP_PERMILLE)) return 0;
    mask=__get_PRIMASK();__disable_irq();
    if(!app_mode_allows(APP_MODE_MOTOR) || vd.guard_faults || vd.control.output.state!=VC_DISARMED){__set_PRIMASK(mask);return 0;}
    vd.guard_running=0;vd_output_zero();
    /* Pin level is an experiment input, not confirmed forward direction.
     * Never derive wheel identity from contradictory old comments; channels
     * are the actual MOTOR1/MOTOR2 macros, and the operator identifies them. */
    gpio_set_level(channel==1?MOTOR1_DIR:MOTOR2_DIR,level);
    vd.probe_duty=(uint32_t)duty_permille*PWM_DUTY_MAX/1000U;
    vd.probe_started_ms=timer_get(GPT_TIM_1);vd.probe_hold_ms=hold_ms;
    vd.motor_probe_started_ms=vd.probe_started_ms;vd.motor_probe_hold_ms=hold_ms;vd.have_motor_probe=1;
    vd.motor_last_permille[channel-1]=duty_permille;vd.probe_active=(uint8_t)(channel+1);
    ++vd.probe_info.sequence;vd.probe_info.kind=2;vd.probe_info.channel=channel;vd.probe_info.level=level;
    vd.probe_info.value=duty_permille;vd.probe_info.hold_ms=hold_ms;
    accepted=vd_output_bench_start();
    if(!accepted)vd.probe_active=0;
    __DMB();__set_PRIMASK(mask);return accepted;
#else
    (void)channel;(void)duty_permille;(void)level;(void)hold_ms;(void)now;return 0;
#endif
}

void vision_drive_get_probe(vd_probe_info_t *out)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask=__get_PRIMASK();__disable_irq();
    *out=vd.probe_info;out->active=vd.probe_active!=0;__set_PRIMASK(mask);
#else
    *out=vd.probe_info;
#endif
}

#if VISION_DRIVE_ENABLE
void vd_probe_poll(uint32_t now)
{
    if (vd.probe_active && (uint32_t)(now - vd.probe_started_ms) >= vd.probe_hold_ms) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq(); vd.probe_active = 0; vd_output_zero(); __set_PRIMASK(mask);
    }
}
#endif
