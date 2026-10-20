#ifndef VISION_TELEMETRY_H
#define VISION_TELEMETRY_H
#include "vision_midterm.h"
#include "track_bayes.h"
typedef struct {
    const vision_result_t *lane;
    const tb_filter_t *track;
    const tb_observation_t *observation;
    uint32_t frames,last_frame,fps_x10,dropped_copies,process_ms,history_resets,now;
    uint8_t timeout_reset,midterm_mode;
} vision_report_t;
void vision_telemetry_help(void);
void vision_telemetry_report(const vision_report_t *report);
void vision_telemetry_preview(char mode,const uint8_t *gray,const uint8_t *binary,const vision_result_t *result);
#endif
