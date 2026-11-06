#ifndef RECORDER_RUNTIME_H
#define RECORDER_RUNTIME_H
#include "recorder_core.h"
#include "fsl_csi.h"
#include "FreeRTOS.h"
#include "queue.h"
typedef struct {uint64_t sequence,timestamp_us;uint8_t reserved[48];uint8_t pixels[REC_ALIGNED_BYTES];} rec_frame_t;
typedef struct {
    uint64_t observed,ambiguous,pool_full,queue_full,accepted,vision_processed;
    uint64_t first_source,last_source,base_source;
    uint32_t capture_copy_max_us,vision_max_ms,control_gap_max_ms;
    uint32_t queue_high_water,storage_errors;
} rec_stats_t;
extern volatile uint8_t recorder_stop_drive;
extern volatile uint8_t recorder_stopped_drive;
/* Link is the single producer; Control consumes the epoch without clearing it.
 * UART faults never call the controller from another task or from an ISR. */
extern volatile uint32_t recorder_link_error_sequence;
extern volatile uint8_t recorder_recording;
extern volatile uint8_t recorder_safe_to_remove;
extern volatile uint8_t recorder_storage_state;
extern volatile uint8_t recorder_storage_error;
extern volatile uint32_t recorder_written,recorder_synced;
void recorder_runtime_init(void);
void recorder_capture_prepare(void);
void recorder_capture_start(csi_handle_t *unused);
void recorder_capture_irq(void);
void recorder_capture_task(void *unused);
void recorder_vision_task(void *unused);
void recorder_vision_init(void);
void recorder_publish_vision(const rec_frame_t *frame);
void recorder_control_task(void *unused);
void recorder_link_task(void *unused);
/* Task-owned initialization/iterations; the RTOS wrappers remain the only
 * production callers. Kept separate to exercise the real handoff on Host. */
void recorder_control_init(uint32_t now);
void recorder_control_poll(uint32_t now);
void recorder_link_init(void);
void recorder_link_poll(void);
void recorder_storage_task(void *unused);
void recorder_record_request(int enabled);
int recorder_record_ack(void);
int recorder_write_acquire(uint8_t *index,rec_frame_t **frame);
void recorder_write_release(uint8_t index);
void recorder_stats(rec_stats_t *out);
void recorder_panic(void);
const char *recorder_session_directory(void);
typedef struct {
    uint32_t ms,iterations,frames,capture_ms,speed_samples,faults,link_errors_seen;
    uint16_t target_right,target_left,pwm_right,pwm_left;
    uint8_t boot,state,observation_valid,far_valid,speed_fault,recovering,search_source;
} rec_drive_status_t;
void recorder_drive_status(rec_drive_status_t *out);
#endif
