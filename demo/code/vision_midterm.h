#ifndef VISION_MIDTERM_H
#define VISION_MIDTERM_H

#include <stdint.h>

/* White road with dark boundaries. Pixel coordinates: x right, y down.
 * Midterm route: straights and bends only. No actuator access in this module. */
#define VISION_WIDTH 188
#define VISION_HEIGHT 120
#define VISION_ROI_TOP 24
#define VISION_ROI_BOTTOM 110
#define VISION_SEED_TOP 72
#define VISION_MIN_WIDTH 12
#define VISION_MIN_CONTRAST 18
#define VISION_MAX_INFER_AGE 5
#define VISION_INVALID_X 255

typedef enum {
    VISION_NO_LANE = 0,
    VISION_TRACK = 1,
    VISION_LOW_CONTRAST = 2
} vision_status_t;

typedef struct {
    uint8_t left[VISION_HEIGHT];
    uint8_t right[VISION_HEIGHT];
    uint8_t center[VISION_HEIGHT];
    /* 0: invalid, 1: left measured/right inferred, 2: right measured/left inferred,
     * 3: both measured. Inferred rows are never reported as two-edge detections. */
    uint8_t row_flags[VISION_HEIGHT];
    uint8_t threshold, contrast, gray_low, gray_high, white_percent;
    uint8_t measured_rows, inferred_rows, confidence, valid;
    vision_status_t status;
    float error_px;       /* Positive: road center is right of image center. */
    float raw_error_px;
    float bend_px;        /* Far center minus near center; positive: right bend. */
    uint8_t bend_valid;
} vision_result_t;

void vision_midterm_reset(void);
void vision_midterm_process(const uint8_t *gray, uint8_t *binary, vision_result_t *out);
const char *vision_midterm_status_name(vision_status_t status);

#endif
