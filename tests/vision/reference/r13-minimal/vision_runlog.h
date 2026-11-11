#ifndef VISION_RUNLOG_H
#define VISION_RUNLOG_H
#include "vision_control.h"
#include "vision_speed.h"
#ifndef VISION_RUNLOG_ENABLE
#define VISION_RUNLOG_ENABLE 0
#endif
#ifndef RUNLOG_CAPACITY
#define RUNLOG_CAPACITY (8U*1024U*1024U)
#endif
/* DTCM debugger descriptor: twelve little-endian u32, filename[16], build[40].
 * sd_state: 0 not attempted, 1 writing, 2 synchronized/closed, 3 failed.
 * recording: 0 parked/cache published, 1 ON, 2 OFF awaiting publication.
 * Buffer remains available after media failure. No SD call is made while ON. */
typedef struct {
    uint32_t magic,format,address,capacity,used,records,dropped,run;
    uint32_t recording,flushed,sd_state,sd_error;
    char filename[16],build[40];
} runlog_status_t;
extern volatile runlog_status_t g_runlog;
uint32_t runlog_crc32(const void *data,uint32_t length);
void runlog_init(void);
void runlog_poll(uint32_t now,int enabled,int stopped);
void runlog_capture(uint32_t now,uint32_t frame,const vision_result_t *lane,
                    const uint8_t *gray,const vc_output_t *control,const vd_speed_info_t *speed);
/* Board writes one bounded chunk only when called from the parked branch. */
void runlog_store_poll(const uint8_t *data,uint32_t length,volatile runlog_status_t *status);
void runlog_publish(const void *data,uint32_t length);
#endif
