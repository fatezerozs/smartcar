#include "stop_trace.h"
#include <string.h>
static st_row_t rows[ST_CAPACITY],latest;
static volatile st_info_t info;
static int32_t origin_left,origin_right;
static uint32_t config[ST_CONFIG_FIELDS];
static uint32_t marker_ms;static uint8_t marker_seen;
void st_init(void){memset((void *)&info,0,sizeof(info));memset(&latest,0,sizeof(latest));memset(config,0,sizeof(config));marker_seen=0;}
void st_set_config(const uint32_t values[ST_CONFIG_FIELDS]){if(st_can_arm())memcpy(config,values,sizeof(config));}
void st_get_config(uint32_t values[ST_CONFIG_FIELDS]){memcpy(values,config,sizeof(config));}
int st_can_arm(void){return !info.recording&&!info.frozen;}
int st_release(uint32_t generation){if(!info.frozen||generation!=info.generation)return 0;info.frozen=0;return 1;}
int st_distance_exhausted(int32_t left,int32_t right){
 int64_t dl=(int64_t)left-origin_left,dr=(int64_t)right-origin_right;
 if(dl<0)dl=-dl;if(dr<0)dr=-dr;
 return info.recording&&(dl>=11394||dr>=11389);
}
void st_test_marker(uint32_t now){marker_ms=now;marker_seen=1;}
void st_context(uint32_t interval,uint32_t faults,uint8_t detail,uint8_t enabled){latest.interval=interval;latest.faults=faults;latest.detail=detail;latest.enabled=enabled;}
void st_control(uint32_t sequence,uint32_t now,int16_t right,int16_t left,int16_t measured_r,int16_t measured_l,int32_t integral_r,int32_t integral_l){
 latest.pi_seq=sequence;latest.pi_ms=now;latest.target_r=right;latest.target_l=left;
 latest.measured_r=measured_r;latest.measured_l=measured_l;latest.integral_r=integral_r;latest.integral_l=integral_l;
}
void st_reset_control(uint32_t now){latest.pi_ms=now;latest.target_r=latest.target_l=0;latest.measured_r=latest.measured_l=0;latest.integral_r=latest.integral_l=0;}
void st_arm(uint32_t now,uint32_t seq,int32_t left,int32_t right){
 uint32_t generation=info.generation+1U;memset((void *)&info,0,sizeof(info));info.generation=generation;info.start_ms=now;info.recording=1;
 origin_left=left;origin_right=right;
 latest.pwm_right=latest.pwm_left=0;st_sample(now,seq,0,0,left,right,1,0);rows[0].interval=0;
}
void st_sample(uint32_t now,uint32_t seq,int16_t left,int16_t right,int32_t total_left,int32_t total_right,uint8_t phase,uint8_t reason){
 st_row_t *r;if(!info.recording)return;
 if(info.count>=ST_CAPACITY){info.overflow=1;info.recording=0;info.frozen=1;return;}
 r=&rows[info.count];*r=latest;r->ms=now;r->seq=seq;r->raw_left=left;r->raw_right=right;
 r->total_left=total_left;r->total_right=total_right;r->phase=phase;r->reason=reason;++info.count;
 if(info.stop_seen&&(uint32_t)(now-info.stop_ms)>=ST_POST_MS){info.recording=0;info.frozen=1;}
}
void st_output(uint16_t right,uint16_t left){latest.pwm_right=right;latest.pwm_left=left;}
void st_zero(uint32_t now,uint8_t reason){st_output(0,0);if(info.recording&&!info.stop_seen){info.stop_seen=1;info.stop_ms=now;info.reason=reason;st_test_marker(now);}}
int st_marker(uint32_t now){return marker_seen&&(uint32_t)(now-marker_ms)<200U;}
void st_info(st_info_t *out){*out=info;}
int st_row(unsigned index,st_row_t *out){if(!info.frozen||index>=info.count)return 0;*out=rows[index];return 1;}
void st_values(const st_row_t *r,uint32_t v[ST_FIELDS]){
 v[0]=r->ms;v[1]=r->seq;v[2]=(uint32_t)r->total_left;v[3]=(uint32_t)r->total_right;
 v[4]=(uint32_t)(int32_t)r->raw_left;v[5]=(uint32_t)(int32_t)r->raw_right;
 v[6]=r->pwm_right;v[7]=r->pwm_left;v[8]=r->phase;v[9]=r->reason;
 v[10]=r->interval;v[11]=r->pi_seq;v[12]=r->pi_ms;
 v[13]=(uint32_t)(int32_t)r->target_r;v[14]=(uint32_t)(int32_t)r->target_l;
 v[15]=(uint32_t)(int32_t)r->measured_r;v[16]=(uint32_t)(int32_t)r->measured_l;
 v[17]=(uint32_t)r->integral_r;v[18]=(uint32_t)r->integral_l;v[19]=r->faults;v[20]=r->detail;v[21]=r->enabled;
}
uint32_t st_crc(void){
 uint32_t c=~0U,v[ST_FIELDS];if(!info.frozen)return 0;
 for(unsigned f=0;f<ST_CONFIG_FIELDS;++f)for(unsigned j=0;j<4;++j){c^=(uint8_t)(config[f]>>(8U*j));for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1U)?0xEDB88320U:0U);}
 for(unsigned i=0;i<info.count;++i){st_values(&rows[i],v);for(unsigned f=0;f<ST_FIELDS;++f)for(unsigned j=0;j<4;++j){c^=(uint8_t)(v[f]>>(8U*j));for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1U)?0xEDB88320U:0U);}}
 return ~c;
}
