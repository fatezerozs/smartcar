/* Real joint Control/Vision handoff + native C26/PI. Synthetic peripheral
 * counters and one-slot queue; no RTOS scheduling, SD or physical outputs. */
#include "../support/fixtures/speed_io.h"
#include "recorder_runtime.h"
#include "joint.h"
#include "task.h"
#include <setjmp.h>
#include <assert.h>
static jmp_buf done;
static rec_frame_t frame;
static unsigned phase,observed,run_first,run_second,stopped_interval,overwrites;
volatile uint8_t joint_writer_start;
static unsigned simulated_recording;
QueueHandle_t xQueueCreateStatic(UBaseType_t n,UBaseType_t size,uint8_t *bytes,StaticQueue_t *s)
{assert(n==1);s->bytes=bytes;s->size=size;s->full=0;return s;}
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *item)
{if(q->full)++overwrites;memcpy(q->bytes,item,q->size);q->full=1;return pdPASS;}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t wait)
{assert(!wait);if(!q->full)return pdFALSE;memcpy(item,q->bytes,q->size);q->full=0;return pdTRUE;}
void joint_control_begin(uint32_t ms){CHECK(ms==now);}
void joint_control_end(uint32_t ms)
{
 CHECK(ms==now);
 if(now<101)CHECK(!vision_drive_is_local());
 if(now>280&&now<490)run_first|=vision_drive_is_local();
 if(now>520&&now<790){CHECK(!vision_drive_is_local());++stopped_interval;}
 if(now>1180&&now<1390)run_second|=vision_drive_is_local();
 if(now>1440)CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
}
void b3_vision(uint32_t seq,uint32_t ms){CHECK(seq>0&&ms<=now);++observed;}
static void publish(void)
{
 ++frame.sequence;frame.timestamp_us=(uint64_t)now*1000U;
 recorder_publish_vision(&frame);
}
void vTaskDelay(TickType_t n)
{
 CHECK(n==1);++now;
 if(now==101)phase=0;
 if(now==501)joint_stop_requested=1;
 if(now==801)phase=1;
 if(now==1001)phase=0;
 if(now==1401)phase=1;
 if(now%10==0){app_mode_sample(phase,1,now);raw3=(uint16_t)(raw3-15);raw4=(uint16_t)(raw4+15);vision_drive_safety_tick();}
 if(now%20==0){publish();if(now==80)publish();}
 if(simulated_recording)joint_writer_start=(uint8_t)(now>=200&&now<1300);
 if(now>=1600)longjmp(done,1);
}
int main(void)
{
 for(unsigned which=0;which<2;++which){
  now=mask=raw3=raw4=0;phase=1;observed=run_first=run_second=stopped_interval=overwrites=0;
  simulated_recording=which;memset(&frame,0,sizeof(frame));
  for(unsigned y=0;y<REC_HEIGHT;++y)for(unsigned x=0;x<REC_WIDTH;++x){unsigned width=40+y*3/4;frame.pixels[y*REC_WIDTH+x]=(uint8_t)(x>=94-width/2&&x<=94+width/2?210:30);}
  app_mode_init(1,1,0);joint_control_prepare();recorder_vision_init();
  if(!setjmp(done))joint_control_task(NULL);
  CHECK(run_first&&run_second&&observed>50&&stopped_interval>200&&overwrites>=1);
 }
 puts("JOINT CONTROL PASS: native C26 OFF/ON, STOP, OFF rearm, latest-slot overwrite and mode epochs; simulated IO only, not RTOS/SD validation");return 0;
}
