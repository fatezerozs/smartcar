/* Real Recorder Control/Link iterations, vision, startup, guard and PI.
 * Simulates peripherals and the one-slot RTOS queue, not kernel scheduling/SD. */
#include "../support/fixtures/speed_io.h"
#include "recorder_runtime.h"
#include "recorder_diagnostics.h"
#include "recorder_sd.h"
#include "app_link.h"
#include "midterm_boot.h"
#include "task.h"
#include <assert.h>

volatile uint8_t recorder_camera_ok=1,recorder_recording,recorder_safe_to_remove;
volatile uint8_t recorder_storage_state,recorder_storage_error,recorder_finish_requested;
volatile uint32_t recorder_written,recorder_synced;
static rec_frame_t frame;
static unsigned scenario,first_running,saw_running,saw_pwm,saw_near_only,saw_recovery;
static char wire[2][32768];
static size_t wire_used[2];

QueueHandle_t xQueueCreateStatic(UBaseType_t count,UBaseType_t size,uint8_t *bytes,StaticQueue_t *storage)
{assert(count==1);storage->bytes=bytes;storage->size=size;storage->full=0;return storage;}
BaseType_t xQueueOverwrite(QueueHandle_t queue,const void *item)
{memcpy(queue->bytes,item,queue->size);queue->full=1;return pdPASS;}
BaseType_t xQueueReceive(QueueHandle_t queue,void *item,TickType_t wait)
{assert(!wait);if(!queue->full)return pdFALSE;memcpy(item,queue->bytes,queue->size);queue->full=0;return pdTRUE;}
TickType_t xTaskGetTickCount(void){return now;}
void vTaskDelay(TickType_t ticks){(void)ticks;}
BaseType_t xTaskDelayUntil(TickType_t *previous,TickType_t increment)
{*previous+=increment;return pdTRUE;}
void board_buzzer_set(int on){(void)on;}
void recorder_boot_stage(uint32_t stage){assert(stage==70 || stage==80);}
void recorder_control_gap(uint32_t gap){CHECK(gap<=1);}
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned channel,uint8_t byte)
{
    if(scenario==15 && channel==APP_LINK_RADIO)return 0;
    assert(wire_used[channel]+1<sizeof(wire[channel]));
    wire[channel][wire_used[channel]++]=(char)byte;return 1;
}
void recorder_stats(rec_stats_t *out){memset(out,0,sizeof(*out));out->control_gap_max_ms=1;}
const char *recorder_session_directory(void){return "0:/SYNTHETIC";}
void recorder_sd_status(rec_sd_status_t *out)
{memset(out,0,sizeof(*out));out->present=out->initialized=1;out->bus_bits=1;out->clock_hz=RECORDER_SD_MAX_HZ;out->mux_radio_tx=out->mux_radio_rx=2;out->mux_servo=1;}
static void inject(const char *text)
{while(*text)app_link_rx_isr(APP_LINK_RADIO,(uint8_t)*text++);}
static void corrupt(unsigned channel)
{app_link_reject_rx_isr(channel,0x2000U);app_link_uart_error_isr(channel,0x20000U);}
static void publish(void)
{
    int valid=scenario!=14 && !(scenario==8 && now<900U);
    if((scenario==16 && now==1600U) ||
       ((scenario==17 || scenario==20 || scenario==21) && now>=1600U && now<1800U) ||
       (scenario==18 && now>=1600U))valid=0;
    if(scenario==10 && now>=1600U)return;
    for(unsigned y=0;y<REC_HEIGHT;++y)for(unsigned x=0;x<REC_WIDTH;++x) {
        unsigned width=40U+y*3U/4U;
        int visible=valid && !(scenario==19 && now>=1600U && y<85U);
        frame.pixels[y*REC_WIDTH+x]=(uint8_t)(visible && x>=94U-width/2U && x<=94U+width/2U?210:30);
    }
    if(scenario!=9 || !frame.sequence){++frame.sequence;frame.timestamp_us=(uint64_t)now*1000U;}
    recorder_publish_vision(&frame);
}
static void events(void)
{
    if(scenario==1 && now==500U)corrupt(APP_LINK_USB);
    if(scenario==1 && now==900U)corrupt(APP_LINK_RADIO);
    if(scenario==2 && now>=500U && now%100U==0)corrupt(APP_LINK_RADIO);
    if(scenario==3 && now==500U)inject("!");
    if(scenario==4 && now==1600U)corrupt(APP_LINK_RADIO);
    if(scenario==5 && (now==300U || now==1300U))inject("@rec sta");
    if(scenario==5 && (now==500U || now==1500U))inject("tus\n");
    if(scenario==6 && now==500U)inject("@export\n");
    if(scenario==7 && now==500U)inject("@rec stop\n");
    if(scenario==11 && now==500U){corrupt(APP_LINK_RADIO);inject("!");}
    if(scenario==12 && now==300U)inject("@exp");
    if(scenario==12 && now==500U)corrupt(APP_LINK_RADIO);
    if(scenario==13 && now==500U)recorder_storage_state=5; /* Existing storage isolation. */
    if(scenario==20 && now==1700U)inject("!");
    if(scenario==21 && now==1700U)inject("@export\n");
}
static void run_case(unsigned which)
{
    rec_drive_status_t d;
    scenario=which;now=mask=raw3=raw4=0;first_running=saw_running=saw_pwm=saw_near_only=saw_recovery=0;
    memset(pwm,0,sizeof(pwm));memset(&frame,0,sizeof(frame));
    memset(wire,0,sizeof(wire));memset(wire_used,0,sizeof(wire_used));
    recorder_stop_drive=recorder_finish_requested=0;recorder_link_error_sequence=0;recorder_stopped_drive=1;
    recorder_storage_state=2;recorder_recording=1;recorder_written=recorder_synced=0;
    app_mode_init(1,1,0);app_link_init();recorder_vision_init();recorder_control_init(0);recorder_link_init();
    for(now=1;now<=2600U;++now) {
        if(now%10U==0){raw3=(uint16_t)(raw3-2U);raw4=(uint16_t)(raw4+2U);vision_drive_safety_tick();}
        events();
        if(now%5U==0)recorder_link_poll();
        if(now%20U==0)publish();
        recorder_control_poll(now);
        recorder_drive_status(&d);
        if(d.state==VC_RUNNING && d.observation_valid && !d.far_valid)saw_near_only=1;
        if(d.state==VC_RUNNING && d.recovering)saw_recovery=1;
        if(scenario==17 && now>=1600U && now<1800U) {
            CHECK(d.state==VC_RUNNING && d.boot==MT_BOOT_RUNNING && d.recovering);
            CHECK(!pwm[0] && !pwm[1]); /* No lane evidence never powers blind motion. */
        }
        if(vision_drive_is_local()) {
            if(!saw_running)first_running=now;
            saw_running=1;CHECK(now>=MIDTERM_BOOT_DELAY_MS);
        }
        if(pwm[0] || pwm[1])saw_pwm=1;
    }
    recorder_drive_status(&d);
#if MIDTERM_VISUAL_REVIEWED
    if(which==0 || which==1 || which==5 || which==7 || which==8 || which==13 || which==15 ||
       which==16 || which==17) {
        CHECK(saw_running && saw_pwm && vision_drive_is_local());
        if(which==1){CHECK(first_running>=1900U && d.link_errors_seen==2);}
        if(which==8)CHECK(first_running>=1900U);
        if(which==0)CHECK(first_running<1300U);
        if(which==5)CHECK(strstr(wire[1],"[REC_DRIVE] rev=5") && !recorder_stop_drive);
        if(which==7)CHECK(recorder_finish_requested && !recorder_stop_drive);
        if(which==16 || which==17)CHECK(saw_recovery && !d.recovering && !d.faults);
    } else if(which==2 || which==9 || which==14) {
        CHECK(!saw_running && !saw_pwm && d.boot==MT_BOOT_WAIT);
    } else {
        CHECK(d.boot==MT_BOOT_STOPPED && !pwm[0] && !pwm[1]);
        CHECK(saw_running==(which==4 || which==10 || which==18 || which==19 || which==20 || which==21));
        if(which==19){
            /* Preserve the near-only input. Before MPC budgets are integrated,
             * current shared C31 protection waits at zero then latches. */
            CHECK(saw_near_only&&saw_recovery&&(d.faults&VC_FAULT_VISION));
            CHECK(!d.target_right&&!d.target_left&&!d.pwm_right&&!d.pwm_left);
        }
        if(which==6 || which==21)CHECK(recorder_finish_requested && recorder_stop_drive);
        if(which==18)CHECK(saw_recovery && d.faults==VC_FAULT_VISION);
    }
    if(which==1) {
        app_link_stats_t s;app_link_stats(APP_LINK_RADIO,&s);
        CHECK(s.rx_bytes==0 && s.rx_corrupt==1);
    }
    if(which==0)CHECK(strstr(wire[1],"qualify_ms=1000 reviewed=1") && strstr(wire[1],"[REC_LINK]"));
#else
    CHECK(!saw_running && !saw_pwm && d.boot==MT_BOOT_BLOCKED);
#endif
}
int main(void)
{
    CHECK(MIDTERM_BOOT_DELAY_MS==1000U);
#if MIDTERM_VISUAL_REVIEWED
    for(unsigned i=0;i<22U;++i)run_case(i);
#else
    run_case(0);
#endif
    printf("RECORDER STARTUP PASS: review=%d, %u checks; real Control/Link/vision/PI, mocked RTOS/peripherals; no hardware/SD timing claim.\n",MIDTERM_VISUAL_REVIEWED,checks);
    return 0;
}
