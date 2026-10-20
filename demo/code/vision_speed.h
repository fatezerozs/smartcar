#ifndef VISION_SPEED_H
#define VISION_SPEED_H
#include <stdint.h>
typedef struct {
    uint32_t samples,sample_ms,faults;
    uint32_t fault_control_age_ms,fault_sample_age_ms,fault_interval_ms;
    int32_t total3,total4;
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
/* Called only under output-layer IRQ mask; returns 0 after an interlock trip. */
int vd_speed_output(uint16_t request_right,uint16_t request_left,int reverse,
                    uint16_t cap,uint16_t *pwm_right,uint16_t *pwm_left);
#endif
