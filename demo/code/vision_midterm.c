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
    previous_seed = (VISION_WIDTH - 1) * VISION_AXIS_NEAR_X;
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
    /* 沿用原例程大津法思路，跳过空类别，避免全黑/全白时除零。 */
    uint32_t total_sum = 0, back_count = 0, back_sum = 0;
    double best = -1.0;
    int first = 0, last = 0, current_is_best = 0;
    int t;
    for (t = 0; t < 256; ++t) total_sum += hist[t] * (uint32_t)t;
    for (t = 0; t < 255; ++t) {
        double numerator, score;
        back_count += hist[t];
        back_sum += hist[t] * (uint32_t)t;
        if (back_count == 0 || back_count == count) continue;
        /* Empty bins leave both class counts/sums unchanged. Preserve the
         * exact plateau midpoint, but do not repeat the same double division. */
        if (hist[t] == 0) {
            if (current_is_best) last = t;
            continue;
        }
        numerator = (double)total_sum * back_count - (double)back_sum * count;
        score = numerator * numerator / ((double)back_count * (count - back_count));
        if (score > best) { best = score; first = last = t; }
        else if (score == best) { last = t; }
        current_is_best = score == best;
    }
    /* Midpoint of an equally good threshold plateau avoids choosing the dark
     * class value itself on an almost binary image. No smoothing across scenes. */
    return (uint8_t)((first + last) / 2);
}

static void gray_range(const uint32_t *hist, uint32_t count, uint8_t *low, uint8_t *high)
{
    uint32_t acc = 0;
    const uint32_t low_count = count / 50, high_count = count * 49 / 50;
    int x, low_found = 0;
    /* At least three dark pixels in a 188-pixel row: keep two 2-pixel edges
     * while ignoring isolated extreme pixels. */
    for (x = 0; x < 256; ++x) {
        acc += hist[x];
        if (!low_found && acc >= low_count) { *low = (uint8_t)x; low_found = 1; }
        if (acc >= high_count) { *high = (uint8_t)x; break; }
    }
}

void vision_midterm_process(const uint8_t *gray, uint8_t *binary, vision_result_t *out)
{
    uint32_t hist[256] = {0};
    const uint32_t count = (VISION_ROI_BOTTOM - VISION_ROI_TOP + 1) * VISION_WIDTH;
    uint32_t whites = 0;
    int x, y, acquired = 0, gaps = 0, near_rows = 0;
    int last_left = 0, last_right = 0, last_flags = 0;
    float prediction = previous_valid ? previous_seed : (VISION_WIDTH - 1) * VISION_AXIS_NEAR_X;
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
    gray_range(hist, count, &out->gray_low, &out->gray_high);
    out->contrast = out->gray_high - out->gray_low;
    out->threshold = otsu(hist, count);
    /* A three-pixel horizontal vote removes isolated threshold speckles.
     * Read only gray and write only binary, so neighborhood order cannot bias it. */
    for (y = 0; y < VISION_HEIGHT; ++y) {
        const uint8_t *gray_row = gray + y * VISION_WIDTH;
        uint8_t *binary_row = binary + y * VISION_WIDTH;
        uint8_t threshold = out->threshold;
        int previous_vote, current_vote, row_whites;
        if (out->contrast >= VISION_MIN_CONTRAST && y >= VISION_ROI_TOP && y <= VISION_ROI_BOTTOM) {
            uint8_t low = 0, high = 0;
            memset(hist, 0, sizeof(hist));
            for (x = 0; x < VISION_WIDTH; ++x) ++hist[gray[y * VISION_WIDTH + x]];
            gray_range(hist, VISION_WIDTH, &low, &high);
            if (high - low >= VISION_MIN_CONTRAST) {
                int offset = (int)out->threshold - out->gray_low;
                if (offset < 1) offset = 1;
                if (offset >= out->contrast) offset = out->contrast - 1;
                /* 按行灰度范围映射整图大津阈值。横向阴影不会把整条
                 * 白色通道压成黑色；行对比度不足时不做局部推测。 */
                int mapped = low + offset * (high - low) / out->contrast;
                /* 不追随小幅统计噪声，只有明显亮度变化才启用行修正。 */
                if (vabs((float)mapped - out->threshold) >= VISION_ROW_THRESHOLD_DEADBAND) {
                    threshold = (uint8_t)mapped;
                    ++out->adaptive_rows;
                }
            }
        }
        out->row_threshold[y] = threshold;
        /* Rolling votes threshold each source pixel once instead of three
         * times. Keep the original unfiltered first/last pixel semantics. */
        previous_vote = gray_row[0] > threshold;
        current_vote = gray_row[1] > threshold;
        binary_row[0] = previous_vote ? 255 : 0;
        row_whites = previous_vote;
        for (x = 1; x < VISION_WIDTH - 1; ++x) {
            int next_vote = gray_row[x + 1] > threshold;
            int vote = previous_vote + current_vote + next_vote >= 2;
            binary_row[x] = vote ? 255 : 0;
            row_whites += vote;
            previous_vote = current_vote;
            current_vote = next_vote;
        }
        binary_row[VISION_WIDTH - 1] = current_vote ? 255 : 0;
        row_whites += current_vote;
        if (y >= VISION_ROI_TOP && y <= VISION_ROI_BOTTOM) whites += (uint32_t)row_whites;
    }
    out->white_percent = (uint8_t)(whites * 100 / count);
    if (out->contrast < VISION_MIN_CONTRAST) {
        out->status = VISION_LOW_CONTRAST;
        previous_valid = 0;
        /* Do not reuse lane widths across lens occlusion/exposure loss. */
        memset(width_age, 255, sizeof(width_age));
        return;
    }
    /* 从下向上跟踪白色赛道内部；允许绕过少量底部遮挡再寻找起点。 */
    for (y = VISION_ROI_BOTTOM; y >= VISION_ROI_TOP; --y) {
        const uint8_t *row = binary + y * VISION_WIDTH;
        float best_score = 1.0e9f, best_center = 0;
        int best_l = -1, best_r = -1, best_flags = 0, best_tied = 0;
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
                if (flags == 1 && r == VISION_WIDTH - 1) {
                    int inferred_r = l + learned_width[y] - 1;
                    /* 历史宽度必须容纳当前可见白色区域，不能把已经
                     * 看见的白像素切掉，伪造一条位于画面内的缺失边。 */
                    if (inferred_r < r) continue;
                    r = inferred_r;
                } else if (flags == 2 && l == 0) {
                    int inferred_l = r - learned_width[y] + 1;
                    if (inferred_l > l) continue;
                    l = inferred_l;
                } else continue;
                center = (l + r) * 0.5f;
                /* x=187.5 would round to 188, outside a 188-pixel image. */
                if (center < 0 || center > VISION_WIDTH - 1) continue;
            } else continue;
            if (acquired && vabs(center - prediction) > VISION_CENTER_STEP_FRACTION * VISION_WIDTH * (gaps + 1)) continue;
            /* A stable midpoint alone does not prove the same lane: both
             * edges may suddenly widen into a background patch, or collapse
             * onto an unrelated stripe. Compare measured edges independently.
             * Allow a configured fraction of image width per row (about 6 px
             * at 188px), including skipped rows. Do not apply
             * this as measured evidence to an edge inferred from history.
             * Rejected rows follow the existing gap limit; never manufacture
             * a far center or silently straighten the observed road. */
            if (acquired && flags == 3 && last_flags == 3) {
                float edge_step_limit = VISION_EDGE_STEP_FRACTION * VISION_WIDTH * (gaps + 1);
                if (vabs((float)(l - last_left)) > edge_step_limit ||
                    vabs((float)(r - last_right)) > edge_step_limit) continue;
            }
            score = 4.0f * vabs(center - prediction) - 0.25f * (r - l + 1);
            if (flags != 3) score += 20;
            if (score < best_score) {
                best_score = score; best_center = center;
                best_l = l; best_r = r; best_flags = flags;
                best_tied = 0;
            } else if (score == best_score) {
                best_tied = 1;
            }
        }
        /* 分数完全相同，没有依据仅按从左到右的扫描顺序选一条。
         * 视为该行未确定，仍允许后续行消除歧义；不采纳歧义宽度。
         * Only exact ties are rejected here; near ties are not calibrated. */
        if (best_tied) { best_flags = 0; ++out->ambiguous_rows; }
        if (!best_flags) {
            if (acquired) { if (++gaps > VISION_MAX_ROW_GAPS) break; }
            else if (y <= VISION_SEED_TOP) break;
            continue;
        }
        if (!acquired) { seed = best_center; acquired = 1; }
        prediction = best_center;
        last_left = best_l;
        last_right = best_r;
        last_flags = best_flags;
        gaps = 0;
        out->left[y] = (best_l >= 0 && best_l < VISION_WIDTH) ? (uint8_t)best_l : VISION_INVALID_X;
        out->right[y] = (best_r >= 0 && best_r < VISION_WIDTH) ? (uint8_t)best_r : VISION_INVALID_X;
        out->center[y] = (uint8_t)(best_center + 0.5f);
        out->row_flags[y] = (uint8_t)best_flags;
        if (best_flags == 3) {
            ++out->measured_rows;
        } else ++out->inferred_rows;
        if (y >= VISION_NEAR_TOP) ++near_rows;
        if (y >= VISION_ERROR_TOP && y <= VISION_ERROR_BOTTOM) {
            float weight = y >= VISION_NEAR_TOP ? 3.0f : (y >= VISION_MIDDLE_TOP ? 2.0f : 1.0f);
            if (best_flags != 3) weight *= 0.5f;
            error_sum += weight * (best_center - (VISION_WIDTH - 1) * 0.5f);
            weights += weight;
        }
        /* 弯道方向只用双边实测，不能把补线本身的形状当成弯道证据。 */
        if (best_flags == 3) {
            if (y >= VISION_BEND_NEAR_TOP && y <= VISION_BEND_NEAR_BOTTOM) { near_sum += best_center; ++near_n; }
            if (y >= VISION_BEND_FAR_TOP && y <= VISION_BEND_FAR_BOTTOM) { far_sum += best_center; ++far_n; }
        }
    }
    /* q 是有效行覆盖评分，不是识别正确率；单边推测只记半分。 */
    out->confidence = (uint8_t)((2U * out->measured_rows + out->inferred_rows) * 100U /
                                (2U * (VISION_ROI_BOTTOM - VISION_ROI_TOP + 1)));
    out->valid = near_rows >= 6 && out->measured_rows + out->inferred_rows >= 12 &&
                 weights > 0 && out->confidence >= 25;
    out->status = out->valid ? VISION_TRACK : VISION_NO_LANE;
    if (out->valid) {
        /* Commit measured widths only after the whole frame passes validation.
         * A rejected fragment must not poison later single-edge inference.
         * Rejected frames still age the last accepted measurements. */
        for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y) {
            if (out->row_flags[y] == 3) {
                learned_width[y] = (uint16_t)(out->right[y] - out->left[y] + 1);
                width_age[y] = 0;
            }
        }
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

void vision_midterm_preview(const uint8_t *gray, const uint8_t *binary,
                            const vision_result_t *result, uint8_t *preview,
                            vision_preview_mode_t mode)
{
    const uint8_t *source = mode == VISION_PREVIEW_BINARY ? binary : gray;
    int x, y;
    for (y = 0; y < VISION_PREVIEW_HEIGHT; ++y)
        for (x = 0; x < VISION_PREVIEW_WIDTH; ++x)
            preview[y * VISION_PREVIEW_WIDTH + x] = source[y * 2 * VISION_WIDTH + x * 2];
    if (mode != VISION_PREVIEW_OVERLAY || !result->valid) return;
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; y += 2) {
        uint8_t flags = result->row_flags[y];
        uint8_t *row = preview + y / 2 * VISION_PREVIEW_WIDTH;
        if (!flags) continue;
        if ((flags & 1) && result->left[y] < VISION_WIDTH) row[result->left[y] / 2] = 96;
        if ((flags & 2) && result->right[y] < VISION_WIDTH) row[result->right[y] / 2] = 96;
        if (result->center[y] < VISION_WIDTH) {
            x = result->center[y] / 2;
            row[x] = flags == 3 ? 0 : 128;
            if (x > 0) row[x - 1] = flags == 3 ? 0 : 128;
        }
    }
}
