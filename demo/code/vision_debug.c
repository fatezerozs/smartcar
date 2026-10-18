#include "main.h"
#include "vision_midterm.h"
#include "vision_debug.h"
#include "vision_drive.h"

#if MT9V03X_W != VISION_WIDTH || MT9V03X_H != VISION_HEIGHT
#error "Vision module dimensions must match the MT9V03X camera configuration."
#endif
#if !DEBUG_UART_USE_INTERRUPT
#error "Vision commands require the existing debug UART RX interrupt."
#endif

/* Preview only: 94x60 gray pixels take ~0.49 seconds at 115200, 8N1.
 * Full-resolution 188x120 frames still feed the algorithm. */
#define PREVIEW_W VISION_PREVIEW_WIDTH
#define PREVIEW_H VISION_PREVIEW_HEIGHT
#define HISTORY_GAP_MS 250U
static uint8_t gray_frame[VISION_HEIGHT][VISION_WIDTH];
static uint8_t preview[PREVIEW_H][PREVIEW_W];
static vision_result_t result;

static int coord(uint8_t value) { return value == VISION_INVALID_X ? -1 : value; }

/* The library pointer is changed by CSI ISR but is not declared volatile.
 * Read the pointer object through its volatile-qualified type on both checks. */
static const uint8_t *camera_buffer(void)
{
    uint8_t (* volatile *shared)[MT9V03X_W] = &mt9v03x_image;
    return (const uint8_t *)*shared;
}

/* 复位/断流时同时清掉对外结果，避免串口或后续控制误用上一帧。 */
static void invalidate_result(void)
{
    memset(&result, 0, sizeof(result));
    memset(result.left, VISION_INVALID_X, sizeof(result.left));
    memset(result.right, VISION_INVALID_X, sizeof(result.right));
    memset(result.center, VISION_INVALID_X, sizeof(result.center));
    memcpy(Left_Line, result.left, sizeof(Left_Line));
    memcpy(Right_Line, result.right, sizeof(Right_Line));
    memcpy(Mid_Line, result.center, sizeof(Mid_Line));
}

static void help(void)
{
#if VISION_DRIVE_ENABLE
    printf("\r\n[VISION2] MIDTERM: visual steering; outputs locked until profile + manual arm\r\n");
    printf("[DRIVE] a=arm h=heartbeat s=STOP c=clear; image/pause/reset disarms\r\n");
#else
    printf("\r\n[VISION2] MIDTERM: straights/bends, actuators OFF\r\n");
#endif
    printf("[VISION2] UART %lu 8N1; send ASCII, HEX/auto-send OFF\r\n", (unsigned long)DEBUG_UART_BAUDRATE);
    printf("[VISION2] t=text g=gray b=binary o=overlay p=pause r=reset ?=help\r\n");
    printf("[VISION2] image: SeekFree V2, GRAY 94x60, header=12 bytes; no mixed text\r\n");
    printf("[VISION2] err>0 means lane is RIGHT; valid=0 means DO NOT use err for control\r\n");
}

static void send_preview(char mode)
{
    /* Official SeekFree V2 RT1064 camera config (12 bytes, not V1's 8):
     * AA, sum(excluding checksum), cmd=02, gray=02, width LE, height LE,
     * data_type=01 (image present / little endian), reserved[3]=0.
     * Protocol reference: seekfree_assistant.c blob 41759d0fe9e81a95cd7b06bb07c9b0c5a5f26136.
     * Explicit bytes avoid compiler structure packing and leave vendor libs untouched. */
    uint8_t header[12] = {0xAA, 0, 0x02, 0x02,
                          (uint8_t)PREVIEW_W, (uint8_t)(PREVIEW_W >> 8),
                          (uint8_t)PREVIEW_H, (uint8_t)(PREVIEW_H >> 8),
                          0x01, 0, 0, 0};
    uint8_t checksum = 0;
    vision_preview_mode_t preview_mode = mode == 'b' ? VISION_PREVIEW_BINARY :
                                        mode == 'o' ? VISION_PREVIEW_OVERLAY : VISION_PREVIEW_GRAY;
    vision_midterm_preview(&gray_frame[0][0], &image_two_value[0][0], &result,
                           &preview[0][0], preview_mode);
    for (uint32_t i = 0; i < sizeof(header); ++i) checksum = (uint8_t)(checksum + header[i]);
    header[1] = checksum;
    /* Existing debug_send_buffer blocks until all bytes have been written.
     * Binary preview is still byte-per-pixel GRAY (0/255), never bit-packed. */
    debug_send_buffer(header, sizeof(header));
    debug_send_buffer(&preview[0][0], sizeof(preview));
}

void vision_debug_run(void)
{
    char mode = 't';
    uint8_t fresh = 0, timeout_reset = 0;
    uint32_t frames = 0, last_text = 0, last_image = 0;
    uint32_t now = timer_get(GPT_TIM_1), last_frame = now, fps_start = now;
    uint32_t fps_frames = 0, fps_x10 = 0, dropped_copies = 0;
    uint32_t process_ms = 0, history_resets = 0;
    vision_midterm_reset();
    invalidate_result();
#if VISION_DRIVE_ENABLE
    vision_drive_init(now);
#endif
    help();
    while (1) {
        uint8_t commands[16];
        uint8_t help_requested = 0, reset_requested = 0;
        uint32_t n = debug_read_ring_buffer(commands, sizeof(commands));
        for (uint32_t i = 0; i < n; ++i) {
            char c = (char)commands[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
#if VISION_DRIVE_ENABLE
            if (c == 's' || c == 'c' || c == 'a' || c == 'h') {
                (void)vision_drive_command(c, timer_get(GPT_TIM_1), mode == 't');
            }
            if (c == 'g' || c == 'b' || c == 'o' || c == 'p' || c == 'r') vision_drive_stop();
#endif
            if (c == 't' || c == 'g' || c == 'b' || c == 'o' || c == 'p') {
                if (c != mode) {
                    if (c == 't') help_requested = 1;
                    mode = c;
                    last_image = timer_get(GPT_TIM_1) - 1000U;
                }
            } else if (c == 'r') {
                reset_requested = 1;
            } else if (c == '?') { mode = 't'; help_requested = 1; }
        }
        /* Coalesce one received batch: apply reset once, honor the final mode,
         * and never print intermediate help/acks into the final image stream.
         * Repeating t while already in text mode does not flood the UART. */
        if (reset_requested) {
            vision_midterm_reset();
            invalidate_result();
            fresh = 0;
        }
        if (mode == 't') {
            if (help_requested) help();
            if (reset_requested) printf("[VISION2] tracking history cleared\r\n");
        }
        now = timer_get(GPT_TIM_1);
#if VISION_DRIVE_ENABLE
        vision_drive_poll(now);
#endif
        if (mt9v03x_finish_flag) {
            const uint8_t *completed;
            uint32_t irq_mask = __get_PRIMASK();
            /* Only pointer/flag acknowledgement is atomic, never processing
             * or UART transmission. Preserve the caller's interrupt state. */
            __disable_irq();
            completed = camera_buffer();
            mt9v03x_finish_flag = 0;
            __DMB();
            __set_PRIMASK(irq_mask);
            if (completed) memcpy(gray_frame, completed, sizeof(gray_frame));
            __DMB();
            /* The flag catches a full two-buffer cycle even if pointer values
             * happen to match. Rejected copies still reach timeout handling. */
            if (!completed || mt9v03x_finish_flag || completed != camera_buffer()) {
                ++dropped_copies;
            } else {
                uint32_t process_start = timer_get(GPT_TIM_1);
                if (frames && (uint32_t)(process_start - last_frame) > HISTORY_GAP_MS) {
                    vision_midterm_reset();
                    ++history_resets;
                }
                vision_midterm_process(&gray_frame[0][0], &image_two_value[0][0], &result);
                process_ms = (uint32_t)(timer_get(GPT_TIM_1) - process_start);
                memcpy(Left_Line, result.left, sizeof(Left_Line));
                memcpy(Right_Line, result.right, sizeof(Right_Line));
                memcpy(Mid_Line, result.center, sizeof(Mid_Line));
                ++frames;
                last_frame = process_start;
#if VISION_DRIVE_ENABLE
                vision_drive_observe(&result, frames, process_start, timer_get(GPT_TIM_1));
#endif
                fresh = 1;
                timeout_reset = 0;
            }
        }
        now = timer_get(GPT_TIM_1);
        if ((uint32_t)(now - fps_start) >= 1000U) {
            fps_x10 = (frames - fps_frames) * 10000U / (uint32_t)(now - fps_start);
            fps_frames = frames;
            fps_start = now;
        }
        if ((uint32_t)(now - last_frame) >= 2000U && !timeout_reset) {
            vision_midterm_reset();
            invalidate_result();
            fresh = 0;
            timeout_reset = 1;
        }
        if (mode == 't' && (uint32_t)(now - last_text) >= 500U) {
#if VISION_DRIVE_ENABLE
            const vc_output_t *drive = vision_drive_output();
            printf("[DRIVE] ready=%d state=%u faults=%lu near=%.3f heading=%.3f steer=%.3f duty=%.3f far_ok=%u\r\n",
                   vision_drive_ready(), (unsigned)drive->state, (unsigned long)drive->faults, (double)drive->lateral,
                   (double)drive->heading, (double)drive->steering, (double)drive->duty, drive->far_valid);
#endif
            if (timeout_reset) {
                printf("[VISION2] NO_FRAME n=%lu camera stream stopped\r\n", (unsigned long)frames);
            } else if (frames == 0) {
                printf("[VISION2] waiting for first camera frame\r\n");
            } else {
                printf("[VISION2] n=%lu fps=%lu.%lu valid=%u state=%s q=%u thr=%u contrast=%u white=%u\r\n",
                       (unsigned long)frames, (unsigned long)(fps_x10 / 10), (unsigned long)(fps_x10 % 10),
                       result.valid, vision_midterm_status_name(result.status), result.confidence,
                       result.threshold, result.contrast, result.white_percent);
                printf("[VISION2] both=%u inferred=%u err=%.1f raw=%.1f bend=%.1f bend_ok=%u row90 L/M/R=%d/%d/%d drop=%lu\r\n",
                       result.measured_rows, result.inferred_rows, (double)result.error_px,
                       (double)result.raw_error_px, (double)result.bend_px, result.bend_valid,
                       coord(result.left[90]), coord(result.center[90]), coord(result.right[90]),
                       (unsigned long)dropped_copies);
                printf("[VISION2] age_ms=%lu proc_ms=%lu history_reset=%lu row90_thr=%u local_rows=%u ambiguous=%u\r\n",
                       (unsigned long)(uint32_t)(now - last_frame), (unsigned long)process_ms,
                       (unsigned long)history_resets, result.row_threshold[90], result.adaptive_rows,
                       result.ambiguous_rows);
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
