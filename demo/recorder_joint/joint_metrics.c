#include "main.h"
#include "joint.h"
#include "vision_drive.h"
#include "vision_speed.h"
#include "recorder_runtime.h"
#include "b2_metrics.h"
#include <stdio.h>
#include <string.h>
typedef struct {
 uint32_t begin,end,control_calls,control_gap_max,control_budget_miss,control_cycles_max;
 uint32_t sample_count,sample_gap_min,sample_gap_max,sample_not_10ms;
 uint32_t pi_calls,pi_consumed,pi_cycles_max,mask_cycles_max,pi_faults;
 uint32_t frames,frame_age_max,c26_levels,mode_changes,pwm_nonzero,pwm_calls;
 uint32_t io_calls,io_errors,io_cycles_max;
 uint32_t queue_last,queue_max,queue_observations;
 rec_stats_t rec_begin,rec_end;
} phase_t;
static phase_t phases[5];
static volatile unsigned current;
static uint32_t last_control,control_start,last_sample,last_sample_seq,last_consumed,pi_start,pwm_known;
static uint32_t pin_request[3],pin_applied[3],pin_vals[3][4];
static uint32_t runtime_last;
static uint32_t queue_depth;
static uint64_t runtime_cycles;
void b2_runtime_init(void){runtime_last=DWT->CYCCNT;runtime_cycles=0;}
uint32_t b2_runtime_us(void)
{
 uint32_t mask=__get_PRIMASK(),now,result;__disable_irq();now=DWT->CYCCNT;
 runtime_cycles+=(uint32_t)(now-runtime_last);runtime_last=now;
 result=(uint32_t)(runtime_cycles/(SystemCoreClock/1000000U));__set_PRIMASK(mask);return result;
}
static unsigned channel(uint32_t pin){return pin==MOTOR1_PWM?0:pin==MOTOR2_PWM?1:pin==SERVO_MOTOR_PWM?2:3;}
void joint_phase(unsigned next)
{
 uint32_t mask=__get_PRIMASK();__disable_irq();
 if(current){phases[current].end=timer_get(GPT_TIM_1);recorder_stats(&phases[current].rec_end);}
 current=next;
 if(next){memset(&phases[next],0,sizeof(phase_t));phases[next].begin=timer_get(GPT_TIM_1);phases[next].sample_gap_min=~0U;phases[next].queue_last=phases[next].queue_max=queue_depth;recorder_stats(&phases[next].rec_begin);}
 last_control=last_sample=0;last_sample_seq=last_consumed=~0U;
 __set_PRIMASK(mask);
}
void joint_control_begin(uint32_t now)
{
 control_start=DWT->CYCCNT;
 if(current){phase_t *p=&phases[current];uint32_t gap=now-last_control;
  if(last_control){if(gap>p->control_gap_max)p->control_gap_max=gap;if(gap>10U)++p->control_budget_miss;}
  last_control=now;++p->control_calls;
 }
}
void joint_control_end(uint32_t now)
{
 uint32_t cycles=DWT->CYCCNT-control_start;(void)now;
 if(current&&cycles>phases[current].control_cycles_max)phases[current].control_cycles_max=cycles;
}
void b3_pit_sample(void)
{
 vd_speed_info_t s;vision_drive_speed_get(&s);
 if(current&&s.samples!=last_sample_seq){phase_t *p=&phases[current];uint32_t gap=s.sample_ms-last_sample;
  if(last_sample){if(gap<p->sample_gap_min)p->sample_gap_min=gap;if(gap>p->sample_gap_max)p->sample_gap_max=gap;if(gap!=10U)++p->sample_not_10ms;}
  last_sample=s.sample_ms;last_sample_seq=s.samples;++p->sample_count;
 }
}
void b3_pi_begin(uint32_t now){(void)now;pi_start=DWT->CYCCNT;}
void b3_pi_end(unsigned serviced,uint32_t masked,const void *snapshot,uint32_t consumed)
{
 if(current){phase_t *p=&phases[current];const vd_speed_info_t *s=snapshot;uint32_t elapsed=DWT->CYCCNT-pi_start;
  ++p->pi_calls;if(elapsed>p->pi_cycles_max)p->pi_cycles_max=elapsed;if(masked>p->mask_cycles_max)p->mask_cycles_max=masked;
  if(serviced&&last_consumed!=~0U&&consumed!=last_consumed&&(s->target_right||s->target_left))++p->pi_consumed;
  last_consumed=consumed;p->pi_faults|=s->faults;
 }
}
void b3_mode(int a,int b,unsigned raw,unsigned selected,unsigned settled,uint32_t revision)
{(void)b;(void)raw;(void)selected;(void)settled;(void)revision;if(current)phases[current].c26_levels|=1U<<(a?1U:0U);}
void b3_encoder_read(uint16_t left,uint16_t right){(void)left;(void)right;}
void joint_queue_depth(uint32_t depth)
{
 uint32_t mask=__get_PRIMASK();__disable_irq();queue_depth=depth;
 if(current){phase_t *p=&phases[current];p->queue_last=depth;if(depth>p->queue_max)p->queue_max=depth;++p->queue_observations;}
 __set_PRIMASK(mask);
}
void b3_vision(uint32_t sequence,uint32_t capture_ms)
{(void)sequence;if(current){uint32_t age=timer_get(GPT_TIM_1)-capture_ms;phase_t *p=&phases[current];++p->frames;if(age>p->frame_age_max)p->frame_age_max=age;}}
void b3_pwm_write(uint32_t pin,uint32_t request,uint32_t actual,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t mctrl)
{
 unsigned i=channel(pin);(void)mctrl;if(i==3)return;
 pin_request[i]=request;pin_applied[i]=actual;pin_vals[i][0]=a;pin_vals[i][1]=b;pin_vals[i][2]=c;pin_vals[i][3]=d;pwm_known|=1U<<i;
 if(current){++phases[current].pwm_calls;if(actual)++phases[current].pwm_nonzero;}
}
/* Same phase-local I/O hooks as B2, with bounded counters only. */
uint32_t b2_io_begin(void){return DWT->CYCCNT;}
void b2_io_end(unsigned kind,uint32_t begin,uint32_t bytes,int okay)
{
 uint32_t elapsed=DWT->CYCCNT-begin,mask=__get_PRIMASK();(void)kind;(void)bytes;__disable_irq();
 if(current){phase_t *p=&phases[current];++p->io_calls;if(!okay)++p->io_errors;if(elapsed>p->io_cycles_max)p->io_cycles_max=elapsed;}
 __set_PRIMASK(mask);
}
void joint_report(unsigned phase)
{
 phase_t p;uint32_t mask=__get_PRIMASK();__disable_irq();p=phases[phase];__set_PRIMASK(mask);
 printf("JOINT PHASE=%u begin_ms=%lu end_ms=%lu control=%lu gap_max_ms=%lu service_budget_ms=10 service_budget_misses=%lu control_cycles_max=%lu samples=%lu sample_min_ms=%lu sample_max_ms=%lu sample_not10=%lu pi_calls=%lu pi_consumed=%lu pi_cycles_max=%lu mask_cycles_max=%lu faults=%lu frames=%lu frame_age_max_ms=%lu c26_mask=%lu pwm_nonzero=%lu io_calls=%lu io_errors=%lu io_cycles_max=%lu accepted_delta=%llu queue_drop_delta=%llu pool_drop_delta=%llu ambiguous_delta=%llu queue_peak_cumulative=%lu hz=%lu\r\n",
 phase,(unsigned long)p.begin,(unsigned long)p.end,(unsigned long)p.control_calls,(unsigned long)p.control_gap_max,(unsigned long)p.control_budget_miss,(unsigned long)p.control_cycles_max,(unsigned long)p.sample_count,(unsigned long)(p.sample_gap_min==~0U?0:p.sample_gap_min),(unsigned long)p.sample_gap_max,(unsigned long)p.sample_not_10ms,(unsigned long)p.pi_calls,(unsigned long)p.pi_consumed,(unsigned long)p.pi_cycles_max,(unsigned long)p.mask_cycles_max,(unsigned long)p.pi_faults,(unsigned long)p.frames,(unsigned long)p.frame_age_max,(unsigned long)p.c26_levels,(unsigned long)p.pwm_nonzero,(unsigned long)p.io_calls,(unsigned long)p.io_errors,(unsigned long)p.io_cycles_max,
 (unsigned long long)(p.rec_end.accepted-p.rec_begin.accepted),(unsigned long long)(p.rec_end.queue_full-p.rec_begin.queue_full),(unsigned long long)(p.rec_end.pool_full-p.rec_begin.pool_full),(unsigned long long)(p.rec_end.ambiguous-p.rec_begin.ambiguous),(unsigned long)p.rec_end.queue_high_water,(unsigned long)SystemCoreClock);
 for(unsigned i=0;i<3;++i)printf("JOINT PWM channel=%u known=%lu request=%lu applied=%lu val2=%lu val3=%lu val4=%lu val5=%lu\r\n",i,(unsigned long)((pwm_known>>i)&1U),(unsigned long)pin_request[i],(unsigned long)pin_applied[i],(unsigned long)pin_vals[i][0],(unsigned long)pin_vals[i][1],(unsigned long)pin_vals[i][2],(unsigned long)pin_vals[i][3]);
 printf("JOINT QUEUE phase=%u last=%lu max=%lu observations=%lu capacity=64\r\n",phase,(unsigned long)p.queue_last,(unsigned long)p.queue_max,(unsigned long)p.queue_observations);
}
