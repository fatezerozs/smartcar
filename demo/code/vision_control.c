#include "vision_control.h"
#include "vision_profile.h"
#include <float.h>
#include <string.h>

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

static int configuration_valid(const vc_config_t *c)
{
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
        c->max_run_ms >= c->frame_timeout_ms && c->max_run_ms <= 60000 &&
        c->arm_frames >= 2 && c->min_coverage >= 1 && c->min_coverage <= 100 &&
        c->min_band_rows >= 1 && c->min_band_rows <= 20 && c->calibration_confirmed <= 1;
}

void vc_trip(vc_controller_t *c, uint32_t reason)
{
    c->output.state = VC_FAULT;
    c->output.faults |= reason;
    c->output.duty = c->output.steering = 0;
    c->stable_frames = 0;
    c->derivative = 0;
}

void vc_init(vc_controller_t *c, const vc_config_t *config)
{
    vc_config_t saved = *config; /* Also safe when clearing with &c->config. */
    memset(c, 0, sizeof(*c));
    c->config = saved;
    c->configured = (uint8_t)configuration_valid(&saved);
    if (!c->configured) vc_trip(c, VC_FAULT_CONFIG);
}

void vc_stop(vc_controller_t *c)
{
    if (c->output.state != VC_FAULT) c->output.state = VC_DISARMED;
    c->output.duty = c->output.steering = 0;
    c->stable_frames = 0;
    c->derivative = 0;
}

void vc_clear_fault(vc_controller_t *c)
{
    if (c->output.state == VC_RUNNING) return;
    vc_init(c, &c->config);
}

void vc_poll(vc_controller_t *c, uint32_t now)
{
    if (c->output.state != VC_RUNNING) return;
    if ((uint32_t)(now - c->last_frame_ms) >= c->config.frame_timeout_ms) vc_trip(c, VC_FAULT_FRAME);
    if ((uint32_t)(now - c->last_command_ms) >= c->config.command_timeout_ms) vc_trip(c, VC_FAULT_COMMAND);
    if ((uint32_t)(now - c->armed_ms) >= c->config.max_run_ms) vc_trip(c, VC_FAULT_RUNTIME);
}

int vc_arm(vc_controller_t *c, uint32_t now, int hardware_confirmed)
{
    if (!hardware_confirmed || !c->configured || !c->config.calibration_confirmed ||
        c->config.cruise_duty <= 0 || c->config.duty_cap <= 0 ||
        c->output.state != VC_DISARMED || !c->output.observation_valid ||
        c->stable_frames < c->config.arm_frames ||
        (uint32_t)(now - c->last_frame_ms) >= c->config.frame_timeout_ms) return 0;
    c->output.state = VC_RUNNING;
    c->last_command_ms = c->armed_ms = now;
    c->output.duty = c->output.steering = 0;
    c->derivative = 0;
    return 1;
}

void vc_heartbeat(vc_controller_t *c, uint32_t now)
{
    /* An expired heartbeat must never be resurrected by a late packet. */
    vc_poll(c, now);
    if (c->output.state == VC_RUNNING) c->last_command_ms = now;
}

static int band_error(const vision_result_t *r, const vc_config_t *c, float band_y, float *error)
{
    float sum = 0;
    int n = 0, y;
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y) {
        float yf = (float)y / (VISION_HEIGHT - 1), axis, half_width, center;
        if (absolute(yf - band_y) > c->band_half_height || r->row_flags[y] != 3) continue;
        if (r->left[y] >= r->right[y] || r->right[y] >= VISION_WIDTH ||
            r->right[y] - r->left[y] + 1 < VISION_MIN_WIDTH) continue;
        center = (r->left[y] + r->right[y]) * 0.5f;
        if (r->center[y] >= VISION_WIDTH || absolute(r->center[y] - center) > 1) continue;
        axis = c->axis_far_x + (c->axis_near_x - c->axis_far_x) *
               (yf - c->far_y) / (c->near_y - c->far_y);
        half_width = (r->right[y] - r->left[y] + 1) * 0.5f;
        sum += (center - axis * (VISION_WIDTH - 1)) / half_width;
        ++n;
    }
    if (n < c->min_band_rows) return 0;
    *error = sum / n;
    return finite_value(*error);
}

void vc_observe(vc_controller_t *c, const vision_result_t *r,
                uint32_t sequence, uint32_t frame_ms, uint32_t now)
{
    float near_error = 0, far_error = 0, dt = 0, heading = 0;
    int have_far;
    vc_poll(c, now); /* Check old deadlines before accepting a delayed frame. */
    if (!c->configured) return;
    if (c->have_frame && sequence == c->last_sequence) return; /* Not a new observation. */
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
    c->output.lateral = c->output.heading = 0;
    if ((uint32_t)(now - frame_ms) >= c->config.frame_timeout_ms || !r->valid ||
        r->status != VISION_TRACK || r->confidence > 100 || r->confidence < c->config.min_coverage ||
        !band_error(r, &c->config, c->config.near_y, &near_error)) {
        c->stable_frames = 0;
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_VISION);
        return;
    }
    if (absolute(near_error) > c->config.max_lateral) {
        c->stable_frames = 0;
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_OFFSET);
        return;
    }
    have_far = band_error(r, &c->config, c->config.far_y, &far_error);
    if (have_far) heading = far_error - near_error;
    if (!finite_value(heading)) { vc_trip(c, VC_FAULT_NUMERIC); return; }
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
        target = c->config.kp * near_error + c->config.kd * c->derivative + c->config.heading_gain * heading;
        if (!finite_value(target)) { vc_trip(c, VC_FAULT_NUMERIC); return; }
        target = clamp(target, -1, 1);
        c->output.steering += clamp(target - c->output.steering,
                                   -c->config.steer_rate * dt, c->config.steer_rate * dt);
        if (!mol_step(&c->config.motor, near_error, heading, c->output.steering,
                      r->confidence * .01f, have_far, c->config.cruise_duty,
                      c->config.duty_cap, c->config.duty_rise_per_s,
                      c->output.duty, dt, &target_duty)) {
            vc_trip(c, VC_FAULT_NUMERIC); return;
        }
        c->output.duty = target_duty;
    } else c->output.duty = c->output.steering = 0;
    c->previous_lateral = near_error;
}
