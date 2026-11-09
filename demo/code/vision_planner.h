#ifndef SMARTCAR_VISION_PLANNER_H
#define SMARTCAR_VISION_PLANNER_H
/* Portable MCU boundary around the SINGLE shared planning kernel.
 * Shadow output is diagnostic data. No function grants a motor lease, writes
 * PWM, changes PI state or supplies production calibration defaults. */
#include "../../tools/speed_planning/speed_planner.h"

#include "vision_planner_build.h"

typedef enum { VP_DISABLED=0,VP_SHADOW=1 } vp_mode_t;
typedef enum { VP_ZERO_NOW=0,VP_TRACK=1,VP_CONTROLLED_STOP=2 } vp_action_t;
typedef enum { VP_ZERO=0,VP_READY,VP_OFF,VP_BAD_CONFIG,VP_BAD_INPUT,
               VP_EXPIRED,VP_BAD_GEOMETRY,VP_INFEASIBLE,VP_TARGET_RANGE } vp_status_t;
enum { VP_VALID_NEAR=1U,VP_VALID_ENCODER=2U,VP_VALID_METRIC_GEOMETRY=4U,
       VP_VALID_GROUND_DOMAIN=8U,VP_VALID_ALL=15U };
enum { VP_FULL_PREVIEW=0,VP_NEAR_PREVIEW=1 };

/* Integer identities travel with every candidate. Sequence zero is legal at
 * wrap; config_revision must be nonzero. Metadata is not proof of approval. */
typedef struct {
    uint32_t output_epoch,request_revision,config_revision,frame_seq,encoder_seq;
    uint32_t capture_ms,encoder_ms;
    uint32_t control_revision; /* Caller advances after adopting trajectory state. */
} vp_stamp_t;
typedef struct {
    uint32_t revision;
    sp_limits_t limits;
    float ticks_per_metre_left,ticks_per_metre_right,rear_track_m;
    uint16_t max_left_ticks_10ms,max_right_ticks_10ms;
    uint32_t frame_max_age_ms,encoder_max_age_ms,candidate_max_age_ms;
    uint8_t compensate,geometry_calibrated;
} vp_config_t;
typedef struct {
    vp_stamp_t stamp;
    uint32_t now_ms,valid;
    vp_action_t action;
    uint8_t near_only,preview_scope;
    sp_preview_t preview;
    sp_trajectory_t trajectory;
    float measured_speed_m_s,requested_speed_m_s,dt_s;
} vp_input_t;
typedef struct {
    vp_stamp_t stamp;
    vp_status_t status;
    vp_action_t action;
    sp_provenance_t provenance;
    uint32_t prepared_ms,frame_max_age_ms,encoder_max_age_ms,candidate_max_age_ms;
    sp_trajectory_t trajectory;
    float cap_m_s,left_ticks_s,right_ticks_s;
    uint16_t left_ticks_10ms,right_ticks_10ms;
    uint8_t quantized;
} vp_candidate_t;

/* Caller supplies a coherent immutable snapshot; no shared state is touched.
 * ZERO_NOW bypasses invalid/missing parameters and always clears all targets.
 * Failed preparation also clears all targets; never reuse the previous result.
 * TRACK uses the metric horizon; CONTROLLED_STOP shapes target zero while
 * preserving the current geometric safety cap (cap outranks normal jerk).
 * Near-invalid/missing encoder is always immediate zero, never coast authority.
 * Geometry origin/ground-domain validation and supervisor admission belong to
 * the upstream owner. This API does not infer them from a provenance label. */
vp_status_t vp_prepare(vp_mode_t mode,const vp_config_t *config,
                       const vp_input_t *input,vp_candidate_t *out);
int vp_config_valid(const vp_config_t *config);
/* Diagnostic publication check only: recheck exact identities and all ages.
 * Does NOT authorize motors, enforce a supervisor deadline or mutate state.
 * Consumer must discard on false; actual control still uses the old path. */
int vp_candidate_matches(vp_mode_t mode,const vp_candidate_t *candidate,
                          const vp_stamp_t *current,uint32_t now_ms);
#endif
