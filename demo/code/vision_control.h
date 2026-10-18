#ifndef VISION_CONTROL_H
#define VISION_CONTROL_H
#include <stdint.h>
#include "vision_midterm.h"
#include "midterm_open_loop.h"

typedef enum { VC_DISARMED = 0, VC_RUNNING, VC_FAULT } vc_state_t;
enum {
    VC_FAULT_CONFIG = 1U, VC_FAULT_VISION = 2U, VC_FAULT_FRAME = 4U,
    VC_FAULT_COMMAND = 8U, VC_FAULT_NUMERIC = 16U,
    VC_FAULT_OFFSET = 32U, VC_FAULT_RUNTIME = 64U, VC_FAULT_EXTERNAL = 128U
};
typedef struct {
    mol_config_t motor; /* 可单独标定的中期 PWM 降额模型，不含编码器反馈。 */
    float near_y, far_y, band_half_height, axis_near_x, axis_far_x;
    float kp, kd, heading_gain, derivative_filter_s;
    float steer_rate, duty_rise_per_s, duty_cap, cruise_duty, max_lateral;
    uint32_t frame_timeout_ms, command_timeout_ms, max_run_ms;
    uint8_t arm_frames, min_coverage, min_band_rows, calibration_confirmed;
} vc_config_t;
typedef struct {
    vc_state_t state;
    uint32_t faults;
    float lateral, heading, steering, duty;
    uint8_t observation_valid, far_valid;
} vc_output_t;
typedef struct {
    vc_config_t config;
    vc_output_t output;
    uint32_t last_frame_ms, last_sequence, last_command_ms, armed_ms;
    float previous_lateral, derivative;
    uint8_t configured, have_frame, stable_frames;
} vc_controller_t;

void vc_default_config(vc_config_t *config);
void vc_init(vc_controller_t *control, const vc_config_t *config);
void vc_observe(vc_controller_t *control, const vision_result_t *result,
                uint32_t sequence, uint32_t frame_ms, uint32_t now_ms);
void vc_poll(vc_controller_t *control, uint32_t now_ms);
int vc_arm(vc_controller_t *control, uint32_t now_ms, int hardware_confirmed);
void vc_heartbeat(vc_controller_t *control, uint32_t now_ms);
void vc_stop(vc_controller_t *control);
void vc_trip(vc_controller_t *control, uint32_t reason);
void vc_clear_fault(vc_controller_t *control);

#endif
