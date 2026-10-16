#include "main.h"
#include "vision_midterm.h"
#include "vision_debug.h"
#include "zf_device_detector.h"

#if MT9V03X_W != VISION_WIDTH || MT9V03X_H != VISION_HEIGHT
#error "Vision module dimensions must match the MT9V03X camera configuration."
#endif

/* Preview only: 94x60 gray pixels take ~0.49 seconds at 115200, 8N1.
 * Full-resolution 188x120 frames still feed the algorithm. */
#define PREVIEW_W (VISION_WIDTH / 2)
#define PREVIEW_H (VISION_HEIGHT / 2)
static uint8_t gray_frame[VISION_HEIGHT][VISION_WIDTH];
static uint8_t preview[PREVIEW_H][PREVIEW_W];
static vision_result_t result;

static int coord(uint8_t value) { return value == VISION_INVALID_X ? -1 : value; }

static void help(void)
{
    printf("\r\n[VISION2] MIDTERM: straights/bends, actuators OFF\r\n");
    printf("[VISION2] UART 115200 8N1; send ASCII, HEX/auto-send OFF\r\n");
    printf("[VISION2] t=text g=gray b=binary o=overlay p=pause r=reset ?=help\r\n");
    printf("[VISION2] image: SeekFree protocol, GRAY 94x60; no text mixed into image stream\r\n");
    printf("[VISION2] err>0 means lane is RIGHT; valid=0 means DO NOT use err for control\r\n");
}

static void send_preview(char mode)
{
    int x, y;
    for (y = 0; y < PREVIEW_H; ++y)
        for (x = 0; x < PREVIEW_W; ++x)
            preview[y][x] = mode == 'b' ? image_two_value[y * 2][x * 2] : gray_frame[y * 2][x * 2];
    if (mode == 'o') {
        for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; y += 2) {
            uint8_t flags = result.row_flags[y];
            if (!flags) continue;
            if ((flags & 1) && result.left[y] < VISION_WIDTH) preview[y / 2][result.left[y] / 2] = 96;
            if ((flags & 2) && result.right[y] < VISION_WIDTH) preview[y / 2][result.right[y] / 2] = 96;
            if (result.center[y] < VISION_WIDTH) {
                x = result.center[y] / 2;
                preview[y / 2][x] = flags == 3 ? 0 : 128;
                if (x > 0) preview[y / 2][x - 1] = flags == 3 ? 0 : 128;
            }
        }
    }
    /* Same transport as libraries/zf_device/zf_device_detector.c.
     * Use DETECTOR_GRAY even for 0/255 binary pixels (not bit-packed). */
    detector_camera_information_config(DETECTOR_GRAY, preview, PREVIEW_W, PREVIEW_H);
    detector_camera_boundary_config(NO_BOUNDARY, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    detector_camera_send();
}

void vision_debug_run(void)
{
    char mode = 't';
    uint8_t fresh = 0, timeout_reset = 0;
    uint32_t frames = 0, last_text = 0, last_image = 0;
    uint32_t now = timer_get(GPT_TIM_1), last_frame = now, fps_start = now;
    uint32_t fps_frames = 0, fps_x10 = 0, dropped_copies = 0;
    vision_midterm_reset();
    detector_init(DETECTOR_DEBUG_UART);
    help();
    while (1) {
        uint8_t commands[16];
        uint32_t n = debug_read_ring_buffer(commands, sizeof(commands));
        for (uint32_t i = 0; i < n; ++i) {
            char c = (char)commands[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            if (c == 't' || c == 'g' || c == 'b' || c == 'o' || c == 'p') {
                mode = c;
                last_image = timer_get(GPT_TIM_1) - 1000U;
                if (mode == 't') help();
            } else if (c == 'r') {
                vision_midterm_reset();
                if (mode == 't') printf("[VISION2] tracking history cleared\r\n");
            } else if (c == '?') { mode = 't'; help(); }
        }
        now = timer_get(GPT_TIM_1);
        if (mt9v03x_finish_flag) {
            const uint8_t *completed = (const uint8_t *)mt9v03x_image;
            mt9v03x_finish_flag = 0;
            memcpy(gray_frame, completed, sizeof(gray_frame));
            /* Do not process a copy if the completed camera buffer changed
             * during memcpy. Camera DMA and streaming never touch gray_frame. */
            if (completed != (const uint8_t *)mt9v03x_image) { ++dropped_copies; continue; }
            vision_midterm_process(&gray_frame[0][0], &image_two_value[0][0], &result);
            memcpy(Left_Line, result.left, sizeof(Left_Line));
            memcpy(Right_Line, result.right, sizeof(Right_Line));
            memcpy(Mid_Line, result.center, sizeof(Mid_Line));
            ++frames;
            last_frame = timer_get(GPT_TIM_1);
            fresh = 1;
            timeout_reset = 0;
        }
        now = timer_get(GPT_TIM_1);
        if ((uint32_t)(now - fps_start) >= 1000U) {
            fps_x10 = (frames - fps_frames) * 10000U / (uint32_t)(now - fps_start);
            fps_frames = frames;
            fps_start = now;
        }
        if ((uint32_t)(now - last_frame) >= 2000U && !timeout_reset) {
            vision_midterm_reset();
            result.valid = 0;
            fresh = 0;
            timeout_reset = 1;
        }
        if (mode == 't' && (uint32_t)(now - last_text) >= 500U) {
            if (timeout_reset) {
                printf("[VISION2] NO_FRAME n=%lu camera stream stopped\r\n", (unsigned long)frames);
            } else if (frames == 0) {
                printf("[VISION2] waiting for first camera frame\r\n");
            } else {
                printf("[VISION2] n=%lu fps=%lu.%lu valid=%u state=%s q=%u thr=%u contrast=%u white=%u\r\n",
                       (unsigned long)frames, (unsigned long)(fps_x10 / 10), (unsigned long)(fps_x10 % 10),
                       result.valid, vision_midterm_status_name(result.status), result.confidence,
                       result.threshold, result.contrast, result.white_percent);
                printf("[VISION2] both=%u inferred=%u err=%.1f bend=%.1f bend_ok=%u row90 L/M/R=%d/%d/%d drop=%lu\r\n",
                       result.measured_rows, result.inferred_rows, (double)result.error_px,
                       (double)result.bend_px, result.bend_valid,
                       coord(result.left[90]), coord(result.center[90]), coord(result.right[90]),
                       (unsigned long)dropped_copies);
            }
            last_text = now;
        }
        if ((mode == 'g' || mode == 'b' || mode == 'o') && fresh &&
            (uint32_t)(now - last_image) >= 1000U) {
            send_preview(mode);
            fresh = 0;
            last_image = timer_get(GPT_TIM_1);
        }
    }
}
