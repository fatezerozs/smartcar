#ifndef SMARTCAR_HOST_SPEED_PLANNER_H
#define SMARTCAR_HOST_SPEED_PLANNER_H
/* Phase 1/2 HOST prototype. No PWM, authorization or MCU integration.
 * All physical parameters are caller-supplied; there are no production defaults. */
#include <stdint.h>
#define SP_CAPACITY 16U
typedef enum { SP_NORMALIZED=0, SP_METRES=1 } sp_units_t;
typedef enum { SP_SIMULATION=0, SP_PROJECT_CALIBRATION=1 } sp_provenance_t;
typedef struct { float x,y; } sp_point_t; /* x right, y forward */
typedef struct {
    uint32_t sequence,capture_ms;
    unsigned count;
    sp_units_t units;
    float distance[SP_CAPACITY],curvature[SP_CAPACITY];
} sp_preview_t;
typedef struct {
    uint32_t sequence,capture_ms;
    unsigned count;
    sp_units_t units;
    sp_point_t points[SP_CAPACITY];
    uint8_t have_frame;
} sp_geometry_history_t;
/* Point order is near -> far. Median-of-three suppresses a single interior
 * lateral outlier. max_step is per-frame lateral continuity in the input units.
 * Rejected/duplicate/old/future observations never update history. */
int sp_geometry(sp_geometry_history_t *history,const sp_point_t *points,unsigned count,
                sp_units_t units,uint32_t sequence,uint32_t capture_ms,uint32_t now_ms,
                uint32_t max_age_ms,float max_step,sp_preview_t *out);
typedef struct {
    sp_provenance_t provenance;
    float maximum_speed,near_cap,lateral_accel,accel,brake,jerk_accel,jerk_brake;
    float latency_s,distance_margin,curvature_epsilon,max_dt_s;
} sp_limits_t; /* SI: m, s, m/s, m/s^2, m/s^3, 1/m */
enum { SP_LIMIT_MAX=1, SP_LIMIT_CURVE=2, SP_LIMIT_HORIZON=4,
       SP_LIMIT_NEAR=8, SP_LIMIT_FORWARD=16, SP_LIMIT_BACKWARD=32 };
typedef struct { unsigned count;float current_cap,speed[SP_CAPACITY];uint32_t reason[SP_CAPACITY]; } sp_plan_t;
int sp_limits_valid(const sp_limits_t *limits);
/* Explicit terminal speed at the last trusted point; current_speed is measured.
 * Normalized geometry is rejected: pixels cannot enter a metres braking model. */
int sp_plan(const sp_preview_t *preview,const sp_limits_t *limits,float current_speed,
            int near_only,float terminal_speed,sp_plan_t *out);
typedef struct { float speed,accel;uint8_t brake_override,infeasible; } sp_trajectory_t;
/* hard_stop clears immediately. Otherwise dt must be (0,max_dt_s]. The cap
 * outranks jerk; infeasible marks a cap requiring >brake deceleration. */
int sp_shape(sp_trajectory_t *state,const sp_limits_t *limits,float target,float cap,
             float dt,int hard_stop);
/* Positive kappa turns right: right is inner. Compensation requires metric
 * geometry and explicitly supplied wheel calibration; otherwise common speed. */
int sp_wheel_targets(float speed,float kappa,float rear_track,int compensation,
                     int geometry_calibrated,float ticks_per_metre_left,
                     float ticks_per_metre_right,float *left_ticks_s,float *right_ticks_s);
typedef struct {float pulse_us,angle_rad;} sp_servo_point_t;
int sp_servo_map(const sp_servo_point_t *map,unsigned count,float angle_rad,float *pulse_us);

typedef enum { SP_DISARMED=0,SP_NORMAL_TRACK,SP_DEGRADED_FAR_LOSS,
               SP_RECOVERY_NO_LANE,SP_CONTROLLED_STOP,SP_STOP_LATCHED,SP_FAULT_LATCHED } sp_mode_t;
enum { SP_FAULT_FRAME=1,SP_FAULT_NEAR=2,SP_FAULT_ENCODER=4,SP_FAULT_CONTROL=8,SP_FAULT_CONFIG=16 };
typedef struct {
    uint32_t frame_timeout_ms,encoder_timeout_ms,near_window_ms,far_budget_ms,stop_timeout_ms;
    unsigned near_frames,far_frames,zero_samples;
    float far_distance_m,zero_speed_m_s;
} sp_policy_t; /* Caller-supplied simulation or reviewed project policy. */
typedef struct {
    uint32_t now_ms,frame_seq,capture_ms,encoder_seq,encoder_ms;
    float odometer_m,measured_speed,odometer_at_capture_m;
    uint8_t new_frame,near_valid,far_valid,encoder_valid,hard_stop,severe_fault,capture_odometer_valid;
} sp_input_t;
typedef struct {
    sp_mode_t mode;
    uint32_t generation,faults,last_frame_seq,last_capture_ms,last_now_ms;
    uint32_t far_started_ms,near_started_ms,last_encoder_seq,last_encoder_ms,stop_started_ms;
    float far_distance_m,last_odometer_m;
    unsigned near_streak,far_streak,zero_streak;
    uint8_t have_frame,far_active,near_active,motor_inhibit,authorized,have_encoder,stop_active;
} sp_supervisor_t;
void sp_supervisor_init(sp_supervisor_t *state);
/* An external C26 owner must perform complete admission. This primitive
 * cannot clear a latch: init/rearm is an explicit test-session boundary. */
int sp_supervisor_arm(sp_supervisor_t *state,const sp_policy_t *policy,const sp_input_t *input);
void sp_supervise(sp_supervisor_t *state,const sp_policy_t *policy,const sp_input_t *input);
int sp_candidate_allowed(const sp_supervisor_t *state,uint32_t candidate_generation);
#endif
