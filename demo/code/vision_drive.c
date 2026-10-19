#include "main.h"
#include "vision_drive.h"

static vc_controller_t control;
static vd_profile_t profile;
static vd_probe_info_t probe_info;
static vd_auto_info_t auto_info;

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
#if VISION_DRIVE_ENABLE
static uint8_t initialized, hardware_ready;
static volatile uint32_t guard_faults, guard_frame_ms, guard_command_ms, guard_armed_ms;
static volatile uint32_t guard_max_run_ms;
static volatile uint8_t guard_running;
static volatile uint8_t guard_local;
static volatile uint8_t switch_seen, switch_closed, switch_inhibit, switch_stop_pending;
static volatile uint32_t switch_changed_ms;
static uint8_t switch_rearm, switch_boot_pending;
static uint8_t local_permitted;
static volatile uint8_t probe_active; /* 1 servo, 2 motor1, 3 motor2. */
static volatile uint32_t probe_started_ms, probe_hold_ms, probe_duty;
static uint16_t probe_last_us;
static uint16_t motor_last_permille[2];
static uint8_t have_motor_probe;
static uint32_t motor_probe_started_ms, motor_probe_hold_ms;

static void zero_outputs(void)
{
    pwm_set_duty(MOTOR1_PWM, 0);
    pwm_set_duty(MOTOR2_PWM, 0);
    pwm_set_duty(SERVO_MOTOR_PWM, 0);
    /* Never reverse a moving motor as a substitute for braking. Direction
     * stays fixed while armed. PWM=0 is not a guaranteed stopping distance. */
}

static void apply_output(void)
{
    if (!initialized) return;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (probe_active && !guard_faults && !switch_inhibit &&
        !(switch_seen && !switch_closed) && control.output.state == VC_DISARMED) {
        /* Separate bounded bench path: at most one actuator. Camera frames
         * cannot arm driving or overwrite this explicitly requested pulse. */
        guard_running = 0;
        pwm_set_duty(MOTOR1_PWM, probe_active == 2 ? probe_duty : 0);
        pwm_set_duty(MOTOR2_PWM, probe_active == 3 ? probe_duty : 0);
        pwm_set_duty(SERVO_MOTOR_PWM, probe_active == 1 ? probe_duty : 0);
    } else if (guard_faults || switch_inhibit || (switch_seen && !switch_closed) ||
               !hardware_ready || control.output.state != VC_RUNNING) {
        probe_active = 0;
        guard_running = 0;
        zero_outputs();
    } else {
        float steer = control.output.steering * profile.right_sign;
        float pulse = profile.servo_center_us + steer * (steer >= 0 ?
            profile.servo_max_us - profile.servo_center_us :
            profile.servo_center_us - profile.servo_min_us);
        float motor1, motor2;
        if (!mol_split(control.output.duty, profile.control.duty_cap,
                       profile.motor1_scale, profile.motor2_scale, &motor1, &motor2)) {
            guard_faults |= VC_FAULT_NUMERIC;
            vc_trip(&control, VC_FAULT_NUMERIC);
            guard_running = 0;
            zero_outputs();
            __DMB(); __set_PRIMASK(mask);
            return;
        }
        uint32_t servo_duty = (uint32_t)(pulse * SERVO_MOTOR_FREQ * PWM_DUTY_MAX / 1000000.0f);
        /* Controller outputs are bounded; profile validates pulse range first.
         * This short critical section prevents foreground from re-enabling
         * PWM after a safety ISR has latched a stop. No UART/float in ISR. */
        gpio_set_level(MOTOR1_DIR, profile.motor1_forward_level);
        gpio_set_level(MOTOR2_DIR, profile.motor2_forward_level);
        pwm_set_duty(SERVO_MOTOR_PWM, servo_duty);
        pwm_set_duty(MOTOR1_PWM, (uint32_t)(motor1 * PWM_DUTY_MAX));
        pwm_set_duty(MOTOR2_PWM, (uint32_t)(motor2 * PWM_DUTY_MAX));
        guard_frame_ms = control.last_frame_ms;
        guard_command_ms = control.last_command_ms;
        guard_armed_ms = control.armed_ms;
        guard_local = control.local_run;
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
    memset(&profile, 0, sizeof(profile));
    profile.control = cfg;
    profile.servo_center_us = VISION_SERVO_CENTER_US;
    profile.servo_min_us = VISION_SERVO_MIN_US;
    profile.servo_max_us = VISION_SERVO_MAX_US;
    profile.right_sign = VISION_SERVO_RIGHT_SIGN;
    profile.hardware_confirmed = VISION_DRIVE_PROFILE_CONFIRMED;
    profile.motor_direction_confirmed = VISION_MOTOR_DIRECTION_CONFIRMED;
    profile.motor1_forward_level = MOTOR1_FORWARD_DIR_LEVEL;
    profile.motor2_forward_level = MOTOR2_FORWARD_DIR_LEVEL;
    profile.motor1_scale = VISION_MOTOR1_SCALE;
    profile.motor2_scale = VISION_MOTOR2_SCALE;
    profile.autostart_enabled = VISION_AUTO_START_ENABLE;
    profile.startup_delay_ms = VISION_AUTO_START_DELAY_MS;
    memset(&probe_info,0,sizeof(probe_info));
    memset(&auto_info,0,sizeof(auto_info));
#if VISION_DRIVE_ENABLE
    guard_running = 0; guard_faults = 0; guard_local = 0;
    switch_seen=0;switch_closed=0;switch_inhibit=VISION_START_SWITCH_ENABLE;switch_stop_pending=0;
    switch_rearm=0;switch_boot_pending=0;switch_changed_ms=now;
    local_permitted=0;
    probe_active = 0; probe_last_us = 0;
    memset(motor_last_permille,0,sizeof(motor_last_permille)); have_motor_probe = 0;
    guard_max_run_ms = cfg.max_run_ms;
    hardware_ready = (uint8_t)vision_drive_profile_ready(&profile);
    initialized = 1;
    zero_outputs();
    /* board_init 已将 PWM 初始化为零，PIT0 已无编码器/PID 输出路径。
     * 保留 PIT1 为独立 10ms 安全租约；不得挪作阻塞 UART/视觉计算。 */
    pit_ms_init(PIT_CH1, VISION_CONTROL_SAFETY_TICK_MS);
    interrupt_set_priority(PIT_IRQn, 0);
#endif
}

void vision_drive_safety_tick(void)
{
#if VISION_DRIVE_ENABLE
    if (initialized && probe_active &&
        (uint32_t)(timer_get(GPT_TIM_1) - probe_started_ms) >= probe_hold_ms) {
        probe_active = 0;
        zero_outputs(); /* Integer-only, also works if foreground/UART blocks. */
    }
    if (initialized && guard_running) {
        uint32_t now = timer_get(GPT_TIM_1), faults = 0;
        if ((uint32_t)(now - guard_frame_ms) >= VISION_CONTROL_FRAME_TIMEOUT_MS) faults |= VC_FAULT_FRAME;
        if (!guard_local && (uint32_t)(now - guard_command_ms) >= VISION_CONTROL_COMMAND_TIMEOUT_MS) faults |= VC_FAULT_COMMAND;
        if ((uint32_t)(now - guard_armed_ms) >= guard_max_run_ms) faults |= VC_FAULT_RUNTIME;
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
    if (switch_stop_pending) {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        switch_stop_pending=0;__set_PRIMASK(mask);
        vision_drive_stop();
    }
    if (switch_seen && !switch_closed &&
        (uint32_t)(now-switch_changed_ms)>=VISION_START_SWITCH_REARM_MS) {
        switch_rearm=1;
    }
    if (switch_seen && switch_closed &&
        (uint32_t)(now-switch_changed_ms)>=VISION_START_SWITCH_DEBOUNCE_MS) {
        switch_inhibit=0;
        if (auto_info.restored && profile.autostart_enabled && (switch_boot_pending || switch_rearm)) {
            uint32_t base=switch_boot_pending?auto_info.boot_ms:switch_changed_ms;
            switch_boot_pending=0;switch_rearm=0;
            auto_info.boot_ms=base;auto_info.delay_ms=profile.startup_delay_ms;
            auto_info.state=local_permitted && hardware_ready && control.output.state==VC_DISARMED &&
                (uint32_t)(now-base)<auto_info.delay_ms ? VD_AUTO_WAITING:VD_AUTO_BLOCKED;
        }
    }
    if (probe_active && (uint32_t)(now - probe_started_ms) >= probe_hold_ms) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq(); probe_active = 0; zero_outputs(); __set_PRIMASK(mask);
    }
    if (guard_faults) vc_trip(&control, guard_faults);
    vc_poll(&control, now);
    if (auto_info.state==VD_AUTO_WAITING &&
        (uint32_t)(now-auto_info.boot_ms)>=auto_info.delay_ms) {
        /* A blocked deadline is terminal for this boot: never creep into
         * motion later when a person moves away or a new scene becomes valid. */
        int accepted=local_permitted && switch_seen && switch_closed && !switch_inhibit &&
            !guard_faults && !probe_active && hardware_ready &&
            vc_arm_local(&control,now,hardware_ready);
        auto_info.state=accepted?VD_AUTO_STARTED:VD_AUTO_BLOCKED;
        apply_output(); /* First command is zero duty; next fresh frame ramps. */
    }
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
    if (auto_info.state==VD_AUTO_WAITING) auto_info.state=VD_AUTO_CANCELLED;
    vc_stop(&control);
#if VISION_DRIVE_ENABLE
    switch_boot_pending=0;switch_rearm=0;
    probe_active = 0;
    apply_output();
#endif
}

void vision_drive_trip(uint32_t reason)
{
    if (auto_info.state==VD_AUTO_WAITING) auto_info.state=VD_AUTO_CANCELLED;
    vc_trip(&control, reason | VC_FAULT_EXTERNAL);
#if VISION_DRIVE_ENABLE
    probe_active = 0;
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
        probe_active = 0; guard_running = 0; zero_outputs(); guard_faults = 0;
        __set_PRIMASK(mask);
        vc_clear_fault(&control);
        return 1;
    }
    if (command == 'a' && text_mode && !guard_faults && !probe_active &&
        !switch_inhibit &&
        !(switch_seen && (!switch_closed || switch_inhibit)) &&
        (!have_motor_probe || (uint32_t)(now-motor_probe_started_ms) >= motor_probe_hold_ms+VISION_MOTOR_PROBE_COAST_MS))
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

void vision_drive_get_profile(vd_profile_t *out)
{
    *out = profile;
    out->control = control.config;
}

int vision_drive_configure(const vd_profile_t *candidate)
{
#if VISION_DRIVE_ENABLE
    if (!candidate || !initialized || guard_faults || control.output.state != VC_DISARMED ||
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
    if (auto_info.state==VD_AUTO_WAITING) auto_info.state=VD_AUTO_CANCELLED;
    probe_active = 0; guard_running = 0; zero_outputs();
    if (!vc_reconfigure(&control, &candidate->control)) { __set_PRIMASK(mask); return 0; }
    profile = *candidate;
    guard_max_run_ms = candidate->control.max_run_ms;
    hardware_ready = (uint8_t)vision_drive_profile_ready(&profile);
    __DMB(); __set_PRIMASK(mask);
    return 1;
#else
    (void)candidate; return 0;
#endif
}

int vision_drive_servo_probe(uint16_t pulse_us, uint16_t hold_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask;
    unsigned difference = pulse_us > probe_last_us ? pulse_us - probe_last_us : probe_last_us - pulse_us;
    vision_drive_poll(now);
    if (!initialized || guard_faults || control.output.state != VC_DISARMED ||
        switch_inhibit || (switch_seen && !switch_closed) ||
        control.config.cruise_duty != 0 || control.config.duty_cap != 0 ||
        pulse_us < VISION_SERVO_PROBE_MIN_US || pulse_us > VISION_SERVO_PROBE_MAX_US ||
        hold_ms < VISION_SERVO_PROBE_MIN_MS || hold_ms > VISION_SERVO_PROBE_MAX_MS ||
        (!probe_last_us && pulse_us != VISION_SERVO_PROBE_INITIAL_US) ||
        (probe_last_us && difference > VISION_SERVO_PROBE_STEP_US)) return 0;
    mask = __get_PRIMASK(); __disable_irq();
    /* Recheck after masking so a concurrent safety trip cannot be bypassed.
     * Deadlines start at the actual register write, not stale parse time. */
    if (guard_faults || control.output.state != VC_DISARMED) { __set_PRIMASK(mask); return 0; }
    guard_running = 0; zero_outputs();
    probe_duty = (uint32_t)((uint64_t)pulse_us * SERVO_MOTOR_FREQ * PWM_DUTY_MAX / 1000000U);
    probe_hold_ms = hold_ms; probe_started_ms = timer_get(GPT_TIM_1);
    probe_last_us = pulse_us; probe_active = 1;
    ++probe_info.sequence;probe_info.kind=1;probe_info.channel=0;probe_info.level=0;
    probe_info.value=pulse_us;probe_info.hold_ms=hold_ms;
    pwm_set_duty(SERVO_MOTOR_PWM, probe_duty);
    __DMB(); __set_PRIMASK(mask);
    return 1;
#else
    (void)pulse_us; (void)hold_ms; (void)now;
    return 0;
#endif
}

int vision_drive_motor_probe(uint8_t channel, uint16_t duty_permille, uint8_t level,
                             uint16_t hold_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask;uint16_t last;
    vision_drive_poll(now);
    if (!initialized || channel<1 || channel>2 || level>1 || guard_faults ||
        switch_inhibit || (switch_seen && !switch_closed) ||
        control.output.state!=VC_DISARMED || probe_active ||
        control.config.cruise_duty!=0 || control.config.duty_cap!=0 ||
        duty_permille<1 || duty_permille>VISION_MOTOR_PROBE_MAX_PERMILLE ||
        hold_ms<VISION_MOTOR_PROBE_MIN_MS || hold_ms>VISION_MOTOR_PROBE_MAX_MS ||
        (have_motor_probe && (uint32_t)(now-motor_probe_started_ms)<motor_probe_hold_ms+VISION_MOTOR_PROBE_COAST_MS)) return 0;
    last=motor_last_permille[channel-1];
    if ((!last && duty_permille>VISION_MOTOR_PROBE_INITIAL_PERMILLE) ||
        (last && duty_permille>last+VISION_MOTOR_PROBE_STEP_PERMILLE)) return 0;
    mask=__get_PRIMASK();__disable_irq();
    if(guard_faults || control.output.state!=VC_DISARMED){__set_PRIMASK(mask);return 0;}
    guard_running=0;zero_outputs();
    /* Pin level is an experiment input, not confirmed forward direction.
     * Never derive wheel identity from contradictory old comments; channels
     * are the actual MOTOR1/MOTOR2 macros, and the operator identifies them. */
    gpio_set_level(channel==1?MOTOR1_DIR:MOTOR2_DIR,level);
    probe_duty=(uint32_t)duty_permille*PWM_DUTY_MAX/1000U;
    probe_started_ms=timer_get(GPT_TIM_1);probe_hold_ms=hold_ms;
    motor_probe_started_ms=probe_started_ms;motor_probe_hold_ms=hold_ms;have_motor_probe=1;
    motor_last_permille[channel-1]=duty_permille;probe_active=(uint8_t)(channel+1);
    ++probe_info.sequence;probe_info.kind=2;probe_info.channel=channel;probe_info.level=level;
    probe_info.value=duty_permille;probe_info.hold_ms=hold_ms;
    pwm_set_duty(channel==1?MOTOR1_PWM:MOTOR2_PWM,probe_duty);
    __DMB();__set_PRIMASK(mask);return 1;
#else
    (void)channel;(void)duty_permille;(void)level;(void)hold_ms;(void)now;return 0;
#endif
}

void vision_drive_get_probe(vd_probe_info_t *out)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask=__get_PRIMASK();__disable_irq();
    *out=probe_info;out->active=probe_active!=0;__set_PRIMASK(mask);
#else
    *out=probe_info;
#endif
}

void vision_drive_schedule_boot(uint32_t boot_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE
    /* Not re-schedulable within a boot, including after STOP/clear/configure. */
    if (auto_info.restored) return;
    auto_info.restored=1; auto_info.boot_ms=boot_ms;
    auto_info.delay_ms=profile.startup_delay_ms;
    switch_boot_pending=profile.autostart_enabled;
    if(profile.autostart_enabled)auto_info.state=VD_AUTO_WAITING;
    if (profile.autostart_enabled && (uint32_t)(now-boot_ms)>=profile.startup_delay_ms) {
        switch_boot_pending=0;auto_info.state=VD_AUTO_BLOCKED;
    }
#else
    (void)boot_ms; (void)now;
#endif
}

void vision_drive_get_auto(vd_auto_info_t *out)
{
    *out=auto_info;
#if VISION_DRIVE_ENABLE
    out->switch_seen=switch_seen;out->switch_closed=switch_closed;
#endif
}

void vision_drive_set_local_allowed(int allowed)
{
#if VISION_DRIVE_ENABLE
    local_permitted=(uint8_t)(allowed!=0);
    if(!allowed && (auto_info.state==VD_AUTO_WAITING || control.local_run))vision_drive_stop();
#else
    (void)allowed;
#endif
}
int vision_drive_is_local(void) { return control.output.state==VC_RUNNING && control.local_run; }
int vision_drive_can_store(void)
{
#if VISION_DRIVE_ENABLE
    return initialized && !guard_faults && !probe_active && control.output.state==VC_DISARMED;
#else
    return 0;
#endif
}

void vision_drive_switch_sample(int closed)
{
#if VISION_DRIVE_ENABLE
    uint32_t now;int changed;
    if(!initialized)return;
    now=timer_get(GPT_TIM_1);
    changed=!switch_seen || switch_closed!=(uint8_t)(closed!=0);
    if(changed) {
        switch_changed_ms=now;switch_closed=(uint8_t)(closed!=0);switch_seen=1;
    }
    if(!closed) {
        if(changed || !switch_inhibit || guard_running || probe_active) {
            switch_stop_pending=1;guard_running=0;probe_active=0;zero_outputs();
        }
        switch_inhibit=1;
    }
#else
    (void)closed;
#endif
}
