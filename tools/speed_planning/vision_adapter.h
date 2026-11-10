#ifndef SMARTCAR_HOST_PREVIEW_ADAPTER_H
#define SMARTCAR_HOST_PREVIEW_ADAPTER_H
#include "speed_planner.h"
#include "vision_midterm.h"
/* Native result reuse, no second image detector. Explicit sample rows descend
 * from near to far. Ordinary rows require paired evidence; route evidence may admit bounded
 * modeled/junction paths and entry guides, without changing row_flags.
 * homography maps image pixels to ground metres. NULL => normalized geometry,
 * which sp_plan rejects. Its data provenance must be recorded by the caller. */
/* Returns a checked image path x. Source validity, confidence, time and ground
 * domain are checked by the preview/frame layers, not this row helper. */
int sp_vision_path_sample(const vision_result_t *result,unsigned row,float *x);
int sp_vision_preview(sp_geometry_history_t *history,const vision_result_t *result,
    const uint8_t *rows,unsigned count,unsigned min_confidence,const float *homography,
    uint32_t sequence,uint32_t capture_ms,uint32_t now_ms,uint32_t max_age_ms,
    float max_step,sp_preview_t *out);
#endif
