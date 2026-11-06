#include <stdint.h>
typedef struct {uint32_t samples,sample_ms,faults;int16_t raw3,raw4,measured_right,measured_left,target_right,target_left;int32_t total3,total4;uint16_t pwm_right,pwm_left;} vd_speed_info_t;
void vision_drive_speed_get(vd_speed_info_t*);
