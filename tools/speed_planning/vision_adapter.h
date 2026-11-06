#ifndef SMARTCAR_HOST_PREVIEW_ADAPTER_H
#define SMARTCAR_HOST_PREVIEW_ADAPTER_H
#include "speed_planner.h"
#include "vision_midterm.h"
/* Native result reuse, no second image detector. Explicit sample rows descend
 * from near to far. Single-edge modeled/partial rows never become paired proof.
 * homography maps image pixels to ground metres. NULL => normalized geometry,
 * which sp_plan rejects. Its data provenance must be recorded by the caller. */
int sp_vision_preview(sp_geometry_history_t *history,const vision_result_t *result,
    const uint8_t *rows,unsigned count,unsigned min_confidence,const float *homography,
    uint32_t sequence,uint32_t capture_ms,uint32_t now_ms,uint32_t max_age_ms,
    float max_step,sp_preview_t *out);
#endif
