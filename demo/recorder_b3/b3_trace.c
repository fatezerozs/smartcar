/* Static prerequisite only: no SD, no new arm path, no positive actuator PWM.
 * All source hooks use bounded RAM operations, never serial/filesystem in IRQ.
 */
#include "main.h"
#include "b3_trace.h"
#include "b3_identity.h"
#include "vision_drive.h"
#include "vision_speed.h"
#include "app_mode.h"
#include "recorder_core.h"
#include "../recorder_b2/include/b2_policy.h"
#include <stdio.h>
#include <string.h>
#ifdef B3_HOST_TEST
#define B3_SYNTHETIC "1"
#else
#define B3_SYNTHETIC "0"
#endif
static b3_event_t events[B3_CAPACITY];
static volatile uint32_t count,lost,active,frozen,reason,waiting_c26,trigger_snapshot;
static uint32_t run,pi_start,pi_last_ms,pi_gap_max,pi_cycles_max,mask_cycles_max,emit_cycles_max;
static volatile uint32_t begin_ms,last_pit_sample,last_mode=~0U,last_raw;
static uint32_t last_pi_sample,last_pi_consumed,last_pi_serviced,last_state=~0U,last_faults=~0U;
static volatile uint32_t requested[3],applied[3],pwm_known[3],last_pwm_seen[3],last_pwm_request[3];
static unsigned ready,exported,start_pending,dump_pending,status_pending,announced;
static b2_line_t line;
static void text(const char *s){LPUART_WriteBlocking(LPUART1,(const uint8_t*)s,strlen(s));}
static uint32_t emit(unsigned kind,const int32_t *v)
{
 uint32_t mask,start,index;if(!active)return 0;mask=__get_PRIMASK();__disable_irq();start=DWT->CYCCNT;
 if(!active){__set_PRIMASK(mask);return 0;}
 if(count>=B3_CAPACITY){++lost;active=0;frozen=1;reason=2;__set_PRIMASK(mask);return 0;}
 index=count;events[index].ordinal=index+1;events[index].kind=kind;
 events[index].ms=timer_get(GPT_TIM_1);events[index].cycles=start;
 memcpy(events[index].v,v,48);__DMB();count=index+1;
 {uint32_t cycles=DWT->CYCCNT-start;if(cycles>emit_cycles_max)emit_cycles_max=cycles;}
 __set_PRIMASK(mask);return index+1;
}
static unsigned channel(uint32_t pin)
{return pin==(uint32_t)MOTOR1_PWM?0U:pin==(uint32_t)MOTOR2_PWM?1U:pin==(uint32_t)SERVO_MOTOR_PWM?2U:3U;}
void b3_pwm_write(uint32_t pin,uint32_t request,uint32_t actual,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t mctrl)
{
 unsigned ch=channel(pin);int32_t v[12]={0};if(ch==3)return;
 requested[ch]=request;applied[ch]=actual;pwm_known[ch]=1;
 if(!active|| (last_pwm_seen[ch]&&last_pwm_request[ch]==request))return;
 last_pwm_seen[ch]=1;last_pwm_request[ch]=request;
 v[0]=(int32_t)pin;v[1]=(int32_t)request;v[2]=(int32_t)actual;
 v[3]=(int32_t)a;v[4]=(int32_t)b;v[5]=(int32_t)c;v[6]=(int32_t)d;v[7]=(int32_t)mctrl;v[8]=(int32_t)ch;v[9]=PWM_DUTY_MAX;v[10]=1;
 (void)emit(B3_COMMIT,v);
}
static void snapshot_pwm(void)
{
 static PWM_Type *const bases[]=PWM_BASE_PTRS;unsigned i;
 const uint32_t pins[3]={MOTOR1_PWM,MOTOR2_PWM,SERVO_MOTOR_PWM};
 for(i=0;i<3;i++){
  uint32_t mask=__get_PRIMASK();__disable_irq();
  uint32_t pin=pins[i],module=pin%40U/10U;PWM_Type *base=bases[pin/40U];int32_t v[12]={0};
  v[0]=(int32_t)pin;v[1]=(int32_t)requested[i];v[2]=(int32_t)applied[i];
  v[3]=base->SM[module].VAL2;v[4]=base->SM[module].VAL3;v[5]=base->SM[module].VAL4;v[6]=base->SM[module].VAL5;v[7]=base->MCTRL;v[8]=(int32_t)i;v[9]=PWM_DUTY_MAX;v[10]=(int32_t)pwm_known[i];
  (void)emit(B3_PWM_SNAPSHOT,v);
  __set_PRIMASK(mask);
 }
}
void b3_encoder_read(uint16_t left,uint16_t right)
{int32_t v[12]={0};if(!active)return;v[0]=left;v[1]=right;last_raw=emit(B3_RAW,v);}
void b3_pit_sample(void)
{
 vd_speed_info_t s;int32_t v[12];if(!active)return;vision_drive_speed_get(&s);
 if(!last_raw||last_pit_sample==s.samples)return;last_pit_sample=s.samples;
 v[0]=(int32_t)s.samples;v[1]=(int32_t)s.sample_ms;v[2]=s.raw3;v[3]=s.raw4;v[4]=s.total3;v[5]=s.total4;
 v[6]=s.target_right;v[7]=s.target_left;v[8]=s.pwm_right;v[9]=s.pwm_left;v[10]=(int32_t)s.faults;v[11]=(int32_t)last_raw;(void)emit(B3_SAMPLE,v);
}
void b3_pi_begin(uint32_t now)
{if(active){if(pi_last_ms&&now-pi_last_ms>pi_gap_max)pi_gap_max=now-pi_last_ms;pi_last_ms=now;pi_start=DWT->CYCCNT;}}
void b3_pi_end(unsigned serviced,uint32_t masked,const void *locked_snapshot,uint32_t consumed)
{
 vd_speed_info_t s;int32_t v[12];uint32_t elapsed;if(!active)return;elapsed=DWT->CYCCNT-pi_start;
 if(elapsed>pi_cycles_max)pi_cycles_max=elapsed;if(masked>mask_cycles_max)mask_cycles_max=masked;
 s=*(const vd_speed_info_t*)locked_snapshot;
 if(last_pi_sample==s.samples&&last_pi_consumed==consumed&&last_pi_serviced==serviced)return;
 last_pi_sample=s.samples;last_pi_consumed=consumed;last_pi_serviced=serviced;
 v[0]=(int32_t)s.samples;v[1]=(int32_t)s.sample_ms;v[2]=s.target_right;v[3]=s.target_left;v[4]=s.measured_right;v[5]=s.measured_left;
 v[6]=s.pwm_right;v[7]=s.pwm_left;v[8]=(int32_t)s.faults;v[9]=(int32_t)elapsed;v[10]=(int32_t)masked;v[11]=(int32_t)consumed;(void)emit(serviced?B3_PI_WORK:B3_PI,v);
}
void b3_mode(int a,int b,unsigned raw,unsigned selected,unsigned settled,uint32_t revision)
{
 uint32_t signature=(uint32_t)a|((uint32_t)b<<1)|(raw<<2)|(selected<<5)|(settled<<8);int32_t v[12]={0};
 if(waiting_c26&&!a){waiting_c26=0;begin_ms=timer_get(GPT_TIM_1);__DMB();active=1;trigger_snapshot=1;}
 if(!active||signature==last_mode)return;last_mode=signature;
 v[0]=a;v[1]=b;v[2]=(int32_t)raw;v[3]=(int32_t)selected;v[4]=(int32_t)settled;v[5]=(int32_t)revision;(void)emit(B3_MODE,v);
}
static int32_t bits(float x){int32_t v;memcpy(&v,&x,4);return v;}
void b3_vision(uint32_t seq,uint32_t ms)
{
 const vc_output_t *o=vision_drive_output();int32_t v[12];if(!active)return;
 v[0]=(int32_t)seq;v[1]=(int32_t)ms;v[2]=o->observation_valid;v[3]=o->far_valid;
 v[4]=bits(o->lateral);v[5]=bits(o->heading);v[6]=bits(o->steering);v[7]=bits(o->near_center_x);v[8]=bits(o->far_center_x);
 v[9]=(int32_t)o->state;v[10]=(int32_t)o->faults;v[11]=o->recovering|((int32_t)o->search_source<<8);(void)emit(B3_VISION,v);
}
void b3_init(void)
{CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;}
uint32_t b3_filter(uint8_t *data,uint32_t n)
{
 uint32_t i,stop=0;
 for(i=0;i<n;i++){
  if(data[i]=='!'||data[i]==3){stop=1;line.used=0;line.discard=1;continue;}
  if(b2_line_feed(&line,data[i])){
   if(!strcmp(line.text,"@b3 status"))status_pending=1;
   else if(!strcmp(line.text,"@b3 capture"))start_pending=1;
   else if(!strcmp(line.text,"@b3 arm"))start_pending=2;
   else if(!strcmp(line.text,"@b3 clear_empty")){if(!active&&!waiting_c26&&!count){ready=frozen=reason=announced=0;}}
   else if(!strcmp(line.text,"@b3 freeze")){active=0;waiting_c26=0;trigger_snapshot=0;frozen=1;reason=3;}
   else if(!strcmp(line.text,"@b3 dump"))dump_pending=1;
   /* All other UART configuration/arming commands are discarded in this static profile. */
  }
 }
 if(stop){data[0]='!';return 1;}return 0;
}
void b3_poll(void)
{
 char s[640];const vc_output_t *o=vision_drive_output();uint32_t now=timer_get(GPT_TIM_1);
 if(status_pending){status_pending=0;snprintf(s,sizeof s,"B3STATIC id=RECORDER_B3_STATIC_C26_1 version=1 outputs=forced_zero mode=%u raw_c26=%u active=%lu frozen=%lu waiting=%lu count=%lu lost=%lu run=%lu source=%s\r\n",(unsigned)app_mode_current(),(unsigned)gpio_get_level(C26),(unsigned long)active,(unsigned long)frozen,(unsigned long)waiting_c26,(unsigned long)count,(unsigned long)lost,(unsigned long)run,B3_INPUT_DIGEST);text(s);}
 if(start_pending){
  unsigned wait=start_pending==2;
  start_pending=0;
  if(active||waiting_c26||(ready&&!exported)){text("B3TRACE ERROR busy_or_unexported\r\n");}
  else if(wait&&(!app_mode_allows(APP_MODE_OBSERVE)||!gpio_get_level(C26))){text("B3TRACE ERROR arm_requires_C26_OFF\r\n");}
  else{
   uint32_t mask;snprintf(s,sizeof s,"B3TRACE %s run=%lu max_ms=%u capacity=%u static=1\r\n",wait?"ARMED":"START",(unsigned long)(run+1),B3_WINDOW_MS,B3_CAPACITY);text(s);
   mask=__get_PRIMASK();__disable_irq();count=lost=reason=0;frozen=0;exported=0;announced=0;ready=1;++run;
   pi_last_ms=pi_gap_max=pi_cycles_max=mask_cycles_max=emit_cycles_max=last_raw=0;last_pi_sample=last_pit_sample=last_pi_consumed=last_pi_serviced=~0U;last_mode=last_state=last_faults=~0U;{unsigned i;for(i=0;i<3;i++)last_pwm_seen[i]=0;}
   begin_ms=timer_get(GPT_TIM_1);waiting_c26=wait;trigger_snapshot=0;active=!wait;__set_PRIMASK(mask);if(!wait)snapshot_pwm();
  }
 }
 now=timer_get(GPT_TIM_1);
 if(waiting_c26&&(uint32_t)(now-begin_ms)>=30000U){uint32_t mask=__get_PRIMASK();__disable_irq();waiting_c26=0;frozen=1;reason=4;__set_PRIMASK(mask);}
 if(trigger_snapshot){trigger_snapshot=0;snapshot_pwm();}
 if(active){
  int32_t v[12]={0};if(now-begin_ms>=B3_WINDOW_MS){uint32_t mask=__get_PRIMASK();__disable_irq();active=0;frozen=1;reason=1;__set_PRIMASK(mask);}
  if((uint32_t)o->state!=last_state||o->faults!=last_faults){last_state=(uint32_t)o->state;last_faults=o->faults;v[0]=(int32_t)o->state;v[1]=(int32_t)o->faults;v[2]=o->observation_valid;v[3]=o->far_valid;v[4]=o->recovering;(void)emit(B3_STATE,v);}
 }
 if(frozen&&!announced){announced=1;snprintf(s,sizeof s,"B3TRACE FROZEN run=%lu count=%lu lost=%lu reason=%lu\r\n",(unsigned long)run,(unsigned long)count,(unsigned long)lost,(unsigned long)reason);text(s);}
 if(dump_pending){
  uint32_t bytes,crc;dump_pending=0;
  if(!ready||!frozen||active||!app_mode_allows(APP_MODE_OBSERVE)||app_mode_change_pending()||o->state==VC_RUNNING){text("B3TRACE ERROR require_frozen_C26_OFF_stopped\r\n");return;}
  bytes=count*sizeof(b3_event_t);crc=rec_crc_update(0,events,bytes);
  snprintf(s,sizeof s,"B3TRACE DATA format=B3T1 run=%lu count=%lu bytes=%lu crc32=%08lX lost=%lu reason=%lu hz=%lu pi_gap_max_ms=%lu pi_max_cycles=%lu mask_max_cycles=%lu emit_max_cycles=%lu m1=%u m2=%u servo=%u static=1 synthetic=" B3_SYNTHETIC " source=%s\r\n",(unsigned long)run,(unsigned long)count,(unsigned long)bytes,(unsigned long)crc,(unsigned long)lost,(unsigned long)reason,(unsigned long)SystemCoreClock,(unsigned long)pi_gap_max,(unsigned long)pi_cycles_max,(unsigned long)mask_cycles_max,(unsigned long)emit_cycles_max,(unsigned)MOTOR1_PWM,(unsigned)MOTOR2_PWM,(unsigned)SERVO_MOTOR_PWM,B3_INPUT_DIGEST);text(s);
  LPUART_WriteBlocking(LPUART1,(const uint8_t*)events,bytes);text("\r\nB3TRACE END\r\n");exported=1;
 }
}
