#ifndef VISION_SPEED_H
#define VISION_SPEED_H
#include <stdint.h>

/* Software timing contract for the foreground PI service and PIT sampling.
 * These are fail-closed limits, not a claim of hard real-time scheduling.
 * The nominal encoder sample cadence is 10ms; the foreground service must
 * consume each new sample once and must not replay missed samples. */
#define VD_SPEED_SAMPLE_MIN_INTERVAL_MS 10U
#define VD_SPEED_SAMPLE_MAX_INTERVAL_MS 20U
#define VD_SPEED_SAMPLE_MAX_AGE_MS 30U
#define VD_SPEED_CONTROL_STALL_TIMEOUT_MS 50U
#define VD_SPEED_STARTUP_SAMPLE_MAX_AGE_MS 20U
#define VD_SPEED_MAX_UNCONSUMED_SAMPLES 1U
typedef struct {
    uint32_t samples,sample_ms,faults;
    uint32_t fault_control_age_ms,fault_sample_age_ms,fault_interval_ms;
    int32_t total3,total4;
    int16_t fault_raw3,fault_raw4,fault_target_right,fault_target_left;
    uint16_t fault_pwm_right,fault_pwm_left;
    int16_t raw3,raw4,target_right,target_left,measured_right,measured_left;
    uint16_t pwm_right,pwm_left,max_ticks,kp_q8,ki_q8;
    int8_t sign3,sign4;
    uint8_t enabled,signs_confirmed,fault_detail;
} vd_speed_info_t;
enum { VS_FAULT_SAMPLE_GAP=1,VS_FAULT_CONTROL_GAP,VS_FAULT_DELTA,
    VS_FAULT_OUTPUT,VS_FAULT_STALE,VS_FAULT_BACKLOG,VS_FAULT_TARGET,
    VS_FAULT_REVERSE_RIGHT,VS_FAULT_REVERSE_LEFT,VS_FAULT_NOCOUNT_RIGHT,VS_FAULT_NOCOUNT_LEFT };
const char *vision_drive_speed_fault_name(unsigned detail);
void vision_drive_speed_get(vd_speed_info_t *out);
int vision_drive_encoder_signs(int sign3,int sign4);
int vision_drive_speed_config(unsigned max_ticks,float kp,float ki);
int vision_drive_speed_enable(int enable);
void vd_speed_init(uint32_t now);
void vd_speed_sample_tick(uint32_t now);
void vd_speed_stop(void);
/* Publish target and read held PWM under the output IRQ mask. No PI here.
 * cap is the target-request scale; max_ticks limits count/10ms targets.
 * C31 PWM hard limit is MT_ROUTE_PWM_FULL_SCALE, independently of this cap. */
int vd_speed_output(uint16_t request_right,uint16_t request_left,int reverse,
                    uint16_t cap,uint16_t *pwm_right,uint16_t *pwm_left);
/* Foreground service, at most once per distinct encoder sample. Missing samples
 * fail closed; never replay the latest measurement as multiple PI steps.
 * Not a guaranteed fixed-period scheduler; PIT1 remains sample/cutoff only. */
void vd_speed_control_poll(uint32_t now_ms);
#endif
