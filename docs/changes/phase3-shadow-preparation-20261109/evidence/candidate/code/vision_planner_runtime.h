#ifndef SMARTCAR_VISION_PLANNER_RUNTIME_H
#define SMARTCAR_VISION_PLANNER_RUNTIME_H
#include "vision_metric_io.h"
#if VISION_PLANNER_RUNTIME && (!VISION_ROUTE_SWITCH || !VISION_ENCODER_ENABLE || VISION_MIDTERM_AUTONOMOUS)
#error "Planner runtime currently requires the C26 route with encoder, not autonomous."
#endif
typedef enum { VDR_OFF=0,VDR_SHADOW=1,VDR_CONTROL=2 } vdr_mode_t;
typedef struct {
    vps_config_t planning;
    vm_odom_config_t odometry;
    vm_vision_config_t vision;
    float requested_speed_m_s;
    uint16_t pwm_cap,feedforward_right,feedforward_left;
} vdr_config_t;
typedef struct {
    vdr_mode_t mode;
    uint32_t polls,plans,frame_waits,revision,source_version;
    uint32_t observed_frames,near_geometry_frames,full_geometry_frames;
    vp_status_t planning_status;
    sp_mode_t supervisor_mode;
    uint32_t output_epoch,frame_sequence,capture_ms,encoder_sequence,encoder_ms;
    float measured_speed_m_s,planned_speed_m_s,cap_m_s;
    uint16_t target_right_ticks,target_left_ticks;
    uint8_t near_valid,far_valid,result_valid;
    uint8_t configured,session_active;
} vdr_info_t;
void vd_planner_runtime_init(void);
/* Explicit stopped-only RAM configuration. No saved/default calibration,
 * implicit arm, runtime serial command or automatic target-source fallback. */
int vision_drive_planner_configure(vdr_mode_t mode,const vdr_config_t *config);
void vd_planner_runtime_observe(const vision_result_t *result,uint32_t sequence,uint32_t capture,uint32_t now);
void vd_planner_runtime_poll(uint32_t now);
void vision_drive_planner_info(vdr_info_t *out);
/* Prepare outside the output mask; final commit only checks integer identity
 * and the remaining time budget. No geometry/supervisor/float work in commit. */
typedef struct {
    uint32_t epoch,revision,source_version,sample_sequence,prepared_ms,valid_for_ms;
    uint8_t valid;
} vdr_output_t;
void vd_planner_runtime_prepare(uint32_t epoch,vdr_output_t *token);
int vd_planner_runtime_output(const vdr_output_t *token,uint16_t *right,uint16_t *left);
uint16_t vd_planner_runtime_cap(void);
#endif
