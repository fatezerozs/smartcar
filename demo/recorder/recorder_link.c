#include "main.h"
#include "recorder_runtime.h"
#include "app_link.h"
#include "task.h"
#include "recorder_diagnostics.h"
#include "vision_profile.h"
#include "recorder_sd.h"
#include <stdio.h>
#include <string.h>
extern volatile uint8_t recorder_finish_requested;
static char line[2][64];
static unsigned used[2],discard[2];
static uint32_t last_status;
static void status(unsigned channel)
{
    rec_stats_t s;char out[512];int n;recorder_stats(&s);
    n=snprintf(out,sizeof(out),"[RECORDER_TRAIN_1] state=%u fs_error=%u dir=%s recording=%u written=%lu synced=%lu queue_peak=%lu queue_drop=%llu pool_drop=%llu ambiguous=%llu ctrl_gap_ms=%lu copy_us=%lu vision_ms=%lu stopped_command=%u safe_to_remove=%u\r\n",
        recorder_storage_state,recorder_storage_error,recorder_session_directory(),recorder_recording,(unsigned long)recorder_written,(unsigned long)recorder_synced,
        (unsigned long)s.queue_high_water,(unsigned long long)s.queue_full,(unsigned long long)s.pool_full,
        (unsigned long long)s.ambiguous,(unsigned long)s.control_gap_max_ms,(unsigned long)s.capture_copy_max_us,
        (unsigned long)s.vision_max_ms,recorder_stopped_drive,recorder_safe_to_remove);
    if(n>0 && n<(int)sizeof(out))(void)app_link_send(channel,(const uint8_t *)out,(uint32_t)n);
    {
        rec_drive_status_t d;recorder_drive_status(&d);
        n=snprintf(out,sizeof(out),"[REC_DRIVE] rev=5 ms=%lu iter=%lu camera=%u qualify_ms=%u reviewed=%u boot=%u state=%u frame=%lu age_ms=%lu valid=%u far=%u recovering=%u source=%u samples=%lu faults=%lu speed_fault=%u target_R/L=%u/%u pwm_R/L=%u/%u link_errors_seen=%lu\r\n",
            (unsigned long)d.ms,(unsigned long)d.iterations,recorder_camera_ok,(unsigned)MIDTERM_BOOT_DELAY_MS,(unsigned)MIDTERM_VISUAL_REVIEWED,d.boot,d.state,(unsigned long)d.frames,
            (unsigned long)(d.ms-d.capture_ms),d.observation_valid,d.far_valid,d.recovering,d.search_source,(unsigned long)d.speed_samples,
            (unsigned long)d.faults,d.speed_fault,d.target_right,d.target_left,d.pwm_right,d.pwm_left,(unsigned long)d.link_errors_seen);
        if(n>0 && n<(int)sizeof(out))(void)app_link_send(channel,(const uint8_t *)out,(uint32_t)n);
    }
    {
        app_link_stats_t usb,radio;app_link_stats(APP_LINK_USB,&usb);app_link_stats(APP_LINK_RADIO,&radio);
        n=snprintf(out,sizeof(out),"[REC_LINK] rx_usb=%lu err_usb=%lu bad_usb=%lu uart_err_usb=0x%08lx rx_radio=%lu err_radio=%lu bad_radio=%lu uart_err_radio=0x%08lx bad_word_radio=0x%08lx\r\n",
            (unsigned long)usb.rx_bytes,(unsigned long)usb.rx_errors,(unsigned long)usb.rx_corrupt,(unsigned long)usb.last_uart_error,
            (unsigned long)radio.rx_bytes,(unsigned long)radio.rx_errors,(unsigned long)radio.rx_corrupt,
            (unsigned long)radio.last_uart_error,(unsigned long)radio.last_bad_word);
        if(n>0 && n<(int)sizeof(out))(void)app_link_send(channel,(const uint8_t *)out,(uint32_t)n);
    }
    {
        rec_sd_status_t sd;recorder_sd_status(&sd);
        n=snprintf(out,sizeof(out),"[REC_SD] card=%u initialized=%u bus_bits=%u clock_hz=%lu configured_mv=3300 init_status=0x%08lx quarantined=%u host_caps=0x%08lx mux_uart_tx/rx=%u/%u mux_servo=%u\r\n",
            sd.present,sd.initialized,sd.bus_bits,(unsigned long)sd.clock_hz,(unsigned long)sd.init_status,
            sd.quarantined,(unsigned long)sd.host_capability,sd.mux_radio_tx,sd.mux_radio_rx,sd.mux_servo);
        if(n>0 && n<(int)sizeof(out))(void)app_link_send(channel,(const uint8_t *)out,(uint32_t)n);
    }
}
void recorder_link_init(void)
{
    memset(line,0,sizeof(line));memset(used,0,sizeof(used));memset(discard,0,sizeof(discard));last_status=0;
    recorder_boot_stage(80U);
}
void recorder_link_poll(void)
{
    unsigned ch;
    for(ch=0;ch<2;++ch) {
        uint8_t data[64];uint32_t i,n=app_link_read(ch,data,sizeof(data));
        if(app_link_take_error(ch)) {
            uint8_t pending[APP_LINK_RX_SIZE];
            uint32_t tail=app_link_read(ch,pending,sizeof(pending));
            /* Preserve cancellation if a command might have been damaged.
             * Pure rejected UART words carry no valid operator input. */
            if(n || tail || used[ch]){recorder_stop_drive=1;discard[ch]=1;}
            ++recorder_link_error_sequence;used[ch]=0;
        }
        for(i=0;i<n;++i) {
            uint8_t c=data[i];
            if(c=='!' || c==3){recorder_stop_drive=1;used[ch]=0;discard[ch]=1;continue;}
            if(c=='\n' || c=='\r') {
                if(!discard[ch]) {
                    line[ch][used[ch]]=0;
                    if(!strcmp(line[ch],"@rec stop"))recorder_finish_requested=1;
                    else if(!strcmp(line[ch],"@export")) {
                        /* Export grants a no-restart latch via Control, then
                         * Storage independently closes/unmounts its files. */
                        recorder_stop_drive=1;recorder_finish_requested=1;
                    }
                    else if(!strcmp(line[ch],"@rec status"))status(ch);
                }
                used[ch]=discard[ch]=0;
            }else if(!discard[ch]) {
                if(used[ch]+1U<sizeof(line[ch]))line[ch][used[ch]++]=(char)c;
                else {used[ch]=0;discard[ch]=1;}
            }
        }
    }
    if((uint32_t)(xTaskGetTickCount()-last_status)>=pdMS_TO_TICKS(1000U)) {
        status(APP_LINK_USB);status(APP_LINK_RADIO);last_status=xTaskGetTickCount();
    }
    app_link_poll();
}
void recorder_link_task(void *unused)
{
    (void)unused;recorder_link_init();
    for(;;) {
        recorder_link_poll();vTaskDelay(pdMS_TO_TICKS(5));
    }
}
