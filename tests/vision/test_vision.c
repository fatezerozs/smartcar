#define _CRT_SECURE_NO_WARNINGS
#include "vision_midterm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t gray[VISION_HEIGHT * VISION_WIDTH];
static struct {
    uint8_t before[32], binary[VISION_HEIGHT * VISION_WIDTH], after[32];
} guarded;
static vision_result_t result;
static struct {
    uint8_t before[32], pixels[VISION_PREVIEW_WIDTH * VISION_PREVIEW_HEIGHT], after[32];
} preview_guarded;
static int failures, checks;
static unsigned int rng = 17;

static unsigned int random_u32(void) { rng = rng * 1664525U + 1013904223U; return rng; }
static float absf(float x) { return x < 0 ? -x : x; }
static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) { ++failures; printf("FAIL: %s\n", name); }
}
static void road(int shift, int bend, int noise)
{
    int x, y;
    for (y = 0; y < VISION_HEIGHT; ++y) {
        int center = 94 + shift + bend * (110 - y) / 2;
        int width = 40 + y * 3 / 4;
        for (x = 0; x < VISION_WIDTH; ++x) {
            int value = (x >= center - width / 2 && x <= center + width / 2) ? 210 : 30;
            if (noise) {
                value += (int)(random_u32() % 25) - 12;
                if (random_u32() % 100 < 1) value = random_u32() & 1U ? 255 : 0;
            }
            gray[y * VISION_WIDTH + x] = (uint8_t)value;
        }
    }
}
static void run(void)
{
    int i;
    memset(guarded.before, 0xa5, sizeof(guarded.before));
    memset(guarded.after, 0x5a, sizeof(guarded.after));
    vision_midterm_process(gray, guarded.binary, &result);
    for (i = 0; i < 32; ++i) {
        if (guarded.before[i] != 0xa5 || guarded.after[i] != 0x5a) {
            check(0, "binary buffer guard"); break;
        }
    }
    for (i = 0; i < VISION_HEIGHT; ++i) {
        if (result.row_flags[i]) {
            check(result.center[i] < VISION_WIDTH, "valid center in bounds");
            check(result.left[i] < VISION_WIDTH || result.left[i] == VISION_INVALID_X, "left coordinate");
            check(result.right[i] < VISION_WIDTH || result.right[i] == VISION_INVALID_X, "right coordinate");
        } else check(result.center[i] == VISION_INVALID_X, "invalid row has no fabricated center");
    }
}
static void flat_road(int first_row, int last_row, int left, int right)
{
    int x, y;
    memset(gray, 30, sizeof(gray));
    for (y = first_row; y <= last_row; ++y)
        for (x = left; x <= right; ++x) gray[y * VISION_WIDTH + x] = 210;
}
static void save_pgm(const char *name, const uint8_t *pixels)
{
    FILE *f = fopen(name, "wb");
    if (!f) { check(0, "write test image"); return; }
    fprintf(f, "P5\n%d %d\n255\n", VISION_WIDTH, VISION_HEIGHT);
    fwrite(pixels, 1, sizeof(gray), f);
    fclose(f);
}
static void summary(const char *name)
{
    printf("%-20s valid=%u state=%s q=%u both=%u inferred=%u err=%.2f bend=%.2f\n",
           name, result.valid, vision_midterm_status_name(result.status), result.confidence,
           result.measured_rows, result.inferred_rows, (double)result.error_px, (double)result.bend_px);
}
static void preview_test(const char *save_name)
{
    int mode, x, y, i;
    for (mode = VISION_PREVIEW_GRAY; mode <= VISION_PREVIEW_OVERLAY; ++mode) {
        memset(&preview_guarded, 0xa5, sizeof(preview_guarded));
        vision_midterm_preview(gray, guarded.binary, &result, preview_guarded.pixels,
                               (vision_preview_mode_t)mode);
        for (i = 0; i < 32; ++i)
            check(preview_guarded.before[i] == 0xa5 && preview_guarded.after[i] == 0xa5,
                  "preview buffer guards");
        for (y = 0; y < VISION_PREVIEW_HEIGHT; ++y)
            for (x = 0; x < VISION_PREVIEW_WIDTH; ++x) {
                int index = y * VISION_PREVIEW_WIDTH + x;
                if (mode != VISION_PREVIEW_OVERLAY || !result.valid) {
                    const uint8_t *source = mode == VISION_PREVIEW_BINARY ? guarded.binary : gray;
                    check(preview_guarded.pixels[index] == source[y * 2 * VISION_WIDTH + x * 2],
                          "preview samples the exact processed frame");
                }
            }
        if (mode == VISION_PREVIEW_OVERLAY && result.valid) {
            for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; y += 2)
                if (result.row_flags[y] && result.center[y] < VISION_WIDTH)
                    check(preview_guarded.pixels[y / 2 * VISION_PREVIEW_WIDTH + result.center[y] / 2] ==
                          (result.row_flags[y] == 3 ? 0 : 128), "overlay center marker");
        }
    }
    if (save_name) {
        FILE *f = fopen(save_name, "wb");
        if (!f) { check(0, "save preview"); return; }
        fprintf(f, "P5\n%d %d\n255\n", VISION_PREVIEW_WIDTH, VISION_PREVIEW_HEIGHT);
        fwrite(preview_guarded.pixels, 1, sizeof(preview_guarded.pixels), f);
        fclose(f);
    }
}
int main(void)
{
    int i, x, y;
    vision_midterm_reset(); road(0, 0, 0); run(); summary("straight");
    check(result.valid && absf(result.error_px) < 2, "straight center");
    check(result.measured_rows > 75 && result.inferred_rows == 0, "straight measured boundaries");
    check(result.ambiguous_rows == 0, "unique straight road has no tied candidates");
    save_pgm("synthetic_straight_gray.pgm", gray);
    save_pgm("synthetic_straight_binary.pgm", guarded.binary);
    preview_test("preview_straight.pgm");
    /* White floor both inside and outside two thin black boundary lines. */
    vision_midterm_reset();
    memset(gray, 210, sizeof(gray));
    for (y = 0; y < VISION_HEIGHT; ++y) {
        gray[y * VISION_WIDTH + 40] = gray[y * VISION_WIDTH + 41] = 30;
        gray[y * VISION_WIDTH + 146] = gray[y * VISION_WIDTH + 147] = 30;
    }
    run(); summary("thin black edges");
    check(result.valid && absf(result.error_px) < 2, "thin edges on white floor are a road");
    check(result.left[90] == 42 && result.right[90] == 145, "track interior between thin black edges");
    save_pgm("synthetic_thin_edges.pgm", gray);
    preview_test("preview_thin_edges.pgm");
    vision_midterm_reset(); road(-25, 0, 0); run(); summary("left displacement");
    check(result.valid && result.error_px < -20, "left error sign");
    vision_midterm_reset(); road(25, 0, 0); run(); summary("right displacement");
    check(result.valid && result.error_px > 20, "right error sign");
    vision_midterm_reset(); road(0, -1, 0); run(); summary("left bend");
    check(result.valid && result.bend_valid && result.bend_px < -10, "left bend direction");
    save_pgm("synthetic_left_bend.pgm", gray);
    preview_test("preview_left_bend.pgm");
    vision_midterm_reset(); road(0, 1, 0); run(); summary("right bend");
    check(result.valid && result.bend_valid && result.bend_px > 10, "right bend direction");
    save_pgm("synthetic_right_bend.pgm", gray);
    preview_test("preview_right_bend.pgm");
    vision_midterm_reset(); road(0, 0, 0);
    memset(gray + 98 * VISION_WIDTH, 0, 22 * VISION_WIDTH);
    run(); summary("bottom occlusion");
    check(result.valid && absf(result.error_px) < 2, "acquire above blocked bottom");
    vision_midterm_reset(); road(0, 0, 1); run(); summary("noise");
    check(result.valid && absf(result.error_px) < 3, "salt/pepper noise tolerance");
    vision_midterm_reset(); road(0, 0, 0);
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = VISION_WIDTH / 2; x < VISION_WIDTH; ++x) {
            int idx = y * VISION_WIDTH + x;
            gray[idx] = gray[idx] > 90 ? (uint8_t)(gray[idx] - 90) : 0;
        }
    run(); summary("half shadow");
    check(result.valid && absf(result.error_px) < 3, "lighting step");
    vision_midterm_reset(); road(0, 0, 0);
    for (y = 72; y <= 96; ++y)
        for (x = 0; x < VISION_WIDTH; ++x)
            gray[y * VISION_WIDTH + x] = (uint8_t)(gray[y * VISION_WIDTH + x] * 3 / 10);
    run(); summary("horizontal shadow");
    check(result.valid && absf(result.raw_error_px) < 2, "dark band must not become complete road loss");
    save_pgm("synthetic_shadow.pgm", gray);
    preview_test("preview_shadow.pgm");
    vision_midterm_reset(); road(0, -1, 0);
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = 0; x < VISION_WIDTH; ++x)
            gray[y * VISION_WIDTH + x] = (uint8_t)(gray[y * VISION_WIDTH + x] * (160 - y) / 160);
    run(); summary("brightness slope");
    check(result.valid && result.bend_valid && result.bend_px < -10, "row brightness gradient keeps bend");
    /* A vertical brightness ramp with no actual boundaries is not a lane. */
    vision_midterm_reset();
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = 0; x < VISION_WIDTH; ++x) gray[y * VISION_WIDTH + x] = (uint8_t)(30 + y);
    run(); check(!result.valid, "illumination gradient alone is not a lane");
    memset(gray, 255, sizeof(gray)); run(); summary("all white");
    check(!result.valid && result.status == VISION_LOW_CONTRAST && result.error_px == 0, "white is not a road");
    save_pgm("synthetic_invalid.pgm", gray);
    preview_test("preview_invalid.pgm");
    memset(gray, 0, sizeof(gray)); run(); summary("all black");
    check(!result.valid && result.status == VISION_LOW_CONTRAST, "black is not a road");
    for (i = 0; i < (int)sizeof(gray); ++i) gray[i] = (uint8_t)(125 + random_u32() % 7);
    run(); summary("low contrast");
    check(!result.valid && result.status == VISION_LOW_CONTRAST, "low contrast rejected");
    /* Partial detections must not appear as accepted tracking in the overlay. */
    result.valid = 0; result.row_flags[90] = 3;
    result.left[90] = 42; result.right[90] = 145; result.center[90] = 94;
    preview_test(NULL);
    vision_midterm_reset(); road(0, 0, 0); run();
    road(70, 0, 0); run(); summary("one edge learned");
    check(result.valid && result.inferred_rows > 50, "temporary one-edge fallback");
    check(!result.bend_valid, "do not label inferred-only geometry as measured bend");
    for (i = 2; i <= VISION_MAX_INFER_AGE; ++i) {
        run(); check(result.valid, "single-edge history remains valid through exact age limit");
    }
    run();
    summary("one edge expired");
    check(!result.valid, "expired lane width rejected");
    vision_midterm_reset(); run();
    check(!result.valid, "no width guessing on cold-start one edge");
    /* A rejected eight-row fragment must not teach a different road width.
     * Reacquire only lower rows, then inspect inferred geometry in old upper rows. */
    vision_midterm_reset(); flat_road(0, 119, 34, 153); run();
    check(result.valid, "history contamination setup");
    flat_road(72, 79, 14, 173); run();
    check(!result.valid && result.measured_rows == 8, "short wide fragment rejected");
    flat_road(80, 110, 34, 153); run();
    check(result.valid, "reacquire lower measured rows");
    flat_road(0, 119, 100, 187); run();
    summary("history quarantine");
    check(result.valid && result.row_flags[77] == 1 && result.center[77] == 160,
          "rejected fragment must not poison inferred widths");
    /* An inferred missing edge must enclose the whole observed white run.
     * Here 98 visible pixels cannot fit an 80-pixel historical width. */
    vision_midterm_reset(); flat_road(0, 119, 54, 133); run();
    flat_road(0, 119, 90, 187); run(); summary("right contradiction");
    check(!result.valid && result.inferred_rows == 0,
          "right inference cannot shrink observed white run");
    vision_midterm_reset(); flat_road(0, 119, 54, 133); run();
    flat_road(0, 119, 0, 97); run(); summary("left contradiction");
    check(!result.valid && result.inferred_rows == 0,
          "left inference cannot shrink observed white run");
    /* Sweep clipping in both directions. Even learned widths make mirrored
     * centers exact half-pixels; compare raw float geometry, not rounded x. */
    for (i = 20; i <= 160; i += 10) {
        int visible;
        for (visible = 12; visible <= i + 4; visible += 4) {
            int expected = visible <= i && 188 - visible + (i - 1) * 0.5f <= 187;
            float right_error;
            int right_valid, right_inferred;
            vision_midterm_reset(); flat_road(0, 119, (188-i)/2, (188+i)/2-1); run();
            check(result.valid, "clipping sweep learns double-edge width");
            flat_road(0, 119, 188-visible, 187); run();
            right_error = result.raw_error_px;
            right_valid = result.valid; right_inferred = result.inferred_rows;
            check(right_valid == expected, "right clipping follows measured-width geometry");
            vision_midterm_reset(); flat_road(0, 119, (188-i)/2, (188+i)/2-1); run();
            flat_road(0, 119, 0, visible-1); run();
            check(result.valid == right_valid && result.inferred_rows == right_inferred,
                  "mirrored clipping has equal validity and row coverage");
            check(absf(result.raw_error_px + right_error) < 0.001f,
                  "mirrored clipping reverses raw error");
        }
    }
    /* Learn 118 pixels, then infer center 187.5: rounding must not emit x=188. */
    vision_midterm_reset();
    memset(gray, 30, sizeof(gray));
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = 35; x <= 152; ++x) gray[y * VISION_WIDTH + x] = 210;
    run();
    memset(gray, 30, sizeof(gray));
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = 129; x < VISION_WIDTH; ++x) gray[y * VISION_WIDTH + x] = 210;
    run(); summary("rightmost inference");
    check(!result.valid, "inferred center beyond last pixel rejected");
    /* Illumination changes must adapt without leaking the previous threshold. */
    for (i = 0; i < 40; ++i) {
        vision_midterm_reset(); road(0, i % 2 ? -1 : 1, 0);
        for (x = 0; x < (int)sizeof(gray); ++x)
            gray[x] = (uint8_t)(gray[x] == 30 ? 10 + i : 120 + i * 3);
        run();
        check(result.valid, "exposure sweep keeps lane");
        check(result.bend_valid && (i % 2 ? result.bend_px < -10 : result.bend_px > 10),
              "exposure sweep keeps bend sign");
    }
    /* Paired mirror images should reverse raw error and measured bend. */
    for (i = -20; i <= 20; i += 5) {
        float original_error, original_bend;
        vision_midterm_reset(); road(i, 1, 0); run();
        original_error = result.raw_error_px; original_bend = result.bend_px;
        for (y = 0; y < VISION_HEIGHT; ++y)
            for (x = 0; x < VISION_WIDTH / 2; ++x) {
                uint8_t tmp = gray[y * VISION_WIDTH + x];
                gray[y * VISION_WIDTH + x] = gray[y * VISION_WIDTH + VISION_WIDTH - 1 - x];
                gray[y * VISION_WIDTH + VISION_WIDTH - 1 - x] = tmp;
            }
        vision_midterm_reset(); run();
        check(result.valid && absf(result.raw_error_px + original_error) < 0.01f, "mirror error symmetry");
        check(result.bend_valid && absf(result.bend_px + original_bend) < 0.01f, "mirror bend symmetry");
    }
    /* Consecutive quadratic bends with changing exposure, not just isolated
     * frames. These files also form the offline viewer's animated demo. */
    vision_midterm_reset();
    {
        float last_error = 0;
        FILE *sequence_log = fopen("sequence_curve.csv", "wb");
        if (sequence_log) fprintf(sequence_log, "frame,valid,raw,error,bend,both,inferred,adaptive_rows\n");
        else check(0, "sequence log opens");
        for (i = 0; i < 120; ++i) {
            int curve = i < 60 ? -120 + i * 4 : 120 - (i - 60) * 4;
            int exposure = 70 + i % 31;
            char name[64];
            for (y = 0; y < VISION_HEIGHT; ++y) {
                int center = 94 + curve * (110 - y) * (110 - y) / 12000;
                int width = 40 + y * 3 / 4;
                for (x = 0; x < VISION_WIDTH; ++x)
                    gray[y * VISION_WIDTH + x] = (uint8_t)((x >= center - width/2 && x <= center + width/2 ? 210 : 30) * exposure / 100);
            }
            run();
            check(result.valid, "continuous bend stays tracked");
            check(result.bend_valid, "continuous bend keeps measured near/far geometry");
            if (curve < -30) check(result.bend_px < -2 && result.raw_error_px < 0, "continuous left bend sign");
            if (curve > 30) check(result.bend_px > 2 && result.raw_error_px > 0, "continuous right bend sign");
            if (i > 0) check(absf(result.error_px - last_error) < 3, "continuous error has no large jump");
            last_error = result.error_px;
            snprintf(name, sizeof(name), "sequence_curve_%03d.pgm", i);
            save_pgm(name, gray);
            if (sequence_log) fprintf(sequence_log, "%d,%u,%.3f,%.3f,%.3f,%u,%u,%u\n", i, result.valid,
                (double)result.raw_error_px, (double)result.error_px, (double)result.bend_px,
                result.measured_rows, result.inferred_rows, result.adaptive_rows);
        }
        if (sequence_log) fclose(sequence_log);
    }
    vision_midterm_reset(); road(25, 0, 0); run();
    memset(gray, 30, sizeof(gray)); run();
    road(-25, 0, 0); run();
    check(result.valid && result.error_px < -20, "no stale error after loss/reacquire");
    vision_midterm_reset(); road(0, 0, 0);
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = 2; x < 16; ++x) gray[y * VISION_WIDTH + x] = 210;
    run(); summary("side distractor");
    check(result.valid && absf(result.error_px) < 2, "do not jump to adjacent white object");
    /* Equally scored disconnected candidates have no unique direction.
     * Left-to-right enumeration must not invent a left-turn decision. */
    vision_midterm_reset();
    memset(gray, 30, sizeof(gray));
    for (y = 0; y < VISION_HEIGHT; ++y)
        for (x = 0; x < VISION_WIDTH; ++x)
            if ((x >= 44 && x <= 71) || (x >= 116 && x <= 143)) gray[y * VISION_WIDTH+x] = 210;
    run(); summary("equal candidates");
    check(!result.valid && result.error_px == 0 && result.measured_rows == 0,
          "equally scored candidates are ambiguous, not a left lane");
    check(result.ambiguous_rows > 0 && result.center[90] == VISION_INVALID_X,
          "exact tie is diagnosed and has no fabricated center");
    save_pgm("synthetic_ambiguous.pgm", gray);
    preview_test(NULL);
    /* After unambiguous acquisition, history can genuinely disambiguate the
     * same two candidates. Test each direction with an independent history. */
    for (i = 0; i < 2; ++i) {
        vision_midterm_reset(); flat_road(0, 119, i ? 116 : 44, i ? 143 : 71); run();
        check(result.valid, "unique candidate supplies history");
        memset(gray, 30, sizeof(gray));
        for (y = 0; y < VISION_HEIGHT; ++y)
            for (x = 0; x < VISION_WIDTH; ++x)
                if ((x >= 44 && x <= 71) || (x >= 116 && x <= 143)) gray[y * VISION_WIDTH+x] = 210;
        run();
        check(result.valid && (i ? result.raw_error_px > 30 : result.raw_error_px < -30),
              "measured history disambiguates without systematic left bias");
        check(result.ambiguous_rows == 0, "prior measured direction breaks the score tie");
    }
    /* Midpoint-only continuity cannot detect large changes to both edges.
     * Reject the upper background, but keep usable lower measured geometry. */
    vision_midterm_reset(); flat_road(0, 119, 54, 133);
    for (y = 0; y < 70; ++y)
        for (x = 14; x <= 173; ++x) gray[y * VISION_WIDTH + x] = 210;
    run(); summary("symmetric background");
    check(result.valid && result.row_flags[90] == 3, "bad far geometry does not discard measured near lane");
    check(result.row_flags[69] == 0 && result.row_flags[45] == 0,
          "same midpoint cannot authorize a sudden background widening");
    check(!result.bend_valid && result.measured_rows == 41,
          "rejected background cannot supply bend or inflate coverage");
    preview_test(NULL);
    vision_midterm_preview(gray, guarded.binary, &result, preview_guarded.pixels, VISION_PREVIEW_OVERLAY);
    for (y = 24; y < 70; y += 2)
        for (x = 0; x < VISION_PREVIEW_WIDTH; ++x)
            check(preview_guarded.pixels[y / 2 * VISION_PREVIEW_WIDTH + x] == gray[y * VISION_WIDTH + 2 * x],
                  "rejected far rows remain unannotated in an otherwise valid frame");
    /* A narrow background stripe shifts the midpoint by only 12 pixels,
     * within the old 14-pixel gate, despite unrelated left/right edges. */
    for (i = 0; i < 2; ++i) {
        vision_midterm_reset(); flat_road(70, 119, 34, 153);
        for (y = 0; y < 70; ++y)
            for (x = i ? 66 : 90; x <= (i ? 97 : 121); ++x)
                gray[y * VISION_WIDTH + x] = 210;
        run(); summary(i ? "left false branch" : "right false branch");
        check(result.valid && result.row_flags[69] == 0 && !result.bend_valid,
              "unrelated narrow stripe is rejected in both mirror directions");
        check(absf(result.raw_error_px) < 0.01f,
              "false far branch cannot bias the near-lane error");
    }
    /* Preserve a plausible smooth bend and rejoin it after three missing rows. */
    vision_midterm_reset();
    memset(gray, 30, sizeof(gray));
    for (y = 0; y < VISION_HEIGHT; ++y) {
        int distance = 110 - y;
        int center;
        if (distance < 0) distance = 0;
        if (distance > 40) distance = 40;
        center = 94 + distance * 3 / 2;
        for (x = center - 12; x <= center + 12; ++x) gray[y * VISION_WIDTH + x] = 210;
    }
    memset(gray + 70 * VISION_WIDTH, 30, 3 * VISION_WIDTH);
    run(); summary("bend with row gap");
    check(result.valid && result.bend_valid && result.bend_px > 20,
          "edge continuity preserves smooth bend across a short dark gap");
    check(result.row_flags[69] == 3 && result.row_flags[45] == 3,
          "short gaps resume measured tracking, not permanent truncation");
    /* Rejected far widths must not replace previously measured valid history. */
    vision_midterm_reset(); flat_road(0, 119, 54, 133); run();
    for (y = 0; y < 70; ++y)
        for (x = 14; x <= 173; ++x) gray[y * VISION_WIDTH + x] = 210;
    run();
    check(result.valid && result.row_flags[69] == 0,
          "valid near rows do not authorize rejected far measurements");
    flat_road(0, 119, 120, 187); run();
    check(result.valid && result.row_flags[69] == 1 && result.center[69] == 160,
          "far rejection preserves rather than poisons single-edge width history");
    /* A transient discontinuity can be skipped if the original road returns. */
    vision_midterm_reset(); flat_road(0, 119, 54, 133);
    for (y = 68; y <= 70; ++y)
        for (x = 14; x <= 173; ++x) gray[y * VISION_WIDTH + x] = 210;
    run();
    check(result.valid && result.row_flags[70] == 0 && result.row_flags[67] == 3 && result.bend_valid,
          "short false-width patch is skipped and the real lane resumes");
    for (i = 0; i < 1000; ++i) {
        for (x = 0; x < (int)sizeof(gray); ++x) gray[x] = (uint8_t)(random_u32() >> 24);
        run();
        check(!result.valid, "random noise not trusted as a lane");
    }
    printf("RESULT: %d checks, %d failures (synthetic fixtures, not real-track validation)\n", checks, failures);
    return failures ? 1 : 0;
}
