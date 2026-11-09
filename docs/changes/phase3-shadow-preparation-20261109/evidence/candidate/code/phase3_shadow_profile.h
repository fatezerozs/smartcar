/* Generated from source-pinned Phase2 records. Shadow only. */
#ifndef PHASE3_SHADOW_PROFILE_H
#define PHASE3_SHADOW_PROFILE_H
#if !VISION_PLANNER_RUNTIME || VISION_PLANNER_ACTIVE
#error "Measured candidate profile supports shadow only."
#endif
#define PHASE3_SHADOW_PROFILE_ID "C26-SHADOW-FA161BDE"
static const vdr_config_t phase3_shadow_configuration = {
  .planning = {.planner = {.revision=4195752926U,
    .limits={SP_SIMULATION,1.15901308f,0.351139024f,0.2f,0.5f,0.8f,2.0f,3.0f,0.1f,0.05f,0.0001f,0.02f},
    .ticks_per_metre_left=11394.0f,
    .ticks_per_metre_right=11389.0f,
    .rear_track_m=0.1529f,
    .max_left_ticks_10ms=132U,
    .max_right_ticks_10ms=132U,
    .frame_max_age_ms=100U,
    .encoder_max_age_ms=30U,
    .candidate_max_age_ms=20U,
    .compensate=0U,
    .geometry_calibrated=0U,
  },.policy={100U,30U,300U,600U,2000U,5U,3U,3U,0.24f,0.02f}},
  .odometry={4195752926U,30U,20U,11394.0f,11389.0f,-1,1},
  .vision={.homography={-0.0974828547f,0.00174818181f,10.6272361f,-0.0199817907f,0.0219550391f,-11.4547422f,0.0119041753f,-0.256175807f,1.0f},
    .min_x_m=-0.1f,
    .max_x_m=0.1f,
    .min_y_m=0.697f,
    .max_y_m=1.297f,
    .max_step_m=0.05f,.near_rows={70,65,61,58,54},.full_rows={70,65,61,58,54,52,49},
    .near_count=5U,.full_count=7U,.min_confidence=25U,.revision=4195752926U,.max_age_ms=100U},
  .requested_speed_m_s=0.351139024f,.pwm_cap=250U,.feedforward_right=0U,.feedforward_left=0U
};
#endif
