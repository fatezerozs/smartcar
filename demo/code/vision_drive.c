#include "main.h"
#include "vision_drive.h"

static vc_controller_t control;
#if VISION_DRIVE_ENABLE
static uint8_t initialized, hardware_ready;
static volatile uint32_t guard_faults, guard_frame_ms, guard_command_ms, guard_armed_ms;
static volatile uint8_t guard_running;

static void zero_outputs(void)
{
    pwm_set_duty(MOTOR1_PWM, 0);
    pwm_set_duty(MOTOR2_PWM, 0);
    pwm_set_duty(SERVO_MOTOR_PWM, 0);
    /* Never reverse a moving motor as a substitute for braking. Direction
     * stays fixed while armed. PWM=0 is not a guaranteed stopping distance. */
}

static int profile_ready(void)
{
    return VISION_DRIVE_PROFILE_CONFIRMED == 1 && VISION_CAMERA_CALIBRATED == 1 &&
        (VISION_SERVO_RIGHT_SIGN == 1 || VISION_SERVO_RIGHT_SIGN == -1) &&
        VISION_SERVO_MIN_US >= 500 && VISION_SERVO_MAX_US <= 2500 &&
        VISION_SERVO_MIN_US < VISION_SERVO_CENTER_US &&
        VISION_SERVO_CENTER_US < VISION_SERVO_MAX_US &&
        VISION_SERVO_MAX_US < 1000000U / SERVO_MOTOR_FREQ &&
        VISION_MOTOR_DUTY_CAP > 0 && VISION_MOTOR_DUTY_CAP <= 1 &&
        VISION_MOTOR_CRUISE_DUTY > 0 && VISION_MOTOR_CRUISE_DUTY <= VISION_MOTOR_DUTY_CAP;
}

static void apply_output(void)
{
    if (!initialized) return;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (guard_faults || !hardware_ready || control.output.state != VC_RUNNING) {
        guard_running = 0;
        zero_outputs();
    } else {
        float steer = control.output.steering * VISION_SERVO_RIGHT_SIGN;
        float pulse = VISION_SERVO_CENTER_US + steer * (steer >= 0 ?
            VISION_SERVO_MAX_US - VISION_SERVO_CENTER_US :
            VISION_SERVO_CENTER_US - VISION_SERVO_MIN_US);
        uint32_t duty = (uint32_t)(control.output.duty * PWM_DUTY_MAX);
        uint32_t servo_duty = (uint32_t)(pulse * SERVO_MOTOR_FREQ * PWM_DUTY_MAX / 1000000.0f);
        /* Controller outputs are bounded; profile validates pulse range first.
         * This short critical section prevents foreground from re-enabling
         * PWM after a safety ISR has latched a stop. No UART/float in ISR. */
        gpio_set_level(MOTOR1_DIR, MOTOR1_FORWARD_DIR_LEVEL);
        gpio_set_level(MOTOR2_DIR, MOTOR2_FORWARD_DIR_LEVEL);
        pwm_set_duty(SERVO_MOTOR_PWM, servo_duty);
        pwm_set_duty(MOTOR1_PWM, duty);
        pwm_set_duty(MOTOR2_PWM, duty);
        guard_frame_ms = control.last_frame_ms;
        guard_command_ms = control.last_command_ms;
        guard_armed_ms = control.armed_ms;
        guard_running = 1;
    }
    __DMB();
    __set_PRIMASK(mask);
}
#endif

void vision_drive_init(uint32_t now)
{
    vc_config_t cfg;
    (void)now;
    vc_default_config(&cfg);
    vc_init(&control, &cfg);
#if VISION_DRIVE_ENABLE
    guard_running = 0; guard_faults = 0;
    hardware_ready = (uint8_t)profile_ready();
    initialized = 1;
    zero_outputs();
    /* main Init() already initializes PWM at zero. PIT0 encoder/PID remains
     * disabled in TEST_VISION. Reserve PIT1 for a 10ms independent lease guard. */
    pit_ms_init(PIT_CH1, VISION_CONTROL_SAFETY_TICK_MS);
    interrupt_set_priority(PIT_IRQn, 0);
#endif
}

void vision_drive_safety_tick(void)
{
#if VISION_DRIVE_ENABLE
    if (initialized && guard_running) {
        uint32_t now = timer_get(GPT_TIM_1), faults = 0;
        if ((uint32_t)(now - guard_frame_ms) >= VISION_CONTROL_FRAME_TIMEOUT_MS) faults |= VC_FAULT_FRAME;
        if ((uint32_t)(now - guard_command_ms) >= VISION_CONTROL_COMMAND_TIMEOUT_MS) faults |= VC_FAULT_COMMAND;
        if ((uint32_t)(now - guard_armed_ms) >= VISION_CONTROL_MAX_RUN_MS) faults |= VC_FAULT_RUNTIME;
        if (faults) {
            guard_faults |= faults;
            guard_running = 0;
            zero_outputs();
        }
    }
#endif
}

void vision_drive_poll(uint32_t now)
{
#if VISION_DRIVE_ENABLE
    if (guard_faults) vc_trip(&control, guard_faults);
    vc_poll(&control, now);
    if (control.output.state != VC_RUNNING && guard_running) apply_output();
#else
    (void)now;
#endif
}

void vision_drive_observe(const vision_result_t *r, uint32_t sequence, uint32_t frame_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE
    vision_drive_poll(now);
    vc_observe(&control, r, sequence, frame_ms, now);
    apply_output();
#else
    (void)r; (void)sequence; (void)frame_ms; (void)now;
#endif
}

void vision_drive_stop(void)
{
    vc_stop(&control);
#if VISION_DRIVE_ENABLE
    apply_output();
#endif
}

void vision_drive_trip(uint32_t reason)
{
    vc_trip(&control, reason | VC_FAULT_EXTERNAL);
#if VISION_DRIVE_ENABLE
    apply_output();
#endif
}

int vision_drive_command(char command, uint32_t now, int text_mode)
{
#if VISION_DRIVE_ENABLE
    vision_drive_poll(now);
    if (command == 's') { vision_drive_stop(); return 1; }
    if (command == 'h') {
        vc_heartbeat(&control, now);
        /* Heartbeat extends command lease, never the camera/update lease. */
        if (control.output.state == VC_RUNNING) guard_command_ms = control.last_command_ms;
        return control.output.state == VC_RUNNING;
    }
    if (command == 'c' && control.output.state != VC_RUNNING) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        guard_running = 0; zero_outputs(); guard_faults = 0;
        __set_PRIMASK(mask);
        vc_clear_fault(&control);
        return 1;
    }
    if (command == 'a' && text_mode && !guard_faults)
        return vc_arm(&control, now, hardware_ready);
#else
    (void)command; (void)now; (void)text_mode;
#endif
    return 0;
}

const vc_output_t *vision_drive_output(void) { return &control.output; }
int vision_drive_ready(void)
{
#if VISION_DRIVE_ENABLE
    return initialized && hardware_ready && control.configured;
#else
    return 0;
#endif
}
