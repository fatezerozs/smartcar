#include "vision_drive_internal.h"
#include "midterm_boot.h"

/* Lifecycle and command facade; IO, guard, startup, probing and profile
 * validation live in their own implementation units. */
vd_context_t vd;
#if VISION_MIDTERM_ROUTE_TRIAL
static uint8_t route_speed_pending;
#endif
#if VISION_ROUTE_SWITCH
static vd_last_stop_t route_last_stop;
static uint8_t route_fault_recorded;
static float route_previous_lateral,route_previous_heading,route_previous_steering;
static uint16_t route_previous_servo;
static vd_trace_sample_t route_trace[VD_TRACE_CAPACITY],trace_visual;
static vd_trace_info_t trace_info;
static unsigned trace_head;
static uint32_t trace_last_ms;
static uint8_t trace_released;
static int16_t trace_scaled(float value)
{
    float n=value*1000.0f;
    if(n>32767)n=32767;if(n< -32767)n=-32767;
    return (int16_t)n;
}
static void route_trace_add(uint32_t now,unsigned stop_reason)
{
    vd_trace_sample_t *p=&route_trace[trace_head];vd_speed_info_t speed;
    vision_drive_speed_get(&speed);*p=trace_visual;
    p->ms=now;p->faults=vd.control.output.faults|vd.guard_faults;
    p->lateral=trace_scaled(vd.control.output.lateral);p->heading=trace_scaled(vd.control.output.heading);
    p->steer=trace_scaled(vd.control.output.steering);p->duty=trace_scaled(vd.control.output.duty);
    p->raw3=speed.raw3;p->raw4=speed.raw4;p->target_right=speed.target_right;p->target_left=speed.target_left;
    p->pwm_right=speed.pwm_right;p->pwm_left=speed.pwm_left;p->servo_us=vision_drive_servo_command_us();
    p->state=stop_reason?(uint8_t)(stop_reason==1?VC_DISARMED:VC_FAULT):(uint8_t)vd.control.output.state;
    p->source=vd.control.output.search_source;
    p->obs_flags=(uint8_t)(vd.control.output.observation_valid|(vd.control.output.far_valid<<1)|(vd.control.output.recovering<<2));
    trace_head=(trace_head+1U)%VD_TRACE_CAPACITY;
    if(trace_info.count<VD_TRACE_CAPACITY)++trace_info.count;
    trace_last_ms=now;
}
static void route_trace_observe(uint32_t now)
{
    if(vd.control.output.state!=VC_RUNNING)return;
    if(!trace_info.active){
        /* Vision-only auto-reacquisition belongs to the same physical drive.
         * Keep pre-loss records instead of erasing them at every local arm. */
        int resume=!trace_released&&trace_info.frozen&&app_mode_allows(APP_MODE_DRIVE)&&route_last_stop.reason==2&&
            route_last_stop.faults&&!(route_last_stop.faults&~(uint32_t)(VC_FAULT_VISION|VC_FAULT_FRAME));
        if(!resume){++trace_info.run;trace_info.count=0;trace_head=0;}
        trace_info.active=1;trace_info.frozen=0;trace_released=0;
    }
    if(!trace_info.count||(uint32_t)(now-trace_last_ms)>=60U)route_trace_add(now,0);
}
void vision_drive_trace_info(vd_trace_info_t *out){*out=trace_info;}
int vision_drive_trace_read(unsigned index,vd_trace_sample_t *out)
{
    if(!out||index>=trace_info.count||!trace_info.frozen)return 0;
    unsigned first=(trace_head+VD_TRACE_CAPACITY-trace_info.count)%VD_TRACE_CAPACITY;
    *out=route_trace[(first+index)%VD_TRACE_CAPACITY];return 1;
}
static void route_record_stop(uint8_t reason,uint32_t faults)
{
    if(trace_info.active){route_trace_add(timer_get(GPT_TIM_1),reason);trace_info.active=0;trace_info.frozen=1;++trace_info.revision;}
    vd_speed_info_t s;vision_drive_speed_get(&s);
    ++route_last_stop.sequence;route_last_stop.ms=timer_get(GPT_TIM_1);
    route_last_stop.run_ms=route_last_stop.ms-vd.control.armed_ms;
    route_last_stop.reason=reason;route_last_stop.faults=faults;
    route_last_stop.speed_detail=s.fault_detail;
    route_last_stop.previous_lateral=route_previous_lateral;
    route_last_stop.previous_heading=route_previous_heading;
    route_last_stop.previous_steering=route_previous_steering;
    route_last_stop.previous_servo_us=route_previous_servo;
    route_last_stop.raw3=(faults&VC_FAULT_ENCODER)?s.fault_raw3:s.raw3;
    route_last_stop.raw4=(faults&VC_FAULT_ENCODER)?s.fault_raw4:s.raw4;
    route_last_stop.target_right=(faults&VC_FAULT_ENCODER)?s.fault_target_right:s.target_right;
    route_last_stop.target_left=(faults&VC_FAULT_ENCODER)?s.fault_target_left:s.target_left;
    route_last_stop.pwm_right=(faults&VC_FAULT_ENCODER)?s.fault_pwm_right:s.pwm_right;
    route_last_stop.pwm_left=(faults&VC_FAULT_ENCODER)?s.fault_pwm_left:s.pwm_left;
}
static void route_record_fault(void)
{
    if(vd.control.output.state==VC_FAULT){
        if(!route_fault_recorded){route_record_stop(2,vd.control.output.faults);route_fault_recorded=1;}
    } else route_fault_recorded=0;
}
void vision_drive_last_stop(vd_last_stop_t *out){*out=route_last_stop;}
#endif

void vision_drive_init(uint32_t now)
{
    vc_config_t cfg;
#if VISION_ROUTE_SWITCH
    memset(&route_last_stop,0,sizeof(route_last_stop));route_fault_recorded=0;
    route_previous_lateral=route_previous_heading=route_previous_steering=0;route_previous_servo=0;
    memset(&trace_info,0,sizeof(trace_info));memset(&trace_visual,0,sizeof(trace_visual));trace_head=0;trace_last_ms=0;trace_released=1;
#endif
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
#if VISION_ENCODER_ENABLE
    vd_speed_init(now);
#endif
    vd_output_zero();
#if VISION_ROUTE_SWITCH
    vd_route_switch_init();
#endif
    /* board_init 已将 PWM 初始化为零，PIT0 已无编码器/PID 输出路径。
     * 保留 PIT1 为独立 10ms 安全租约；不得挪作阻塞 UART/视觉计算。 */
    pit_ms_init(PIT_CH1, VISION_CONTROL_SAFETY_TICK_MS);
    interrupt_set_priority(PIT_IRQn, 0);
#if VISION_MIDTERM_ROUTE_TRIAL
    /* Measured field profile, RAM only. Never arm or save at boot. */
    {
        vd_profile_t p=vd.profile;
        p.servo_center_us=1550;p.servo_min_us=1500;p.servo_max_us=1600;p.right_sign=-1;
        p.hardware_confirmed=p.motor_direction_confirmed=1;
        p.motor1_forward_level=p.motor2_forward_level=1;
        p.motor1_scale=p.motor2_scale=1.0f;p.autostart_enabled=0;
        p.control.axis_near_x=MT_ROUTE_AXIS_NEAR_X;p.control.axis_far_x=MT_ROUTE_AXIS_FAR_X;
        p.control.near_y=0.78f;p.control.far_y=0.62f;p.control.band_half_height=0.035f;
        p.control.calibration_confirmed=1;p.control.cruise_duty=0.10f;
        p.control.duty_cap=0.15f;p.control.duty_rise_per_s=0.10f;
        p.control.max_run_ms=MT_ROUTE_MAX_RUN_MS;
#if VISION_ROUTE_SWITCH
        p.control.kp=1.20f;p.control.heading_gain=1.20f;
        /* Use a closer strip to center the chassis before the far bend can
         * pull it along an edge. Extrapolate the measured camera axis. */
        p.control.near_y=.86f;
        p.control.axis_near_x=MT_ROUTE_AXIS_FAR_X+
            (MT_ROUTE_AXIS_NEAR_X-MT_ROUTE_AXIS_FAR_X)*(.86f-.62f)/(.78f-.62f);
        p.control.cruise_duty=0.145f;p.control.duty_rise_per_s=0.20f;
        p.control.duty_cap=.18f;
        p.control.max_run_ms=0; /* C26 ON: continuous laps; OFF revokes authority. */
        /* C26_14: +/-120us endpoints, progressively applied in the actuator.
         * These are commanded bounds, not a measured mechanical angle. */
        p.servo_min_us=1430;p.servo_max_us=1670;
        p.control.min_coverage=VISION_TRACK_MIN_COVERAGE;
#endif
        route_speed_pending=(uint8_t)vision_drive_configure(&p);
    }
#endif
#endif
}

void vision_drive_poll(uint32_t now)
{
#if VISION_DRIVE_ENABLE
#if VISION_ROUTE_SWITCH
    if(!app_mode_allows(APP_MODE_DRIVE))trace_released=1;
#endif
    if (vd.mode_stop_pending) {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        vd.mode_stop_pending=0;__set_PRIMASK(mask);
#if VISION_ROUTE_SWITCH
        vd_route_switch_mode_stop();
#else
        vision_drive_stop();
#endif
    }
#if VISION_MIDTERM_ROUTE_TRIAL
    /* Wait for the first real PIT sample. Prepare exactly once per boot:
     * later STOP/config changes/faults must not silently re-enable PI. */
    if(route_speed_pending) {
        vd_speed_info_t s;vision_drive_speed_get(&s);
        if(s.samples && (uint32_t)(now-s.sample_ms)<=20U) {
            route_speed_pending=0;
            if(!(vision_drive_encoder_signs(-1,1) &&
                 vision_drive_speed_config(VISION_ROUTE_SWITCH?50:40,1.0f,0.05f) && vision_drive_speed_enable(1)))
                vision_drive_trip(VC_FAULT_CONFIG);
        }
    }
#endif
    vd_startup_poll_switch(now);
    vd_probe_poll(now);
    vd_motion_poll(now);
    if (vd.guard_faults) vc_trip(&vd.control, vd.guard_faults);
    vc_poll(&vd.control, now);
#if VISION_ROUTE_SWITCH
    route_record_fault(); /* Save before stable OFF acknowledges/clears faults. */
#endif
    vd_startup_poll_deadline(now);
#if VISION_ROUTE_SWITCH
    vd_route_switch_poll(now);
#endif
    if ((vd.control.output.state != VC_RUNNING && vd.guard_running)
#if VISION_ENCODER_ENABLE
        || vd.control.output.state==VC_RUNNING /* Consume encoder samples between camera frames. */
#endif
       ) vd_output_apply();
#else
    (void)now;
#endif
}

void vision_drive_observe(const vision_result_t *r, uint32_t sequence, uint32_t frame_ms, uint32_t now)
{
#if VISION_DRIVE_ENABLE
    vision_drive_poll(now);
#if VISION_ROUTE_SWITCH
    trace_visual.frame=sequence;trace_visual.valid=r->valid;trace_visual.coverage=r->confidence;
    trace_visual.row_flags=r->row_flags[90];trace_visual.center=r->center[90];trace_visual.width=r->row_width[90];
    trace_visual.measured=r->measured_rows;trace_visual.inferred=r->inferred_rows;trace_visual.junction=r->junction_rows;
    trace_visual.anchors=r->junction_anchors;
    trace_visual.ring=r->roundabout_state;trace_visual.ring_rows=r->roundabout_rows;
    trace_visual.ring_side=r->roundabout_side;
#endif
    vc_observe(&vd.control, r, sequence, frame_ms, now);
#if VISION_ROUTE_SWITCH
    route_record_fault();
#endif
    vd_output_apply();
#if VISION_ROUTE_SWITCH
    route_trace_observe(now);
    if(vd.control.output.state==VC_RUNNING && vd.control.output.observation_valid){
        route_previous_lateral=vd.control.output.lateral;
        route_previous_heading=vd.control.output.heading;
        route_previous_steering=vd.control.output.steering;
        route_previous_servo=vision_drive_servo_command_us();
    }
#endif
#else
    (void)r; (void)sequence; (void)frame_ms; (void)now;
#endif
}

void vision_drive_stop(void)
{
#if VISION_ROUTE_SWITCH
    route_record_fault();
    if(vd.control.output.state==VC_RUNNING)
        route_record_stop(vd.guard_faults?2:1,vd.guard_faults);
    vd_route_switch_cancel();
#endif
#if VISION_MIDTERM_AUTONOMOUS
    midterm_boot_cancel(timer_get(GPT_TIM_1));
#endif
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
#if VISION_ROUTE_SWITCH
    vd_route_switch_cancel();
#endif
#if VISION_MIDTERM_AUTONOMOUS
    midterm_boot_cancel(timer_get(GPT_TIM_1));
#endif
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
#if VISION_MIDTERM_AUTONOMOUS
    if(command!='s')return 0; /* No remote arm/clear/replay in acceptance firmware. */
#endif
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
#if VISION_ROUTE_SWITCH
    if(command=='a')return 0; /* Only a fresh physical OFF->ON can start. */
#endif
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
