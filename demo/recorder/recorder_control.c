#include "main.h"
#include "recorder_runtime.h"
#include "vision_drive.h"
#include "vision_midterm.h"
#include "midterm_boot.h"
#include "app_mode.h"
#include "task.h"
#include "recorder_diagnostics.h"
#include "vision_speed.h"
#include <string.h>
typedef struct {vision_result_t result;uint64_t sequence,timestamp_us;} observation_t;
static observation_t latest,received;
static uint8_t queue_bytes[sizeof(observation_t)];
static StaticQueue_t queue_storage;
static QueueHandle_t observations;
AT_SDRAM_SECTION_ALIGN(static uint8_t binary[REC_BYTES],64);
volatile uint8_t recorder_stop_drive,recorder_stopped_drive=1;
volatile uint32_t recorder_link_error_sequence;
static rec_drive_status_t drive_status;
static uint32_t last_control_ms,seen_link_errors;
static uint64_t last_vision_us;
void recorder_drive_status(rec_drive_status_t *out)
{taskENTER_CRITICAL();*out=drive_status;taskEXIT_CRITICAL();}
extern void recorder_control_gap(uint32_t gap);
void recorder_vision_init(void){last_vision_us=0;vision_midterm_reset();vision_midterm_route_enable(1);}
void recorder_publish_vision(const rec_frame_t *frame)
{
    if(last_vision_us && frame->timestamp_us-last_vision_us>250000U) {
        vision_midterm_reset();vision_midterm_route_enable(1);
    }
    vision_midterm_process(frame->pixels,binary,&latest.result);
    latest.sequence=frame->sequence;latest.timestamp_us=frame->timestamp_us;
    last_vision_us=frame->timestamp_us;
    configASSERT(xQueueOverwrite(observations,&latest)==pdPASS);
}
void recorder_control_init(uint32_t now)
{
    last_control_ms=now;seen_link_errors=0;memset(&drive_status,0,sizeof(drive_status));
    observations=xQueueCreateStatic(1,sizeof(observation_t),queue_bytes,&queue_storage);
    configASSERT(observations);
    (void)app_mode_take_change();vision_drive_init(now);midterm_boot_init(0,now);
    recorder_boot_stage(70U);
}
void recorder_control_poll(uint32_t now)
{
    recorder_control_gap(now-last_control_ms);last_control_ms=now;
    uint32_t link_errors=recorder_link_error_sequence;
    if(link_errors!=seen_link_errors) {
        /* Before motion: restart the full fresh-track window. During
         * motion: stop and latch. Control alone owns either transition. */
        midterm_boot_link_error(now);seen_link_errors=link_errors;
    }
    if(recorder_stop_drive) {
        /* Sticky until MCU reset: never replay an old arm after export. */
        midterm_boot_cancel(now);vision_drive_stop();
    }
    vision_drive_set_local_allowed(!recorder_stop_drive && app_mode_allows(APP_MODE_DRIVE));
    if(xQueueReceive(observations,&received,0)==pdTRUE && !recorder_stop_drive) {
        vision_drive_observe(&received.result,(uint32_t)received.sequence,
            (uint32_t)(received.timestamp_us/1000U),now);
        ++drive_status.frames;drive_status.capture_ms=(uint32_t)(received.timestamp_us/1000U);
    }
    vision_drive_poll(now);
    if(!recorder_stop_drive)midterm_boot_poll(now);
    recorder_stopped_drive=(uint8_t)(vision_drive_output()->state!=VC_RUNNING);
    {
        vd_speed_info_t speed;const vc_output_t *out=vision_drive_output();
        vision_drive_speed_get(&speed);drive_status.ms=now;++drive_status.iterations;
        drive_status.boot=(uint8_t)midterm_boot_state();drive_status.state=(uint8_t)out->state;
        drive_status.observation_valid=out->observation_valid;drive_status.far_valid=out->far_valid;
        drive_status.recovering=out->recovering;drive_status.search_source=out->search_source;
        drive_status.faults=out->faults|speed.faults;drive_status.speed_samples=speed.samples;
        drive_status.link_errors_seen=seen_link_errors;
        drive_status.speed_fault=speed.fault_detail;
        drive_status.target_right=(uint16_t)speed.target_right;drive_status.target_left=(uint16_t)speed.target_left;
        drive_status.pwm_right=speed.pwm_right;drive_status.pwm_left=speed.pwm_left;
    }
}
void recorder_control_task(void *unused)
{
    TickType_t wake=xTaskGetTickCount();(void)unused;
    recorder_control_init(timer_get(GPT_TIM_1));
    for(;;) {
        recorder_control_poll(timer_get(GPT_TIM_1));
        /* Delay relative to NOW after an overrun; never run a catch-up burst. */
        wake=xTaskGetTickCount();(void)xTaskDelayUntil(&wake,1);
    }
}
