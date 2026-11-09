#ifndef SPEED_RESPONSE_TRACE_H
#define SPEED_RESPONSE_TRACE_H
#include <stdint.h>
#define ST_CAPACITY 640U
#define ST_POST_MS 2000U
#define ST_FIELDS 22U
#define ST_CONFIG_FIELDS 10U
typedef struct {
 uint32_t ms,seq;int32_t total_left,total_right;int16_t raw_left,raw_right;
 uint16_t pwm_right,pwm_left;uint8_t phase,reason;
 uint32_t interval,pi_seq,pi_ms;int16_t target_r,target_l,measured_r,measured_l;
 int32_t integral_r,integral_l;uint32_t faults;uint8_t detail,enabled;
} st_row_t;
typedef struct {uint32_t generation,start_ms,stop_ms,count;uint8_t recording,frozen,stop_seen,overflow,reason;} st_info_t;
void st_init(void);
int st_can_arm(void);
int st_release(uint32_t generation);
int st_distance_exhausted(int32_t left,int32_t right);
void st_test_marker(uint32_t now);
void st_arm(uint32_t now,uint32_t seq,int32_t left,int32_t right);
void st_set_config(const uint32_t values[ST_CONFIG_FIELDS]);
void st_get_config(uint32_t values[ST_CONFIG_FIELDS]);
void st_sample(uint32_t now,uint32_t seq,int16_t left,int16_t right,int32_t total_left,int32_t total_right,uint8_t phase,uint8_t reason);
void st_context(uint32_t interval,uint32_t faults,uint8_t detail,uint8_t enabled);
void st_control(uint32_t sequence,uint32_t now,int16_t right,int16_t left,int16_t measured_r,int16_t measured_l,int32_t integral_r,int32_t integral_l);
void st_reset_control(uint32_t now);
void st_output(uint16_t right,uint16_t left);
void st_zero(uint32_t now,uint8_t reason);
int st_marker(uint32_t now);
void st_info(st_info_t *out);
int st_row(unsigned index,st_row_t *out);
void st_values(const st_row_t *row,uint32_t values[ST_FIELDS]);
uint32_t st_crc(void);
#endif
