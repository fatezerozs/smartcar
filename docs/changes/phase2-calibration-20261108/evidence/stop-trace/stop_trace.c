#include "stop_trace.h"
#include <string.h>
static st_row_t rows[ST_CAPACITY];
static volatile st_info_t info;
static uint16_t right_pwm,left_pwm;
static uint32_t marker_ms;static uint8_t marker_seen;
void st_init(void){memset((void *)&info,0,sizeof(info));right_pwm=left_pwm=0;marker_seen=0;}
int st_can_arm(void){return !info.recording&&!info.frozen;}
int st_release(uint32_t generation){if(!info.frozen||generation!=info.generation)return 0;info.frozen=0;return 1;}
void st_test_marker(uint32_t now){marker_ms=now;marker_seen=1;}
void st_arm(uint32_t now,uint32_t seq,int32_t left,int32_t right){
 uint32_t gen=info.generation+1U;memset((void *)&info,0,sizeof(info));info.generation=gen;info.start_ms=now;info.recording=1;
 right_pwm=left_pwm=0;
 st_sample(now,seq,0,0,left,right,1,0);
}
void st_sample(uint32_t now,uint32_t seq,int16_t left,int16_t right,int32_t total_left,int32_t total_right,uint8_t phase,uint8_t reason){
 st_row_t *r;if(!info.recording)return;
 if(info.count>=ST_CAPACITY){info.overflow=1;info.recording=0;info.frozen=1;return;}
 r=&rows[info.count++];r->ms=now;r->seq=seq;r->total_left=total_left;r->total_right=total_right;r->raw_left=left;r->raw_right=right;
 r->pwm_right=right_pwm;r->pwm_left=left_pwm;r->phase=phase;r->reason=reason;
 if(info.stop_seen&&(uint32_t)(now-info.stop_ms)>=ST_POST_MS){info.recording=0;info.frozen=1;}
}
void st_output(uint16_t right,uint16_t left){right_pwm=right;left_pwm=left;}
void st_zero(uint32_t now,uint8_t reason){right_pwm=left_pwm=0;if(info.recording&&!info.stop_seen){info.stop_seen=1;info.stop_ms=now;info.reason=reason;st_test_marker(now);}}
int st_marker(uint32_t now){return marker_seen&&(uint32_t)(now-marker_ms)<200U;}
void st_info(st_info_t *out){*out=info;}
int st_row(unsigned index,st_row_t *out){if(!info.frozen||index>=info.count)return 0;*out=rows[index];return 1;}
static uint32_t crc_u32(uint32_t c,uint32_t v){for(unsigned i=0;i<4;++i){c^=(uint8_t)(v>>(8U*i));for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1U)?0xEDB88320U:0U);}return c;}
uint32_t st_crc(void){
 uint32_t c=~0U;if(!info.frozen)return 0;
 for(unsigned i=0;i<info.count;++i){const st_row_t *r=&rows[i];
 c=crc_u32(c,r->ms);c=crc_u32(c,r->seq);c=crc_u32(c,(uint32_t)r->total_left);c=crc_u32(c,(uint32_t)r->total_right);
 c=crc_u32(c,(uint32_t)(int32_t)r->raw_left);c=crc_u32(c,(uint32_t)(int32_t)r->raw_right);
 c=crc_u32(c,r->pwm_right);c=crc_u32(c,r->pwm_left);c=crc_u32(c,r->phase);c=crc_u32(c,r->reason);}
 return ~c;
}
