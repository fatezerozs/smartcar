#ifndef STOP_TRACE_H
#define STOP_TRACE_H
#include <stdint.h>
#define ST_CAPACITY 640U
#define ST_POST_MS 2000U
typedef struct {uint32_t ms,seq;int32_t total_left,total_right;int16_t raw_left,raw_right;uint16_t pwm_right,pwm_left;uint8_t phase,reason;} st_row_t;
typedef struct {uint32_t generation,start_ms,stop_ms,count;uint8_t recording,frozen,stop_seen,overflow,reason;} st_info_t;
void st_init(void);
int st_can_arm(void);
int st_release(uint32_t generation);
void st_test_marker(uint32_t now);
void st_arm(uint32_t now,uint32_t seq,int32_t left,int32_t right);
void st_sample(uint32_t now,uint32_t seq,int16_t left,int16_t right,int32_t total_left,int32_t total_right,uint8_t phase,uint8_t reason);
void st_output(uint16_t right,uint16_t left);
void st_zero(uint32_t now,uint8_t reason);
int st_marker(uint32_t now);
void st_info(st_info_t *out);
int st_row(unsigned index,st_row_t *out);
uint32_t st_crc(void);
#endif
