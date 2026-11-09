#ifndef SMARTCAR_VISION_METRIC_IO_H
#define SMARTCAR_VISION_METRIC_IO_H
#include "vision_planner_shadow.h"
#include "vision_speed.h"
#include "../../tools/speed_planning/vision_adapter.h"
#define VM_HISTORY_CAPACITY 32U
typedef enum { VM_OK=0,VM_DUPLICATE,VM_WAIT,VM_BAD_CONFIG,VM_BAD_SOURCE,
               VM_CONFLICT,VM_OLD,VM_EXPIRED,VM_NO_BRACKET } vm_status_t;
typedef struct {
    uint32_t sequence,ms;
    int32_t total_left,total_right;
    int8_t sign_left,sign_right;
    uint8_t valid;
} vm_encoder_t;
typedef struct {
    uint32_t revision,max_age_ms,interpolation_gap_ms; /* Shared calibration bundle revision. */
    float ticks_per_metre_left,ticks_per_metre_right;
    int8_t sign_left,sign_right;
} vm_odom_config_t;
typedef struct {
    uint32_t ms,sequence;
    uint64_t travel_left,travel_right;
    uint8_t continuous;
} vm_history_t;
typedef struct {
    vm_odom_config_t config;
    vm_encoder_t last;
    vm_history_t history[VM_HISTORY_CAPACITY];
    uint64_t travel_left,travel_right;
    int32_t delta_left,delta_right;
    uint32_t elapsed_ms,sequence_span,revision;
    unsigned head,count;
    uint8_t configured;
} vm_odom_t;
typedef struct {
    uint32_t sequence,ms,elapsed_ms,sequence_span,revision;
    int32_t delta_left,delta_right;
    float odometer_m,speed_m_s,left_speed_m_s,right_speed_m_s;
    uint8_t velocity_valid;
} vm_motion_t;
/* Reads only an already captured native snapshot. ENCODER3 is left,
 * ENCODER4 is right. have_sample is explicit: sequence0 is legal at wrap. */
int vm_encoder_from_speed(const vd_speed_info_t *source,int have_sample,vm_encoder_t *out);
int vm_odom_init(vm_odom_t *state,const vm_odom_config_t *config);
vm_status_t vm_odom_push(vm_odom_t *state,const vm_encoder_t *sample);
vm_status_t vm_odom_current(const vm_odom_t *state,uint32_t now_ms,vm_motion_t *out);
/* Exact sample or bracketed linear interpolation only, never extrapolation.
 * A future capture returns WAIT; a missed sample marks a discontinuous span.
 * Interpolation is a model, not physically measured inter-sample motion. */
vm_status_t vm_odom_at_capture(const vm_odom_t *state,uint32_t capture_ms,
                               uint32_t now_ms,float *odometer_m);

typedef struct {
    float homography[9]; /* Pixels -> rear-axle ground coordinates, metres. */
    float min_x_m,max_x_m,min_y_m,max_y_m,max_step_m;
    uint8_t near_rows[SP_CAPACITY],full_rows[SP_CAPACITY];
    unsigned near_count,full_count,min_confidence;
    uint32_t revision,max_age_ms;
} vm_vision_config_t;
typedef struct {sp_geometry_history_t near_history,full_history;uint32_t revision;} vm_vision_t;
typedef struct {
    uint32_t sequence,capture_ms,config_revision;
    sp_preview_t near_preview,full_preview;
    /* near/full are independent observations; domain_valid remains the
     * legacy near-domain admission bit, not an OR of both path domains. */
    uint8_t near_valid,far_valid,domain_valid;
} vm_frame_t;
/* Reuses the existing strict paired-row adapter. No second detector, no
 * inferred geometry approval. Config/rows/domain are supplied by the caller. */
int vm_vision_frame(vm_vision_t *state,const vm_vision_config_t *config,
                     const vision_result_t *source,uint32_t sequence,uint32_t capture_ms,
                     uint32_t now_ms,vm_frame_t *out);
int vm_vision_config_valid(const vm_vision_config_t *config);
void vm_vision_init(vm_vision_t *state); /* New geometry session only. */
/* Builds the actual vps input from an accepted frame + cumulative counts.
 * If a NEW usable frame lacks an odometer bracket, return WAIT and let the caller
 * keep processing its PREVIOUS accepted frame until the next sample arrives.
 * An explicitly invalid near frame is forwarded immediately. Full evidence
 * may carry an available capture bracket, but never delays near-loss delivery.
 * Odometry, vision and identity must name the same immutable calibration
 * bundle revision. Never send a partial result to vps_arm/tick on failure. */
vm_status_t vm_shadow_input(const vm_odom_t *odometry,const vm_frame_t *frame,int new_frame,
                             vp_stamp_t identity,uint32_t now_ms,float requested_speed_m_s,
                             int external_admitted,vps_input_t *out);
#endif
