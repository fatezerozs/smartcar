#define _CRT_SECURE_NO_WARNINGS
#include "vision_midterm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Layered preprocessing contract; reference remains the old mapping/deadband
 * baseline. Spatially uniform two-class scenes and all gray pairs stay equal.
 * Spatial illumination, noise and outliers use bounded row-local refinement:
 * validate them independently, never bless a mismatch by observing outputs.
 * Global statistics and gray previews remain equal for EVERY scene. Tracking
 * and overlays are excluded (tie rejection/edge continuity changed separately).
 */
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

/* Independent row oracle: sort samples, maximize the sum of squared class
 * sums divided by class size. Compare exact rational objectives using uint64_t,
 * rather than production's floating between-class score or histogram shortcuts.
 * For 188 8-bit samples cross-products are < 4e15 (well below UINT64_MAX).
 * Every split represents the inclusive threshold plateau [v[k-1],v[k]-1]. */
static int compare_gray(const void *a, const void *b)
{
    return (int)*(const uint8_t *)a - (int)*(const uint8_t *)b;
}

static int sorted_split(const uint8_t *sorted,int begin,int count)
{
    uint64_t total = 0, left = 0, best_num = 0, best_den = 1;
    int k, first = 0, last = 0;
    for (k = 0; k < count; ++k) total += sorted[begin+k];
    for (k = 1; k < count; ++k) {
        uint64_t right, num, den;
        left += sorted[begin+k - 1];
        if (sorted[begin+k - 1] == sorted[begin+k]) continue;
        right = total - left;
        num = left * left * (count - k) + right * right * k;
        den = (uint64_t)k * (count - k);
        if (num * best_den > best_num * den) {
            best_num = num; best_den = den;
            first = sorted[begin+k - 1]; last = sorted[begin+k] - 1;
        } else if (num * best_den == best_num * den) last = sorted[begin+k] - 1;
    }
    return (first + last) / 2;
}

static int row_oracle(const uint8_t *row, int global, int global_contrast)
{
    uint8_t sorted[VISION_WIDTH];int at=0,split=0;
    uint64_t middle_sum=0,white_sum=0;int middle_count,white_count;
    if(global_contrast<VISION_MIN_CONTRAST)return global;
    memcpy(sorted,row,sizeof(sorted));qsort(sorted,VISION_WIDTH,sizeof(sorted[0]),compare_gray);
    if(sorted[VISION_WIDTH*(VISION_GRAY_PERCENTILE_DENOMINATOR-1)/VISION_GRAY_PERCENTILE_DENOMINATOR-1]-
       sorted[VISION_WIDTH/VISION_GRAY_PERCENTILE_DENOMINATOR-1]<VISION_MIN_CONTRAST)return global;
    int first=sorted_split(sorted,0,VISION_WIDTH);
    for(int pass=0;pass<2;++pass){
        at=0;middle_sum=white_sum=0;
        while(at<VISION_WIDTH&&sorted[at]<=first)++at;
        if(VISION_WIDTH-at<2*VISION_MIN_WIDTH||sorted[VISION_WIDTH-1]-sorted[at]<VISION_MIN_CONTRAST)return first;
        int second=sorted_split(sorted,at,VISION_WIDTH-at);split=at;
        while(split<VISION_WIDTH&&sorted[split]<=second)middle_sum+=sorted[split++];
        for(int k=split;k<VISION_WIDTH;++k)white_sum+=sorted[k];
        middle_count=split-at;white_count=VISION_WIDTH-split;
        if(middle_count<VISION_MIN_WIDTH||white_count<VISION_MIN_WIDTH||
           white_sum*middle_count-middle_sum*white_count<(uint64_t)VISION_MIN_CONTRAST*middle_count*white_count||
           4*middle_sum*white_count<3*white_sum*middle_count)return first;
        /* Independently enumerate connected runs; the native implementation scans
         * outward from its seed instead. Only that run's midpoint may qualify. */
        int seed=(int)((VISION_WIDTH-1)*VISION_AXIS_NEAR_X),supported=0;
        for(int x=0;x<VISION_WIDTH;){
            if(row[x]<=first){++x;continue;}
            int left=x;while(x<VISION_WIDTH&&row[x]>first)++x;
            if(left<=seed&&seed<x){
                supported=row[(left+x-1)/2]>second;break;
            }
        }
        if(!supported)return first;
        first=second;
    }
    return first;
}
static int check_frame(int frame, int strict)
{
    int mode, x, y, adaptive = 0;
    unsigned int whites = 0;
    reference_process(images[0], binary_a, &result_a);
    vision_midterm_process(images[0], binary_b, &result_b);
    if (result_a.threshold != result_b.threshold || result_a.gray_low != result_b.gray_low ||
        result_a.gray_high != result_b.gray_high || result_a.contrast != result_b.contrast) {
        printf("FAIL: global statistics at frame %d\n", frame); return 0;
    }
    if (strict && (memcmp(binary_a, binary_b, sizeof(binary_a)) ||
        memcmp(result_a.row_threshold, result_b.row_threshold, sizeof(result_a.row_threshold)) ||
        result_a.white_percent != result_b.white_percent || result_a.adaptive_rows != result_b.adaptive_rows)) {
        printf("FAIL: strict compatibility at frame %d\n", frame); return 0;
    }
    /* Also apply the independent oracle to strict scenes: an identical bug in
     * both implementations must not silently satisfy the local contract. */
    for (y = 0; y < VISION_HEIGHT; ++y) {
        int in_roi = y >= VISION_ROI_TOP && y <= VISION_ROI_BOTTOM;
        const uint8_t *row = images[0] + y * VISION_WIDTH;
        int threshold = in_roi ? row_oracle(row, result_a.threshold, result_a.contrast) : result_a.threshold;
        if (result_b.row_threshold[y] != threshold) {
            printf("FAIL: row oracle frame %d y=%d expected=%d actual=%u\n",
                   frame, y, threshold, result_b.row_threshold[y]); return 0;
        }
        if (in_roi && threshold != result_a.threshold) ++adaptive;
        for (x = 0; x < VISION_WIDTH; ++x) {
            int vote = row[x] > threshold;
            if (x > 0 && x < VISION_WIDTH - 1)
                vote = (vote + (row[x - 1] > threshold) + (row[x + 1] > threshold)) >= 2;
            if (binary_b[y * VISION_WIDTH + x] != (vote ? 255 : 0)) {
                printf("FAIL: binary oracle frame %d x=%d y=%d\n", frame, x, y); return 0;
            }
            if (in_roi) whites += vote;
        }
    }
    if (result_b.adaptive_rows != adaptive || result_b.white_percent !=
        whites * 100 / ((VISION_ROI_BOTTOM - VISION_ROI_TOP + 1) * VISION_WIDTH)) {
        printf("FAIL: local summary at frame %d\n", frame); return 0;
    }
    for (mode = VISION_PREVIEW_GRAY; mode <= VISION_PREVIEW_BINARY; ++mode) {
        reference_preview(images[0], binary_a, &result_a, preview_a, (vision_preview_mode_t)mode);
        vision_midterm_preview(images[0], binary_b, &result_b, preview_b, (vision_preview_mode_t)mode);
        if ((strict || mode == VISION_PREVIEW_GRAY) && memcmp(preview_a, preview_b, sizeof(preview_a))) {
            printf("FAIL: compatible preview frame %d mode %d\n", frame, mode); return 0;
        }
        /* Changed binary previews must still exactly sample THEIR binary
         * buffer; do not compare two intentionally different segmentations. */
        for (y = 0; y < VISION_PREVIEW_HEIGHT; ++y)
            for (x = 0; x < VISION_PREVIEW_WIDTH; ++x) {
                int src = 2 * y * VISION_WIDTH + 2 * x, dst = y * VISION_PREVIEW_WIDTH + x;
                if (preview_a[dst] != (mode == VISION_PREVIEW_GRAY ? images[0][src] : binary_a[src]) ||
                    preview_b[dst] != (mode == VISION_PREVIEW_GRAY ? images[0][src] : binary_b[src])) {
                    printf("FAIL: preview sampling frame %d mode %d\n", frame, mode); return 0;
                }
            }
    }
    return 1;
}

/* Analytic fixture independent of both preprocessors: a 120-pixel-wide road
 * crosses a 33-row shadow without changing edges. Both levels are scaled by
 * 1/3 in the shadow. Exact two-class plateau midpoints are 79 and 25, and
 * every road pixel must remain connected through both shadow transitions. */
static int check_shadow(void)
{
    int x, y;
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = 0; x < VISION_WIDTH; ++x) {
            int value = x >= 34 && x <= 153 ? 149 : 10;
            images[0][y * VISION_WIDTH + x] = (uint8_t)(y >= 61 && y <= 93 ? value / 3 : value);
        }
    reference_reset(); vision_midterm_reset();
    if (!check_frame(-1, 0)) return 0;
    for (y = 0; y < VISION_HEIGHT; ++y) {
        if (y >= VISION_ROI_TOP && y <= VISION_ROI_BOTTOM) {
            int expected = y >= 61 && y <= 93 ? 25 : 79;
            if (result_b.row_threshold[y] != expected || result_b.row_flags[y] != 3 ||
                result_b.left[y] != 34 || result_b.right[y] != 153) {
                printf("FAIL: shadow threshold/measured edges y=%d\n", y); return 0;
            }
        }
        for (x = 0; x < VISION_WIDTH; ++x)
            if (binary_b[y * VISION_WIDTH + x] != (x >= 34 && x <= 153 ? 255 : 0)) {
                printf("FAIL: shadow road mask/connectivity x=%d y=%d\n", x, y); return 0;
            }
    }
    if (!result_b.valid || result_b.adaptive_rows != VISION_ROI_BOTTOM - VISION_ROI_TOP + 1) {
        printf("FAIL: shadow validity/adaptation\n"); return 0;
    }
    return 1;
}

int main(void)
{
    int frame, low, high, x, y, compared = 0, strict_count = 0;
    reference_reset(); vision_midterm_reset();
    for (frame = 0; frame < 6000; ++frame) {
        /* Input-defined compatibility domain: no shadow, random texture or
         * inverted outliers. All other generated scenes use the local oracle,
         * including noisy frames whose binary pixels actually change. */
        int strict = frame % 4 != 0 && frame % 7 != 0 && frame % 17 != 0;
        generate(images[0], frame);
        if (frame % 37 == 0) { reference_reset(); vision_midterm_reset(); }
        if (!check_frame(compared++, strict)) return 1;
        strict_count += strict;
    }
    /* Every unordered pair of 8-bit intensities, including constants. Strict
     * compatibility checks every Otsu plateau and low-contrast transition. */
    for (low = 0; low < 256; ++low)
        for (high = low; high < 256; ++high) {
            for (y = 0; y < VISION_HEIGHT; ++y)
                for (x = 0; x < VISION_WIDTH; ++x)
                    images[0][y * VISION_WIDTH + x] = (uint8_t)(x >= 34 && x <= 153 ? high : low);
            reference_reset(); vision_midterm_reset();
            if (!check_frame(compared++, 1)) return 1;
            ++strict_count;
        }
    if (!check_shadow()) return 1;
    printf("PREPROCESS CONTRACT: %d frames, %d strict baseline, %d local semantics\n",
           compared, strict_count, compared - strict_count);
    printf("All frames: global statistics, exact row oracle, binary votes, adaptive/white summaries, preview sampling.\n");
    printf("All 32896 gray pairs strictly equal; analytic shadow thresholds/edges/connectivity passed.\n");
    printf("Tracking and overlay excluded except measured edges in the analytic shadow fixture.\n");
    return 0;
}
