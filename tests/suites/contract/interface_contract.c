/* Public data contract: field widths/geometry, not incidental struct padding. */
#include "vision_frame.h"
#include "vision_control.h"
#include "vision_planner.h"
#include "vision_planner_runtime.h"
#include <stddef.h>
_Static_assert(sizeof(((vision_frame_stamp_t *)0)->sequence) == sizeof(uint32_t), "Frame sequence is uint32");
_Static_assert(sizeof(((vision_frame_stamp_t *)0)->capture_ms) == sizeof(uint32_t), "Capture clock is uint32 milliseconds");
_Static_assert(sizeof(((vision_frame_stamp_t *)0)->time_valid) == 1, "Capture validity is explicit");
_Static_assert(sizeof(((vision_result_t *)0)->left) == VISION_HEIGHT, "One left boundary byte per row");
_Static_assert(sizeof(((vision_result_t *)0)->right) == VISION_HEIGHT, "One right boundary byte per row");
_Static_assert(sizeof(((vision_result_t *)0)->center) == VISION_HEIGHT, "One center byte per row");
_Static_assert(sizeof(((vision_result_t *)0)->row_flags) == VISION_HEIGHT, "Per-row validity is preserved");
_Static_assert(VISION_PREVIEW_WIDTH * 2 == VISION_WIDTH, "Preview is horizontally subsampled");
_Static_assert(VISION_PREVIEW_HEIGHT * 2 == VISION_HEIGHT, "Preview is vertically subsampled");
_Static_assert(VISION_PLANNER_ACTIVE == 0 && VISION_PLANNER_RUNTIME == 0,
               "Primary defaults leave the metric runtime disabled");
_Static_assert(sizeof(((vp_stamp_t *)0)->output_epoch) == sizeof(uint32_t), "Planner candidate binds output epoch");
_Static_assert(sizeof(((vdr_output_t *)0)->sample_sequence) == sizeof(uint32_t), "Output token binds encoder sample");
int main(void) { return 0; }
