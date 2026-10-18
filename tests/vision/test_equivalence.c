#define _CRT_SECURE_NO_WARNINGS
#include "vision_midterm.h"
#include <stdio.h>
#include <string.h>

/* Preprocessing-only equivalence against the pre-performance baseline.
 * Tracking now intentionally differs for ambiguous rows; do not claim whole
 * results/overlays are equivalent across that correctness change. */
void reference_reset(void);
void reference_process(const uint8_t *, uint8_t *, vision_result_t *);
void reference_preview(const uint8_t *, const uint8_t *, const vision_result_t *, uint8_t *, vision_preview_mode_t);
static uint8_t images[16][VISION_WIDTH * VISION_HEIGHT];
static uint8_t binary_a[VISION_WIDTH * VISION_HEIGHT], binary_b[sizeof(binary_a)];
static uint8_t preview_a[VISION_PREVIEW_WIDTH * VISION_PREVIEW_HEIGHT], preview_b[sizeof(preview_a)];
static vision_result_t result_a, result_b;
static unsigned int rng = 109;
static unsigned int random_u32(void) { rng = rng * 1664525U + 1013904223U; return rng; }

static void generate(uint8_t *image, int index)
{
    int x, y;
    int shift = (int)(random_u32() % 161) - 80;
    int width = 20 + (int)(random_u32() % 121);
    int low = (int)(random_u32() % 100), high = 110 + (int)(random_u32() % 146);
    for (y = 0; y < VISION_HEIGHT; ++y) {
        int center = 94 + shift + (index % 3 - 1) * (110-y) / 2;
        for (x = 0; x < VISION_WIDTH; ++x) {
            int value = x >= center-width/2 && x <= center+width/2 ? high : low;
            if (index % 4 == 0 && y > 60 && y < 94) value /= 3;
            if (index % 7 == 0) value = (int)(random_u32() >> 24);
            if (index % 11 == 0) value = y + 40;
            if (index % 13 == 0) value = 255;
            if (index % 17 == 0 && random_u32() % 97 == 0) value = 255 - value;
            image[y * VISION_WIDTH + x] = (uint8_t)value;
        }
    }
}

static int check_identical(int frame)
{
    int mode;
    reference_process(images[0], binary_a, &result_a);
    vision_midterm_process(images[0], binary_b, &result_b);
    if (memcmp(binary_a, binary_b, sizeof(binary_a)) ||
        memcmp(result_a.row_threshold, result_b.row_threshold, sizeof(result_a.row_threshold)) ||
        result_a.threshold != result_b.threshold || result_a.gray_low != result_b.gray_low ||
        result_a.gray_high != result_b.gray_high || result_a.contrast != result_b.contrast ||
        result_a.white_percent != result_b.white_percent || result_a.adaptive_rows != result_b.adaptive_rows) {
        printf("FAIL: preprocessing mismatch at frame %d\n", frame); return 0;
    }
    for (mode = VISION_PREVIEW_GRAY; mode <= VISION_PREVIEW_BINARY; ++mode) {
        reference_preview(images[0], binary_a, &result_a, preview_a, (vision_preview_mode_t)mode);
        vision_midterm_preview(images[0], binary_b, &result_b, preview_b, (vision_preview_mode_t)mode);
        if (memcmp(preview_a, preview_b, sizeof(preview_a))) {
            printf("FAIL: preview mismatch at frame %d, mode %d\n", frame, mode); return 0;
        }
    }
    return 1;
}

int main(void)
{
    int frame, low, high, x, y, compared = 0;
    reference_reset(); vision_midterm_reset();
    for (frame = 0; frame < 6000; ++frame) {
        generate(images[0], frame);
        if (frame % 37 == 0) { reference_reset(); vision_midterm_reset(); }
        if (!check_identical(compared++)) return 1;
    }
    /* Every unordered pair of 8-bit intensities, including constants. This
     * checks all two-class Otsu plateaus and low-contrast branch transitions. */
    for (low = 0; low < 256; ++low)
        for (high = low; high < 256; ++high) {
            for (y = 0; y < VISION_HEIGHT; ++y)
                for (x = 0; x < VISION_WIDTH; ++x)
                    images[0][y * VISION_WIDTH + x] = (uint8_t)(x >= 34 && x <= 153 ? high : low);
            reference_reset(); vision_midterm_reset();
            if (!check_identical(compared++)) return 1;
        }
    printf("PREPROCESS EQUIVALENCE: %d frames, binary/thresholds/gray+binary previews identical\n", compared);
    printf("Includes all 32896 unordered 8-bit gray pairs, including constants.\n");
    printf("Tracking and overlay intentionally excluded: tie rejection and edge continuity change behavior.\n");
    return 0;
}
