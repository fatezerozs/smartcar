#include "vision_control.h"
#include "vision_profile.h"
#include <float.h>
#include <string.h>
#if VISION_ROUTE_SWITCH
#include "vision_junction_policy.h"
#endif

static int finite_value(float x) { return x == x && x >= -FLT_MAX && x <= FLT_MAX; }
static float absolute(float x) { return x < 0 ? -x : x; }
static float clamp(float x, float low, float high) { return x < low ? low : (x > high ? high : x); }
static int range(float x, float low, float high) { return finite_value(x) && x >= low && x <= high; }

void vc_default_config(vc_config_t *c)
{
    memset(c, 0, sizeof(*c));
    mol_default_config(&c->motor);
    c->near_y = VISION_AXIS_NEAR_Y; c->far_y = VISION_AXIS_FAR_Y;
    c->axis_near_x = VISION_AXIS_NEAR_X; c->axis_far_x = VISION_AXIS_FAR_X;
    c->band_half_height = VISION_CONTROL_BAND_HALF_HEIGHT;
    c->kp = VISION_CONTROL_KP; c->kd = VISION_CONTROL_KD;
    c->heading_gain = VISION_CONTROL_HEADING_GAIN;
    c->derivative_filter_s = VISION_CONTROL_D_FILTER_S;
    c->steer_rate = VISION_CONTROL_STEER_RATE;
    c->duty_rise_per_s = VISION_CONTROL_DUTY_RISE_PER_S;
    c->duty_cap = VISION_MOTOR_DUTY_CAP; c->cruise_duty = VISION_MOTOR_CRUISE_DUTY;
    c->max_lateral = VISION_CONTROL_MAX_LATERAL;
    c->frame_timeout_ms = VISION_CONTROL_FRAME_TIMEOUT_MS;
    c->command_timeout_ms = VISION_CONTROL_COMMAND_TIMEOUT_MS;
    c->max_run_ms = VISION_CONTROL_MAX_RUN_MS;
    c->arm_frames = VISION_CONTROL_ARM_FRAMES;
    c->min_coverage = VISION_CONTROL_MIN_COVERAGE;
    c->min_band_rows = VISION_CONTROL_MIN_BAND_ROWS;
    c->calibration_confirmed = VISION_CAMERA_CALIBRATED;
}

int vc_config_valid(const vc_config_t *c)
{
    if (!c) return 0;
    return mol_config_valid(&c->motor) && range(c->near_y, 0, 1) && range(c->far_y, 0, 1) &&
        c->near_y > c->far_y && range(c->band_half_height, 0.01f, 0.15f) &&
        c->near_y - c->far_y > 2 * c->band_half_height &&
        c->near_y + c->band_half_height <= (float)VISION_ROI_BOTTOM / (VISION_HEIGHT - 1) &&
        c->far_y - c->band_half_height >= (float)VISION_ROI_TOP / (VISION_HEIGHT - 1) &&
        range(c->axis_near_x, 0, 1) && range(c->axis_far_x, 0, 1) &&
        range(c->kp, 0, 10) && range(c->kd, 0, 2) && range(c->heading_gain, 0, 10) &&
        range(c->derivative_filter_s, 0.001f, 2) && range(c->steer_rate, 0.01f, 20) &&
        range(c->duty_rise_per_s, 0.001f, 1) && range(c->duty_cap, 0, 1) &&
        range(c->cruise_duty, 0, c->duty_cap) && range(c->max_lateral, 0.1f, 3) &&
        c->frame_timeout_ms >= 20 && c->frame_timeout_ms <= 500 &&
        c->command_timeout_ms >= 20 && c->command_timeout_ms <= 2000 &&
        ((VISION_ROUTE_SWITCH && !c->max_run_ms) ||
         (c->max_run_ms >= c->frame_timeout_ms && c->max_run_ms <= (VISION_ROUTE_SWITCH?120000U:60000U))) &&
        c->arm_frames >= 2 && c->min_coverage >= 1 && c->min_coverage <= 100 &&
        c->min_band_rows >= 1 && c->min_band_rows <= 20 && c->calibration_confirmed <= 1;
}

void vc_trip(vc_controller_t *c, uint32_t reason)
{
    c->local_run = 0;
    c->output.state = VC_FAULT;
    c->output.faults |= reason;
    c->output.duty = c->output.steering = 0;
    c->output.speed_boost=0;
    c->output.paired_support=c->output.edge_support=0;c->output.recovery_drive=0;
    c->output.recovering=0;c->output.search_source=0;c->recovery_steer=0;
    c->have_partial=c->straight_tracking=0;
    c->stable_frames = 0;
    c->derivative = 0;
}

void vc_init(vc_controller_t *c, const vc_config_t *config)
{
    vc_config_t saved = *config; /* Also safe when clearing with &c->config. */
    memset(c, 0, sizeof(*c));
    c->config = saved;
    c->configured = (uint8_t)vc_config_valid(&saved);
    if (!c->configured) vc_trip(c, VC_FAULT_CONFIG);
}

void vc_stop(vc_controller_t *c)
{
    c->local_run = 0;
    if (c->output.state != VC_FAULT) c->output.state = VC_DISARMED;
    c->output.duty = c->output.steering = 0;
    c->output.speed_boost=0;
    c->output.paired_support=c->output.edge_support=0;c->output.recovery_drive=0;
    c->output.recovering=0;c->output.search_source=0;c->recovery_steer=0;
    c->have_partial=c->straight_tracking=0;
    c->stable_frames = 0;
    c->derivative = 0;
}

int vc_reconfigure(vc_controller_t *c, const vc_config_t *config)
{
    if (c->output.state != VC_DISARMED || !config || !vc_config_valid(config)) return 0;
    vc_init(c, config);
    return 1;
}

void vc_clear_fault(vc_controller_t *c)
{
    if (c->output.state == VC_RUNNING) return;
    vc_init(c, &c->config);
}

void vc_poll(vc_controller_t *c, uint32_t now)
{
    int remote=!c->local_run;
    if (c->output.state != VC_RUNNING) return;
#if VISION_MIDTERM_C31_AUTONOMOUS
    if(c->loss_active && (uint32_t)(now-c->loss_frame_ms)>=VISION_C31_ZERO_WAIT_MS)
        vc_trip(c,VC_FAULT_VISION);
#elif VISION_ROUTE_SWITCH
    if(c->output.recovering&&(uint32_t)(now-c->last_valid_ms)>VISION_RECOVERY_INITIAL_MS&&
       (!c->have_partial||(uint32_t)(now-c->last_partial_ms)>VISION_RECOVERY_PARTIAL_FRESH_MS||
        (c->have_partial!=2&&(uint32_t)(now-c->last_valid_ms)>VISION_RECOVERY_MAX_MS)))vc_trip(c,VC_FAULT_VISION);
#endif
    if ((uint32_t)(now - c->last_frame_ms) >= c->config.frame_timeout_ms) vc_trip(c, VC_FAULT_FRAME);
    if (remote && (uint32_t)(now - c->last_command_ms) >= c->config.command_timeout_ms) vc_trip(c, VC_FAULT_COMMAND);
    if (c->config.max_run_ms && (uint32_t)(now - c->armed_ms) >= c->config.max_run_ms) vc_trip(c, VC_FAULT_RUNTIME);
}

int vc_arm(vc_controller_t *c, uint32_t now, int hardware_confirmed)
{
    if (!hardware_confirmed || !c->configured || !c->config.calibration_confirmed ||
        c->config.cruise_duty <= 0 || c->config.duty_cap <= 0 ||
        c->output.state != VC_DISARMED || !c->output.observation_valid ||
        c->stable_frames < c->config.arm_frames ||
        (uint32_t)(now - c->last_frame_ms) >= c->config.frame_timeout_ms) return 0;
    c->output.state = VC_RUNNING;
    c->local_run = 0;
    c->last_command_ms = c->armed_ms = now;
    c->output.duty = c->output.steering = 0;
    c->have_partial=c->straight_tracking=0;
    c->loss_active=c->reacquire_frames=0;c->output.vision_state=VC_VISION_NORMAL;
    c->derivative = 0;
    return 1;
}

int vc_arm_local(vc_controller_t *c, uint32_t now, int hardware_confirmed)
{
    if (!vc_arm(c, now, hardware_confirmed)) return 0;
    c->local_run = 1;
    return 1;
}

void vc_heartbeat(vc_controller_t *c, uint32_t now)
{
    /* An expired heartbeat must never be resurrected by a late packet. */
    vc_poll(c, now);
    if (c->output.state == VC_RUNNING) c->last_command_ms = now;
}

#include "vision_route_control.h"

#if VISION_MIDTERM_C31_AUTONOMOUS
static void c31_zero_wait(vc_controller_t *c,unsigned reason,uint32_t capture,uint32_t now)
{
    if(c->output.state!=VC_RUNNING)return;
    if(!c->loss_active){c->loss_active=1;c->loss_frame_ms=capture;}
    if(reason){c->output.vision_state=(uint8_t)reason;c->reacquire_frames=0;}
    c->output.duty=0;c->output.speed_boost=0;
    c->output.paired_support=c->output.edge_support=0;c->output.recovery_drive=0;
    c->output.recovering=1;c->output.search_source=0;
    c->invalid_frames=1;c->derivative=0;c->straight_tracking=0;
    /* A delayed observation consumes, rather than renews, the original budget. */
    if((uint32_t)(now-c->loss_frame_ms)>=VISION_C31_ZERO_WAIT_MS)vc_trip(c,VC_FAULT_VISION);
}
#endif

void vc_observe(vc_controller_t *c, const vision_result_t *r,
                uint32_t sequence, uint32_t frame_ms, uint32_t now)
{
    float near_error = 0, far_error = 0, dt = 0, heading = 0;
    int have_far;
    vc_poll(c, now); /* Check old deadlines before accepting a delayed frame. */
    if (!c->configured) return;
    if (c->have_frame && sequence == c->last_sequence) return; /* Not a new observation. */
#if VISION_MIDTERM_C31_AUTONOMOUS || VISION_ROUTE_SWITCH
    /* Modular ordering is valid within half the uint32 domain, including wrap.
     * Old/ambiguous identities never refresh a lease or count as recovery. */
    if(c->have_frame && (uint32_t)(sequence-c->last_sequence)>=UINT32_C(0x80000000))return;
#endif
    if (c->have_frame && (uint32_t)(frame_ms - c->last_frame_ms) >= c->config.frame_timeout_ms) {
        c->stable_frames = 0;
        c->derivative = 0;
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_FRAME);
    }
    if (c->have_frame) {
        dt = (uint32_t)(frame_ms - c->last_frame_ms) * 0.001f;
        if (dt <= 0) {
            c->stable_frames = 0;
            c->output.observation_valid = 0;
            if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_FRAME);
            return;
        }
    }
    c->have_frame = 1;
    c->last_sequence = sequence;
    c->last_frame_ms = frame_ms;
    c->output.observation_valid = c->output.far_valid = 0;
    c->output.speed_boost=0;
    c->output.paired_support=c->output.edge_support=0;c->output.recovery_drive=0;
    c->output.lateral = c->output.heading = 0;
    c->output.near_center_x = c->output.far_center_x = 0;
    if ((uint32_t)(now - frame_ms) >= c->config.frame_timeout_ms || r->confidence > 100) {
        c->stable_frames=0;
        if(c->output.state==VC_RUNNING)vc_trip(c,VC_FAULT_VISION);
        return;
    }
    if (!r->valid || r->status != VISION_TRACK || r->confidence < c->config.min_coverage ||
        !control_band_error(r, &c->config, c->config.near_y, &near_error, &c->output.near_center_x)) {
        c->stable_frames = 0;
        c->straight_tracking=0;
#if VISION_MIDTERM_C31_AUTONOMOUS
        /* Local geometry can describe direction, but never grants motor power.
         * Keep the existing bounded steering command while awaiting full road. */
        c31_zero_wait(c,VC_VISION_NO_LANE,frame_ms,now);
        return;
#elif VISION_ROUTE_SWITCH
        /* Partial CURRENT road can guide a bounded low-speed recovery.
         * No current boundary evidence means zero motor demand; never power
         * through a junction or outside view using only an old turn. */
        {float current_target=0;int strong=0,paired=0,carry=0;
        int current=current_partial_direction(r,&c->config,&current_target,&strong,&paired);
        float near_target=0;
        int near_current=current_near_corridor(r,&c->config,&near_target);
        if(near_current){current=strong=1;current_target=near_target;}
        if(c->output.state==VC_RUNNING && current &&
           ((uint32_t)(now-c->last_valid_ms)<=VISION_RECOVERY_INITIAL_MS||
            (current&&strong&&((uint32_t)(now-c->last_valid_ms)<=VISION_RECOVERY_MAX_MS||near_current)))) {
            float step=c->config.steer_rate*dt;
            float chosen=current_target;
            float search=clamp(chosen*1.25f,-1,1);
            /* Current white-path steering is continuous across zero and the
             * old .12 threshold. Do not jump from .119 to a .60 command. */
            /* A short modeled one-edge strip near center cannot establish a
             * new straight. Retain some recent curvature until that CURRENT
             * strip gives a clear new direction. Paired edges can unwind it. */
            if(current&&strong&&!near_current&&!paired&&!r->junction_partial&&absolute(chosen)<.20f&&absolute(c->recovery_steer)>.35f){
                search=clamp(search+c->recovery_steer*.85f*(1.0f-absolute(chosen)/.20f),-1,1);carry=1;}
            if(current&&strong){c->have_partial=(uint8_t)(near_current?2:1);c->last_partial_ms=now;}
            if(current&&search*c->output.steering<0)step*=2.0f;
            else if(current&&absolute(search)<absolute(c->output.steering))step*=1.5f;
            c->output.steering+=clamp(search-c->output.steering,-step,step);
            /* A current four-row entry plus broad white forward corridor is
             * stronger than a detached partial edge. Keep enough low-speed
             * demand to pass that transition, below clear-road/corner speed. */
            c->output.duty=clamp(c->output.duty+c->config.duty_rise_per_s*dt,0,
                c->config.cruise_duty*(r->junction_partial?.60f:.45f));
            c->output.recovery_drive=1;
            c->output.recovering=1;c->invalid_frames=1;c->derivative=0;
            c->output.search_source=(uint8_t)(near_current?4:(carry?3:2));
            return;
        }}
        if(c->output.state==VC_RUNNING && c->invalid_frames<2 &&
           (uint32_t)(now-c->last_valid_ms)<=40U) {
            c->output.recovering=1;c->output.search_source=0;
            ++c->invalid_frames;c->output.duty=0;c->derivative=0;
            return; /* No motor drive on missing evidence, no guessed steering. */
        }
#endif
#if !VISION_MIDTERM_C31_AUTONOMOUS
        /* C31 already returns after c31_zero_wait above. Compile this legacy
         * fallback only for variants that can reach it (MSVC /W4 /WX). */
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_VISION);
        return;
#endif
    }
#if !VISION_ROUTE_SWITCH
    if (absolute(near_error) > c->config.max_lateral) {
        c->stable_frames = 0;
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_OFFSET);
        return;
    }
#endif
    have_far = control_band_error(r, &c->config, c->config.far_y, &far_error, &c->output.far_center_x);
#if VISION_ROUTE_SWITCH
    have_far=reference_preview_error(r,&c->config,&far_error,&c->output.far_center_x)||have_far;
#endif
#if VISION_MIDTERM_C31_AUTONOMOUS
    if(c->output.state==VC_RUNNING) {
        c->output.observation_valid=1;c->output.far_valid=(uint8_t)have_far;
        if(!have_far){c31_zero_wait(c,VC_VISION_FAR_LOSS,frame_ms,now);return;}
        if(c->loss_active) {
            if(++c->reacquire_frames<VISION_C31_REACQUIRE_FRAMES) {
                c31_zero_wait(c,0,frame_ms,now);return;
            }
            c->loss_active=0;c->reacquire_frames=0;c->output.vision_state=VC_VISION_NORMAL;
        }
    }
#endif
    if (have_far) heading = far_error - near_error;
    if (!finite_value(heading)) { vc_trip(c, VC_FAULT_NUMERIC); return; }
    if(c->invalid_frames){c->previous_lateral=near_error;c->derivative=0;}
    c->invalid_frames=0;c->last_valid_ms=frame_ms;
    c->output.recovering=0;
    c->output.search_source=0;
    c->output.observation_valid = 1;
    c->output.far_valid = (uint8_t)have_far;
    c->output.lateral = near_error;
    c->output.heading = heading;
    if (c->stable_frames < 255) ++c->stable_frames;
    if (c->output.state == VC_RUNNING) {
        float target, target_duty, derivative;
        /* Same-millisecond/reordered timestamps are not valid control timing. */
        if (dt <= 0 || dt >= c->config.frame_timeout_ms * 0.001f) { vc_trip(c, VC_FAULT_FRAME); return; }
        derivative = (near_error - c->previous_lateral) / dt;
        c->derivative += dt / (c->config.derivative_filter_s + dt) * (derivative - c->derivative);
#if VISION_ROUTE_SWITCH
        /* Preview the bend before the near point has moved. Keep the calibrated
         * far offset itself; subtracting near can cancel a useful turn command. */
        /* Keep raw lateral telemetry, but saturate its steering contribution.
         * A measured/clipped-edge lane is not discarded at an arbitrary offset
         * before the vehicle has had time to turn. Missing evidence still zeros. */
        {float preview_scale=1.0f;
        /* When the near road center has crossed to the other side, prioritize
         * centering instead of holding the previous bend from far preview.
         * Continuous taper avoids switching abruptly at a tiny center error. */
        if(have_far&&near_error*far_error<0)
            preview_scale=clamp((.22f-absolute(near_error))/.18f,.05f,1.0f);
        target=c->config.kp*clamp(near_error,-c->config.max_lateral,c->config.max_lateral)+
            c->config.kd*c->derivative+
            (have_far?preview_scale*c->config.heading_gain*clamp(far_error,-c->config.max_lateral,c->config.max_lateral):0);}
#else
        target = c->config.kp * near_error + c->config.kd * c->derivative + c->config.heading_gain * heading;
#endif
        if (!finite_value(target)) { vc_trip(c, VC_FAULT_NUMERIC); return; }
        target = clamp(target, -1, 1);
#if VISION_ROUTE_SWITCH
        c->recovery_steer=target;
#endif
        {float rate=c->config.steer_rate;
#if VISION_ROUTE_SWITCH
        /* Release an excessive bend command faster than building it up.
         * A reversal still passes smoothly through center with bounded rate. */
        if(absolute(target)<absolute(c->output.steering)||target*c->output.steering<0)rate*=1.5f;
        else if(absolute(near_error)>.35f&&target*near_error>0)rate*=1.5f;
#endif
        c->output.steering += clamp(target - c->output.steering,-rate*dt,rate*dt);}
        if (!mol_step(&c->config.motor, near_error, heading, c->output.steering,
                      r->confidence * .01f, have_far, c->config.cruise_duty,
                      c->config.duty_cap, c->config.duty_rise_per_s,
                      c->output.duty, dt, &target_duty)) {
            vc_trip(c, VC_FAULT_NUMERIC); return;
        }
#if VISION_ROUTE_SWITCH
        target_duty=corner_duty(c,r,near_error,far_error,have_far,target,
                               c->output.duty,dt,target_duty);
#endif
        c->output.duty = target_duty;
#if VISION_ROUTE_SWITCH
        c->output.speed_boost=(uint8_t)(have_far&&vj_paired_support(r));
        c->output.paired_support=have_far?vj_paired_weight(r):0;
        c->output.edge_support=have_far?current_edge_weight(r):0;
        /* Labels describe CURRENT support; they do not veto ring/cross speed.
         * The output layer blends paired/curve scales continuously using
         * actual/requested steering and lateral margin. Missing support still
         * falls back; coverage metadata alone cannot force a scale jump. */
        if(c->output.speed_boost&&absolute(c->output.steering)>=.45f)
            c->output.speed_boost=2;
        else if(!c->output.speed_boost&&have_far&&current_edge_weight(r)>=1.0f)
            c->output.speed_boost=3;
#endif
    } else c->output.duty = c->output.steering = 0;
    c->previous_lateral = near_error;
}
