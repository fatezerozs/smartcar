#ifndef SMARTCAR_VISION_PLANNER_SHADOW_H
#define SMARTCAR_VISION_PLANNER_SHADOW_H
#include "vision_planner.h"
/* Single serialized foreground owner, shadow session only. No C26 admission,
 * PI, output lease, interrupt or device access. init is an explicit NEW session,
 * not an automatic recovery/clear operation for a running production vehicle. */
typedef struct {vp_config_t planner;sp_policy_t policy;} vps_config_t;
typedef struct {
    sp_input_t observation;
    vp_stamp_t identity; /* control_revision is owned by this executor. */
    sp_preview_t near_preview,full_preview; /* Capture-aligned metric horizons. */
    uint32_t valid;
    float requested_speed_m_s;
    uint8_t external_admitted; /* Caller assertion, never physical authorization. */
} vps_input_t;
typedef enum { VPS_INACTIVE=0,VPS_WAIT,VPS_READY,VPS_ZERO,VPS_FAULT } vps_status_t;
typedef struct {
    vp_candidate_t candidate;
    uint32_t revision,supervisor_generation;
    sp_mode_t supervisor_mode;
    vps_status_t status;
} vps_result_t;
typedef struct {
    vps_config_t config;
    sp_supervisor_t supervisor;
    sp_trajectory_t trajectory;
    sp_preview_t near_preview,full_preview;
    sp_input_t latest_observation;
    vp_stamp_t identity;
    float near_odometer,full_odometer;
    uint32_t revision,control_revision,last_control_ms,consumed_encoder;
    uint8_t configured,have_near,have_full,seed_required;
} vps_state_t;
int vps_init(vps_state_t *state,vp_mode_t mode,const vps_config_t *config);
int vps_arm(vps_state_t *state,const vps_input_t *input);
vps_status_t vps_tick(vps_state_t *state,const vps_input_t *input,vps_result_t *out);
/* A result describes already-adopted SHADOW trajectory state, never PWM.
 * Exact identity, state revision, ages and supervisor deadlines are checked.
 * Calling this does not advance live state; tick must still run every cycle. */
int vps_result_matches(const vps_state_t *state,const vps_result_t *result,
                         const vp_stamp_t *current,uint32_t now_ms);
#endif
