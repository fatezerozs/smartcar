/* Private stationary adapter: original C26 controller owns all state/PI/output.
 * Recorder requests never grant or remove C26 permission. Physical PWM is
 * independently clamped by both private PWM driver entry points.
 */
#include "main.h"
#include "joint.h"
#include "recorder_runtime.h"
#include "app_mode.h"
#include "vision_drive.h"
#include "track_bayes.h"
#include "task.h"
#include <string.h>
typedef struct {vision_result_t result;uint32_t sequence,capture_ms,mode_epoch;} observation_t;
static observation_t latest,received;
static StaticQueue_t queue_storage;
static uint8_t queue_bytes[sizeof(observation_t)];
static QueueHandle_t observations;
AT_SDRAM_SECTION_ALIGN(static uint8_t binary[REC_BYTES],64);
static tb_filter_t track;
static tb_model_t model;
static tb_observation_t track_observation;
static volatile uint32_t mode_epoch=1;
volatile uint8_t recorder_stop_drive=1,recorder_stopped_drive=1,joint_stop_requested;
volatile uint32_t recorder_link_error_sequence;
void recorder_vision_init(void)
{
 vd_profile_t profile;vision_drive_get_profile(&profile);
 tb_default_model(&model);model.axis_near_x=profile.control.axis_near_x;
 model.axis_far_x=profile.control.axis_far_x;model.near_y=profile.control.near_y;
 model.far_y=profile.control.far_y;model.band_half_height=profile.control.band_half_height;
 (void)tb_init(&track,&model);vision_midterm_reset();
}
void recorder_publish_vision(const rec_frame_t *frame)
{
 static uint32_t last_ms,vision_epoch;uint32_t epoch=mode_epoch,now=timer_get(GPT_TIM_1),capture_ms=(uint32_t)(frame->timestamp_us/1000U);
 if(vision_epoch!=epoch){vision_epoch=epoch;vision_midterm_route_reset();tb_reset(&track);last_ms=0;}
 if(last_ms&&(uint32_t)(capture_ms-last_ms)>250U){vision_midterm_reset();tb_reset(&track);}
 tb_poll(&track,now);
 vision_midterm_route_enable((uint8_t)app_mode_allows(APP_MODE_DRIVE));
 vision_midterm_route_hint(tb_route_hint(&track,now));
 vision_midterm_process(frame->pixels,binary,&latest.result);
 if(track.configured){tb_extract(&track.model,binary,&latest.result,&track_observation);(void)tb_update(&track,&track_observation,(uint32_t)frame->sequence,capture_ms);}
 latest.sequence=(uint32_t)frame->sequence;latest.capture_ms=capture_ms;latest.mode_epoch=epoch;last_ms=capture_ms;
 configASSERT(xQueueOverwrite(observations,&latest)==pdPASS);
}
void joint_control_task(void *unused)
{
 (void)unused;
 for(;;){uint32_t now=timer_get(GPT_TIM_1);joint_control_begin(now);
  if(app_mode_take_change()){vd_route_switch_mode_stop();++mode_epoch;__DMB();}
  if(joint_stop_requested){joint_stop_requested=0;vision_drive_stop();}
  vision_drive_set_local_allowed(app_mode_allows(APP_MODE_DRIVE));
  vision_drive_poll(now);
  if(xQueueReceive(observations,&received,0)==pdTRUE&&received.mode_epoch==mode_epoch){
   vision_drive_observe(&received.result,received.sequence,received.capture_ms,timer_get(GPT_TIM_1));
   b3_vision(received.sequence,received.capture_ms);
  }
  recorder_stopped_drive=(uint8_t)(vision_drive_output()->state!=VC_RUNNING);
  joint_control_end(timer_get(GPT_TIM_1));vTaskDelay(1);
 }
}
void joint_control_prepare(void)
{
 observations=xQueueCreateStatic(1,sizeof(observation_t),queue_bytes,&queue_storage);configASSERT(observations);
 (void)app_mode_take_change();vision_drive_init(timer_get(GPT_TIM_1));
}
