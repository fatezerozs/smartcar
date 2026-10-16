#include "vision_midterm.h"
#include <string.h>

static uint16_t learned_width[VISION_HEIGHT];
static uint8_t width_age[VISION_HEIGHT];
static uint8_t previous_valid;
static float previous_error;
static float previous_seed;

static float vabs(float x) { return x < 0 ? -x : x; }

void vision_midterm_reset(void)
{
    memset(learned_width, 0, sizeof(learned_width));
    memset(width_age, 255, sizeof(width_age));
    previous_valid = 0;
    previous_error = 0;
    previous_seed = (VISION_WIDTH - 1) * 0.5f;
}

const char *vision_midterm_status_name(vision_status_t status)
{
    switch (status) {
    case VISION_TRACK: return "TRACK";
    case VISION_LOW_CONTRAST: return "LOW_CONTRAST";
    default: return "NO_LANE";
    }
}

static uint8_t otsu(const uint32_t *hist, uint32_t count)
{
    uint32_t total_sum = 0, back_count = 0, back_sum = 0;
    double best = -1.0;
    int first = 0, last = 0;
    int t;
    for (t = 0; t < 256; ++t) total_sum += hist[t] * (uint32_t)t;
    for (t = 0; t < 255; ++t) {
        double numerator, score;
        back_count += hist[t];
        back_sum += hist[t] * (uint32_t)t;
        if (back_count == 0 || back_count == count) continue;
        numerator = (double)total_sum * back_count - (double)back_sum * count;
        score = numerator * numerator / ((double)back_count * (count - back_count));
        if (score > best) { best = score; first = last = t; }
        else if (score == best) { last = t; }
    }
    /* Midpoint of an equally good threshold plateau avoids choosing the dark
     * class value itself on an almost binary image. No smoothing across scenes. */
    return (uint8_t)((first + last) / 2);
}

void vision_midterm_process(const uint8_t *gray, uint8_t *binary, vision_result_t *out)
{
    uint32_t hist[256] = {0};
    const uint32_t count = (VISION_ROI_BOTTOM - VISION_ROI_TOP + 1) * VISION_WIDTH;
    uint32_t acc = 0, whites = 0;
    int x, y, acquired = 0, gaps = 0, near_rows = 0;
    float prediction = previous_valid ? previous_seed : (VISION_WIDTH - 1) * 0.5f;
    float seed = prediction, error_sum = 0, weights = 0;
    float near_sum = 0, far_sum = 0;
    int near_n = 0, far_n = 0;
    memset(out, 0, sizeof(*out));
    memset(out->left, VISION_INVALID_X, sizeof(out->left));
    memset(out->right, VISION_INVALID_X, sizeof(out->right));
    memset(out->center, VISION_INVALID_X, sizeof(out->center));
    for (y = 0; y < VISION_HEIGHT; ++y) {
        if (width_age[y] < 255) ++width_age[y];
    }
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y)
        for (x = 0; x < VISION_WIDTH; ++x) ++hist[gray[y * VISION_WIDTH + x]];
    for (x = 0; x < 256; ++x) {
        acc += hist[x];
        if (acc >= count / 20) { out->gray_low = (uint8_t)x; break; }
    }
    acc = 0;
    for (x = 0; x < 256; ++x) {
        acc += hist[x];
        if (acc >= count * 19 / 20) { out->gray_high = (uint8_t)x; break; }
    }
    out->contrast = out->gray_high - out->gray_low;
    out->threshold = otsu(hist, count);
    /* A three-pixel horizontal vote removes isolated threshold speckles.
     * Read only gray and write only binary, so neighborhood order cannot bias it. */
    for (y = 0; y < VISION_HEIGHT; ++y) {
        for (x = 0; x < VISION_WIDTH; ++x) {
            int i = y * VISION_WIDTH + x;
            int vote = gray[i] > out->threshold;
            if (x > 0 && x < VISION_WIDTH - 1) {
                vote += gray[i - 1] > out->threshold;
                vote += gray[i + 1] > out->threshold;
                vote = vote >= 2;
            }
            binary[i] = vote ? 255 : 0;
            if (y >= VISION_ROI_TOP && y <= VISION_ROI_BOTTOM && vote) ++whites;
        }
    }
    out->white_percent = (uint8_t)(whites * 100 / count);
    if (out->contrast < VISION_MIN_CONTRAST) {
        out->status = VISION_LOW_CONTRAST;
        previous_valid = 0;
        /* Do not reuse lane widths across lens occlusion/exposure loss. */
        memset(width_age, 255, sizeof(width_age));
        return;
    }
    for (y = VISION_ROI_BOTTOM; y >= VISION_ROI_TOP; --y) {
        const uint8_t *row = binary + y * VISION_WIDTH;
        float best_score = 1.0e9f, best_center = 0;
        int best_l = -1, best_r = -1, best_flags = 0;
        for (x = 0; x < VISION_WIDTH;) {
            int l, r, has_l, has_r, flags;
            float center, score;
            while (x < VISION_WIDTH && !row[x]) ++x;
            if (x == VISION_WIDTH) break;
            l = x;
            while (x < VISION_WIDTH && row[x]) ++x;
            r = x - 1;
            if (r - l + 1 < VISION_MIN_WIDTH) continue;
            has_l = l >= 2 && !row[l - 1] && !row[l - 2];
            has_r = r < VISION_WIDTH - 2 && !row[r + 1] && !row[r + 2];
            flags = has_l | (has_r << 1);
            if (flags == 3) {
                center = (l + r) * 0.5f;
            } else if ((flags == 1 || flags == 2) && previous_valid &&
                       width_age[y] <= VISION_MAX_INFER_AGE && learned_width[y] >= VISION_MIN_WIDTH) {
                /* Infer only a boundary clipped by the image edge, never one
                 * hidden inside the image by an arbitrary dark obstruction. */
                if (flags == 1 && r == VISION_WIDTH - 1) r = l + learned_width[y] - 1;
                else if (flags == 2 && l == 0) l = r - learned_width[y] + 1;
                else continue;
                center = (l + r) * 0.5f;
                if (center < 0 || center >= VISION_WIDTH) continue;
            } else continue;
            if (acquired && vabs(center - prediction) > 14.0f * (gaps + 1)) continue;
            score = 4.0f * vabs(center - prediction) - 0.25f * (r - l + 1);
            if (flags != 3) score += 20;
            if (score < best_score) {
                best_score = score; best_center = center;
                best_l = l; best_r = r; best_flags = flags;
            }
        }
        if (!best_flags) {
            if (acquired) { if (++gaps > 4) break; }
            else if (y <= VISION_SEED_TOP) break;
            continue;
        }
        if (!acquired) { seed = best_center; acquired = 1; }
        prediction = best_center;
        gaps = 0;
        out->left[y] = (best_l >= 0 && best_l < VISION_WIDTH) ? (uint8_t)best_l : VISION_INVALID_X;
        out->right[y] = (best_r >= 0 && best_r < VISION_WIDTH) ? (uint8_t)best_r : VISION_INVALID_X;
        out->center[y] = (uint8_t)(best_center + 0.5f);
        out->row_flags[y] = (uint8_t)best_flags;
        if (best_flags == 3) {
            learned_width[y] = (uint16_t)(best_r - best_l + 1);
            width_age[y] = 0;
            ++out->measured_rows;
        } else ++out->inferred_rows;
        if (y >= 80) ++near_rows;
        if (y >= 45 && y <= 100) {
            float weight = y >= 80 ? 3.0f : (y >= 60 ? 2.0f : 1.0f);
            if (best_flags != 3) weight *= 0.5f;
            error_sum += weight * (best_center - (VISION_WIDTH - 1) * 0.5f);
            weights += weight;
        }
        if (y >= 85 && y <= 100) { near_sum += best_center; ++near_n; }
        if (y >= 45 && y <= 64) { far_sum += best_center; ++far_n; }
    }
    out->confidence = (uint8_t)((2U * out->measured_rows + out->inferred_rows) * 100U /
                                (2U * (VISION_ROI_BOTTOM - VISION_ROI_TOP + 1)));
    out->valid = near_rows >= 6 && out->measured_rows + out->inferred_rows >= 12 &&
                 weights > 0 && out->confidence >= 25;
    out->status = out->valid ? VISION_TRACK : VISION_NO_LANE;
    if (out->valid) {
        out->raw_error_px = error_sum / weights;
        out->error_px = previous_valid ? 0.6f * previous_error + 0.4f * out->raw_error_px : out->raw_error_px;
        if (near_n >= 4 && far_n >= 4) {
            out->bend_px = far_sum / far_n - near_sum / near_n;
            out->bend_valid = 1;
        }
        previous_error = out->error_px;
        previous_seed = seed;
    } else {
        /* Invalid -> valid flag 0 and zero error, never fabricate a centered road.
         * Integration must inspect valid before using error_px for steering. */
        previous_error = 0;
    }
    previous_valid = out->valid;
}
