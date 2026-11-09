#include "main.h"
#include "visual_trace.h"
#include "vision_drive.h"
#include "stop_trace.h"
#include "app_transport.h"
#include "app_link.h"
#include <stdio.h>
#include <string.h>
#define VQ_CAPACITY 750U
#define VQ_BYTES (188U*120U)
typedef struct {uint32_t v[16];uint8_t pixels[VQ_BYTES];} vq_frame_t;
AT_SDRAM_SECTION_ALIGN(static vq_frame_t frames[VQ_CAPACITY],64);
static uint32_t count,generation,active,reason,start,cues,cue1,cue2,finished;
extern int vc_diagnostic_band(const vision_result_t *,const vc_config_t *,float);
static uint32_t crc32(const uint8_t *p,unsigned n){uint32_t c=~0U;unsigned i;while(n--){c^=*p++;for(i=0;i<8;i++)c=(c>>1)^((0U-(c&1U))&0xEDB88320U);}return ~c;}
int vq_quiet(void){return active||count;}
static void freeze(uint32_t now,uint32_t why){active=0;reason=why;finished=now;st_test_marker(now);}
void vq_poll(uint32_t now,int observe){
 if(!active)return;
 if(!observe){freeze(now,3);return;}
 if((uint32_t)(now-start)>=30000U){freeze(now,2);return;}
 if(!(cues&1U)&&(uint32_t)(now-start)>=4000U){cues|=1U;cue1=now;st_test_marker(now);}
 if(!(cues&2U)&&(uint32_t)(now-start)>=9000U){cues|=2U;cue2=now;st_test_marker(now);}
}
void vq_record(const uint8_t *gray,const vision_result_t *r,uint32_t seq,uint32_t cap,uint32_t process,uint32_t end,uint32_t observed,uint32_t drops){
 vq_frame_t *f;vd_profile_t cfg;const vc_output_t *out;uint32_t begin;
 if(!active||count>=VQ_CAPACITY||(uint32_t)(cap-start)>=0x80000000U)return;
 begin=timer_get(GPT_TIM_1);f=&frames[count];vision_drive_get_profile(&cfg);out=vision_drive_output();
 f->v[0]=seq;f->v[1]=cap;f->v[2]=process;f->v[3]=end;f->v[4]=observed;f->v[5]=begin;
 f->v[6]=r->valid;f->v[7]=r->status;f->v[8]=r->confidence;
 f->v[9]=(uint32_t)vc_diagnostic_band(r,&cfg.control,cfg.control.near_y);
 f->v[10]=(uint32_t)vc_diagnostic_band(r,&cfg.control,cfg.control.far_y);
 f->v[11]=out->observation_valid;f->v[12]=out->far_valid;f->v[13]=cues;f->v[14]=drops;
 memcpy(f->pixels,gray,VQ_BYTES);f->v[15]=timer_get(GPT_TIM_1);++count;
 if(count==VQ_CAPACITY)freeze(f->v[15],1);
}
int vq_command(const char *s,char *out,unsigned size,int observe,uint32_t now){
 unsigned index;char extra;
 if(!strcmp(s,"vq_start")){
  if(!observe||active||count||app_transport_owner()!=APP_LINK_USB){snprintf(out,size,"[VQ] REJECT start requires USB OBSERVE empty\r\n");return 1;}
  generation++;active=1;reason=0;start=now;cues=cue1=cue2=finished=0;
  snprintf(out,size,"[VQ] START gen=%lu capacity=750 width=188 height=120 baud=921600 cues_ms=4000/9000\r\n",(unsigned long)generation);return 1;
 }
 if(!strcmp(s,"vq_info")){
  snprintf(out,size,"[VQ_INFO] gen=%lu count=%lu active=%lu reason=%lu start=%lu cue1=%lu cue2=%lu finish=%lu cues=%lu capacity=750 payload=22624 baud=921600 zero_pwm=1\r\n",(unsigned long)generation,(unsigned long)count,(unsigned long)active,(unsigned long)reason,(unsigned long)start,(unsigned long)cue1,(unsigned long)cue2,(unsigned long)finished,(unsigned long)cues);return 1;
 }
 if(!strcmp(s,"vq_abort")){if(active)freeze(now,4);snprintf(out,size,"[VQ] ABORTED retained=%lu\r\n",(unsigned long)count);return 1;}
 if(!strncmp(s,"vq_clear ",9)){
  if(active||!observe||sscanf(s+9,"%u%c",&index,&extra)!=1||index!=generation){snprintf(out,size,"[VQ] REJECT clear\r\n");return 1;}
  count=0;reason=0;snprintf(out,size,"[VQ] CLEARED gen=%lu\r\n",(unsigned long)generation);return 1;
 }
 if(!strncmp(s,"vq_frame ",9)){
  uint32_t header[8];vq_frame_t *f;
  if(active||!count||!observe||app_transport_owner()!=APP_LINK_USB||sscanf(s+9,"%u%c",&index,&extra)!=1||index>=count){snprintf(out,size,"[VQ] REJECT frame\r\n");return 1;}
  f=&frames[index];header[0]=0x31465156U;header[1]=generation;header[2]=index;header[3]=sizeof(*f);header[4]=crc32((const uint8_t *)f,sizeof(*f));header[5]=188;header[6]=120;header[7]=16;
  /* Frozen-only blocking export; no UART TX poll/printf runs inside this call. */
  uart_write_buffer(DEBUG_UART_INDEX,(const uint8_t *)header,sizeof(header));uart_write_buffer(DEBUG_UART_INDEX,(const uint8_t *)f,sizeof(*f));
  snprintf(out,size,"[VQ] FRAME_END i=%u\r\n",index);return 1;
 }
 return 0;
}
