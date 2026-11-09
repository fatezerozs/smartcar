#ifndef SMARTCAR_HOST_PLANNING_PIPELINE_H
#define SMARTCAR_HOST_PLANNING_PIPELINE_H
/* Host/reference composition, no GPIO, PWM, UART, RTOS or C26 start manager.
 * Production wheel_speed.c is linked unchanged; no duplicate PI algorithm. */
#include "speed_planner.h"
#include "wheel_speed.h"
#define SP_TELEMETRY_CAPACITY 8U
typedef struct {
    sp_limits_t limits;sp_policy_t policy;
    float ticks_per_metre_left,ticks_per_metre_right,feedforward_full_speed,planning_brake;
    uint16_t kp_q8,ki_q8;
} sp_pipeline_config_t;
typedef struct {
    sp_input_t observation;
    int32_t measured_left_ticks10ms,measured_right_ticks10ms;
} sp_pipeline_input_t;
typedef struct {
    uint64_t session;
    uint32_t generation,request,sample_sequence,sample_ms,control_ms;
    sp_trajectory_t trajectory;ws_pi_t pi[2];
    int32_t left_target,right_target;
    uint16_t left_pwm,right_pwm;
    float cap;uint8_t ready;
} sp_pipeline_candidate_t;
typedef struct {
    uint32_t control_ms,frame_sequence,capture_ms,encoder_sequence,generation,commit_sequence;
    sp_mode_t mode;uint32_t faults;
    float speed_reference,accel_reference,measured_speed,far_distance,current_cap;
    int32_t left_target,right_target,left_integral,right_integral;
    uint16_t left_pwm,right_pwm;
    uint8_t near_valid,far_valid,raw_near_valid,raw_far_valid,brake_override;
} sp_telemetry_t;
/* Fixed-size single-owner host queue. Production SPSC/RTOS ownership and IRQ
 * synchronization are explicitly NOT supplied by this struct. Drop newest. */
typedef struct {
    sp_telemetry_t data[SP_TELEMETRY_CAPACITY];unsigned head,count,high_water;
    uint32_t dropped;
} sp_telemetry_queue_t;
typedef struct {
    sp_pipeline_config_t config;sp_supervisor_t supervisor;
    sp_preview_t near_preview,full_preview;sp_trajectory_t trajectory;ws_pi_t pi[2];
    uint64_t session;
    uint32_t request,latest_sample,consumed_sample,consumed_ms,last_control_ms,commit_sequence;
    uint16_t left_pwm,right_pwm;int32_t left_target,right_target;
    float near_odometer,full_odometer,current_cap;
    uint8_t configured,have_near,have_full,near_valid,far_valid;
} sp_pipeline_t;
/* Rebase still-fresh metric geometry using travelled distance since capture.
 * No new image identity is minted. Exhausted preview is rejected. */
/* Caller supplies a nonzero session identity, unique while old candidates can
 * exist. Init is a new offline session, not a runtime fault-clear API. */
int sp_pipeline_init(sp_pipeline_t *p,const sp_pipeline_config_t *config,uint64_t session);
int sp_pipeline_arm(sp_pipeline_t *p,const sp_pipeline_input_t *input,
                    const sp_preview_t *near_preview,const sp_preview_t *full_preview);
int sp_pipeline_prepare(sp_pipeline_t *p,const sp_pipeline_input_t *input,
                        const sp_preview_t *near_preview,const sp_preview_t *full_preview,
                        sp_pipeline_candidate_t *candidate);
int sp_pipeline_commit(sp_pipeline_t *p,const sp_pipeline_candidate_t *candidate);
void sp_pipeline_snapshot(const sp_pipeline_t *p,const sp_pipeline_input_t *input,sp_telemetry_t *out);
int sp_telemetry_push(sp_telemetry_queue_t *q,const sp_telemetry_t *sample);
int sp_telemetry_pop(sp_telemetry_queue_t *q,sp_telemetry_t *sample);
#endif
