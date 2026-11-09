#ifndef SMARTCAR_VISION_METRIC_PI_H
#define SMARTCAR_VISION_METRIC_PI_H
#include "vision_metric_io.h"
#include "wheel_speed.h"
/* Diagnostic, caller-owned pair of the EXISTING PI algorithm. No live driver
 * state, GPIO/PWM or legacy duty/steering mix. Never call both target sources
 * on the same physical PI. Active runtime ownership remains to be integrated. */
typedef struct {
    uint32_t revision; /* Same immutable calibration bundle as planner/metric IO. */
    uint16_t kp_left_q8,ki_left_q8,kp_right_q8,ki_right_q8,pwm_cap_left,pwm_cap_right;
} vm_pi_config_t;
typedef struct {
    vm_pi_config_t config;
    ws_pi_t left,right;
    uint64_t session;
    uint32_t revision,last_sequence,last_ms;
    uint8_t configured;
} vm_pi_state_t;
typedef enum { VM_PI_READY=0,VM_PI_WAIT,VM_PI_REJECTED,VM_PI_ZERO } vm_pi_status_t;
typedef struct {
    ws_pi_t left,right;
    vp_stamp_t stamp;
    uint64_t session;
    uint32_t base_revision,result_revision,feedback_revision,elapsed_ms;
    int32_t measured_left,measured_right;
    uint16_t target_left,target_right;
    vm_pi_status_t status;
} vm_pi_candidate_t;
int vm_pi_init(vm_pi_state_t *state,const vm_pi_config_t *config,uint64_t session,
                 const vm_motion_t *anchor);
/* STOP clears PI immediately and invalidates every prepared candidate. An
 * available current feedback sample can anchor the next interval, even while
 * inhibited, so normal recovery need not synthesize or replay lost samples. */
void vm_pi_stop(vm_pi_state_t *state,const vm_motion_t *current);
vm_pi_status_t vm_pi_prepare(const vm_pi_state_t *state,const vps_state_t *planner,
                              const vps_result_t *result,const vm_motion_t *feedback,
                              const vp_stamp_t *current,uint32_t now_ms,
                              uint16_t feedforward_left,uint16_t feedforward_right,
                              vm_pi_candidate_t *out);
/* Caller serializes prepare/commit; rechecks all versions + sample/age and
 * supervisor deadlines. Success publishes only the caller's diagnostic PI
 * state. A rejected candidate has zero PWM; failures never mutate live IO. */
int vm_pi_commit(vm_pi_state_t *state,const vps_state_t *planner,const vps_result_t *result,
                    const vm_motion_t *feedback,const vp_stamp_t *current,uint32_t now_ms,
                    const vm_pi_candidate_t *candidate);
/* Serialized offline composition: ZERO/FAULT resets both diagnostic PIs in
 * this call, WAIT retains state without publishing another command, READY
 * prepares+rechecks+adopts one feedback step. Errors clear diagnostic state. */
vm_pi_status_t vm_pi_apply_shadow(vm_pi_state_t *state,const vps_state_t *planner,
                                    const vps_result_t *result,const vm_motion_t *feedback,
                                    const vp_stamp_t *current,uint32_t now_ms,
                                    uint16_t feedforward_left,uint16_t feedforward_right,
                                    vm_pi_candidate_t *out);
#endif
