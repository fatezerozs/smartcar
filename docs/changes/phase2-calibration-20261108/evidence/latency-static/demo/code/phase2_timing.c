#include "phase2_timing.h"
#include <stdio.h>
#include <string.h>
#define PT_CAPACITY 256U
static uint32_t rows[PT_CAPACITY][8], count, generation, started, active, reason;
int pt_active(void){return active!=0;}
void pt_poll(uint32_t now,int observe){if(active && (!observe || (uint32_t)(now-started)>=30000U)){active=0;reason=observe?2U:3U;}}
void pt_record(uint32_t seq,uint32_t cap,uint32_t copy,uint32_t process,uint32_t end,uint32_t observed,uint32_t flags,uint32_t drops){
 uint32_t *p;if(!active||count>=PT_CAPACITY)return;p=rows[count++];p[0]=seq;p[1]=cap;p[2]=copy;p[3]=process;p[4]=end;p[5]=observed;p[6]=flags;p[7]=drops;
 if(count==PT_CAPACITY){active=0;reason=1;}
}
static uint32_t checksum(void){uint32_t h=2166136261U;unsigned i,j,k;for(i=0;i<count;i++)for(j=0;j<8;j++)for(k=0;k<4;k++){h^=(rows[i][j]>>(8*k))&255U;h*=16777619U;}return h;}
int pt_command(const char *s,char *out,unsigned size,int observe,uint32_t now){
 unsigned index;char extra;uint32_t *p;
 if(!strcmp(s,"timing_start")){
  if(!observe||active||count){snprintf(out,size,"[TIMING] REJECT start requires OBSERVE and empty buffer\r\n");return 1;}
  ++generation;active=1;reason=0;started=now;snprintf(out,size,"[TIMING] START gen=%lu capacity=256 clock_ms=1 zero_pwm=1\r\n",(unsigned long)generation);return 1;
 }
 if(!strcmp(s,"timing_clear")){
  if(!observe||active){snprintf(out,size,"[TIMING] REJECT clear requires OBSERVE and inactive\r\n");return 1;}
  count=0;reason=0;snprintf(out,size,"[TIMING] CLEARED gen=%lu\r\n",(unsigned long)generation);return 1;
 }
 if(!strcmp(s,"timing_info")){
  snprintf(out,size,"[TIMING_INFO] gen=%lu count=%lu active=%lu reason=%lu fnv1a=%08lX fields=8 clock_ms=1 zero_pwm=1\r\n",(unsigned long)generation,(unsigned long)count,(unsigned long)active,(unsigned long)reason,(unsigned long)(active?0:checksum()));return 1;
 }
 if(!strncmp(s,"timing_row ",11)){
  if(active||sscanf(s+11,"%u%c",&index,&extra)!=1||index>=count){snprintf(out,size,"[TIMING] REJECT row\r\n");return 1;}
  p=rows[index];snprintf(out,size,"[TIMING_ROW] gen=%lu i=%u seq=%lu cap=%lu copy=%lu process=%lu end=%lu observed=%lu flags=%lu drops=%lu\r\n",(unsigned long)generation,index,(unsigned long)p[0],(unsigned long)p[1],(unsigned long)p[2],(unsigned long)p[3],(unsigned long)p[4],(unsigned long)p[5],(unsigned long)p[6],(unsigned long)p[7]);return 1;
 }
 return 0;
}
