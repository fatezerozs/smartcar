#ifndef VISION_MIDTERM_H
#define VISION_MIDTERM_H

#include <stdint.h>
#include "vision_profile.h"

/* 中期视觉：沿用原工程白底黑边约定，仅输出直道/弯道的边界和偏差。
 * 不调用屏幕、编码器、电机和舵机；必须先检查 result.valid 再使用偏差。
 * 原 Camera.c / Island.c 保留，便于与队友版本逐项对照。
 * White road with dark boundaries. Pixel coordinates: x right, y down.
 * Midterm route: straights and bends only. No actuator access in this module. */
#define VISION_WIDTH 188
#define VISION_HEIGHT 120
#define VISION_ROI_TOP (VISION_HEIGHT * VISION_ROI_TOP_PERMILLE / 1000)
#define VISION_ROI_BOTTOM (VISION_HEIGHT * VISION_ROI_BOTTOM_PERMILLE / 1000)
#define VISION_SEED_TOP (VISION_HEIGHT * VISION_SEED_TOP_PERMILLE / 1000)
#define VISION_MIN_WIDTH (VISION_WIDTH * VISION_MIN_WIDTH_PERMILLE / 1000)
#define VISION_ERROR_TOP (VISION_HEIGHT * VISION_ERROR_TOP_PERMILLE / 1000)
#define VISION_ERROR_BOTTOM (VISION_HEIGHT * VISION_ERROR_BOTTOM_PERMILLE / 1000)
#define VISION_NEAR_TOP (VISION_HEIGHT * VISION_NEAR_TOP_PERMILLE / 1000)
#define VISION_MIDDLE_TOP (VISION_HEIGHT * VISION_MIDDLE_TOP_PERMILLE / 1000)
#define VISION_BEND_NEAR_TOP (VISION_HEIGHT * VISION_BEND_NEAR_TOP_PERMILLE / 1000)
#define VISION_BEND_NEAR_BOTTOM (VISION_HEIGHT * VISION_BEND_NEAR_BOTTOM_PERMILLE / 1000)
#define VISION_BEND_FAR_TOP (VISION_HEIGHT * VISION_BEND_FAR_TOP_PERMILLE / 1000)
#define VISION_BEND_FAR_BOTTOM (VISION_HEIGHT * VISION_BEND_FAR_BOTTOM_PERMILLE / 1000)
#define VISION_INVALID_X 255
#define VISION_PREVIEW_WIDTH (VISION_WIDTH / 2)
#define VISION_PREVIEW_HEIGHT (VISION_HEIGHT / 2)

#if VISION_ROI_TOP < 0 || VISION_ROI_BOTTOM >= VISION_HEIGHT || VISION_ROI_TOP >= VISION_ROI_BOTTOM || VISION_MIN_WIDTH < 2
#error "Invalid normalized vision geometry profile."
#endif

typedef enum {
    VISION_NO_LANE = 0,
    VISION_TRACK = 1,
    VISION_LOW_CONTRAST = 2
} vision_status_t;

typedef enum {
    VISION_PREVIEW_GRAY = 0,
    VISION_PREVIEW_BINARY,
    VISION_PREVIEW_OVERLAY
} vision_preview_mode_t;

typedef struct {
    uint8_t left[VISION_HEIGHT];
    uint8_t right[VISION_HEIGHT];
    uint8_t center[VISION_HEIGHT];
    /* 0: invalid, 1: left measured/right inferred, 2: right measured/left inferred,
     * 3: both measured. Inferred rows are never reported as two-edge detections. */
    uint8_t row_flags[VISION_HEIGHT];
    uint8_t row_threshold[VISION_HEIGHT]; /* Actual threshold used by each row. */
    uint8_t threshold, contrast, gray_low, gray_high, white_percent;
    uint8_t measured_rows, inferred_rows, confidence, valid;
    uint8_t adaptive_rows; /* ROI rows whose threshold differs from global Otsu. */
    uint8_t ambiguous_rows; /* Examined rows rejected for an exact best-score tie. */
    vision_status_t status;
    float error_px;       /* Positive: road center is right of image center. */
    float raw_error_px;
    float bend_px;        /* Measured far minus near center; positive: right bend. */
    uint8_t bend_valid;
} vision_result_t;

void vision_midterm_reset(void);
void vision_midterm_process(const uint8_t *gray, uint8_t *binary, vision_result_t *out);
/* Input gray/binary buffers must be separate, each VISION_WIDTH*VISION_HEIGHT.
 * Preview output needs VISION_PREVIEW_WIDTH*VISION_PREVIEW_HEIGHT bytes.
 * Overlay draws only accepted results, never a rejected partial detection. */
void vision_midterm_preview(const uint8_t *gray, const uint8_t *binary,
                            const vision_result_t *result, uint8_t *preview,
                            vision_preview_mode_t mode);
const char *vision_midterm_status_name(vision_status_t status);

#endif
