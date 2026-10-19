#include "main.h"
#include "vision_midterm.h"
#include "vision_debug.h"
#include "vision_drive.h"
#include "track_bayes.h"
#include "vision_field.h"
#include "vision_storage.h"
#if VISION_WIRELESS_UART_ENABLE
#include <stdarg.h>
#include <stdio.h>
/* 无线模式仍走原 UART8 RX 中断/FIFO。整帧、文字、命令统一选同一链路，
 * 不把 USB 和无线命令拼接，避免一个链路的心跳维持另一个链路的运行。 */
static int vision_wireless_printf(const char *format, ...)
{
    char text[VF_RESPONSE_CAPACITY]; int n; va_list args;
    va_start(args, format);
    n = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (n > 0) {
        uint32_t length = (uint32_t)n < sizeof(text) ? (uint32_t)n : sizeof(text) - 1U;
        (void)wireless_uart_send_buffer((const uint8_t *)text, length);
    }
    return n;
}
#undef printf
#define printf vision_wireless_printf
#define debug_read_ring_buffer wireless_uart_read_buffer
#define debug_send_buffer wireless_uart_send_buffer
#endif

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
static tb_filter_t track;
static tb_model_t track_model;
static tb_observation_t track_observation;
#if VISION_DRIVE_ENABLE
static vf_parser_t field_parser;
#endif

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
    memset(&track_observation, 0, sizeof(track_observation));
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
    printf("\r\n[VISION2] MIDTERM: camera steering / open-loop motor; saved profile + C12 may start after delay\r\n");
    printf("[DRIVE] a=REMOTE arm h=REMOTE heartbeat s=STOP c=clear; C12 open/image/pause/reset disarms\r\n");
    printf("[FIELD] build=%s; @status, @gain kp kd kh, @motor cruise cap rise, @camera near far, @bands near_y far_y half, @servo center min max sign, @motor_dir level1 level2, @duration ms; newline; !=STOP\r\n",VISION_FIELD_BUILD);
    printf("[OPENLOOP] @balance motor1_scale motor2_scale; 0..1 attenuation; no encoders; PWM is NOT measured speed\r\n");
    printf("[STARTUP] @startup enabled delay_ms; @save, @load; C12->GND enables; open=STOP; saved boot needs valid lane; no heartbeat for LOCAL run\r\n");
    printf("[PROBE] @probe pulse_us hold_ms; bench only; motors zero; first %uus then <=%uus step; STOP/timeout off\r\n",
        (unsigned)VISION_SERVO_PROBE_INITIAL_US,(unsigned)VISION_SERVO_PROBE_STEP_US);
    printf("[WHEEL] @wheel channel permille level hold_ms; lifted bench only; one motor; <=%u permille / %ums; NOT forward confirmation\r\n",
        (unsigned)VISION_MOTOR_PROBE_MAX_PERMILLE,(unsigned)VISION_MOTOR_PROBE_MAX_MS);
#else
    printf("\r\n[VISION2] MIDTERM: straights/bends, actuators OFF\r\n");
#endif
#if VISION_WIRELESS_UART_ENABLE
    printf("[VISION2] UART8 wireless %lu 8N1; send ASCII, HEX/auto-send OFF\r\n", (unsigned long)WIRELESS_UART_BUAD_RATE);
#else
    printf("[VISION2] UART %lu 8N1; send ASCII, HEX/auto-send OFF\r\n", (unsigned long)DEBUG_UART_BAUDRATE);
#endif
    printf("[VISION2] t=text g=gray b=binary o=overlay p=pause r=reset ?=help\r\n");
    printf("[BAYES] f=full-track observer (NO DRIVE), m=midterm control; both stop/reset\r\n");
    printf("[BAYES] default model UNTRAINED; posterior is not measured accuracy\r\n");
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
    uint8_t midterm_mode = 1; /* 完整赛道先做概率观测，不能用中期控制强行过环岛。 */
    uint8_t fresh = 0, timeout_reset = 0;
    uint32_t frames = 0, last_text = 0, last_image = 0;
#if VISION_DRIVE_ENABLE
    uint32_t last_run_text = 0;
#endif
    uint32_t now = timer_get(GPT_TIM_1), last_frame = now, fps_start = now;
    uint32_t fps_frames = 0, fps_x10 = 0, dropped_copies = 0;
    uint32_t process_ms = 0, history_resets = 0;
    vision_midterm_reset();
    tb_default_model(&track_model);
    (void)tb_init(&track, &track_model);
    invalidate_result();
#if VISION_DRIVE_ENABLE
    vision_drive_init(now);
    {
        vd_profile_t saved;
        if (vs_load(&saved) && vision_drive_configure(&saved)) {
            track.model.axis_near_x=saved.control.axis_near_x;
            track.model.axis_far_x=saved.control.axis_far_x;
            track.model.near_y=saved.control.near_y;track.model.far_y=saved.control.far_y;
            track.model.band_half_height=saved.control.band_half_height;
            tb_reset(&track);
            vision_drive_schedule_boot(0,timer_get(GPT_TIM_1));
        }
    }
    vf_init(&field_parser);
#endif
    help();
    while (1) {
        uint8_t commands[16];
        uint8_t help_requested = 0, reset_requested = 0;
        uint32_t n = debug_read_ring_buffer(commands, sizeof(commands));
#if VISION_DRIVE_ENABLE
        /* Any operator traffic cancels a pending boot countdown. A delayed
         * config/query packet must never coexist with an impending launch. */
        if (n) {
            vd_auto_info_t startup;vision_drive_get_auto(&startup);
            if(startup.state==VD_AUTO_WAITING || (startup.restored && !frames))vision_drive_stop();
        }
        uint8_t batch_stop = field_parser.active;
        uint8_t probe_allowed = mode == 't' && midterm_mode, scan_field = field_parser.active;
        char field_response[VF_RESPONSE_CAPACITY] = {0};
        /* 同一接收批次里停止优先，不能被后面的 a 覆盖。 */
        for (uint32_t i = 0; i < n; ++i) {
            char c = (char)commands[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            if (c == 's' || c == 'g' || c == 'b' || c == 'o' || c == 'p' ||
                c == 'r' || c == 'f' || c == 'm' || c == '@' || c == '!' || c == 3) batch_stop = 1;
            /* Probe permission considers only real top-level STOP/mode bytes,
             * not letters within @probe/@servo. Still scan ! in every state.
             * A STOP before or after a fragmented probe forbids that whole line. */
            if (c == '!' || c == 3) { probe_allowed = 0; scan_field = 0; }
            else if (c == '@') scan_field = 1;
            else if (scan_field) { if (c == '\r' || c == '\n') scan_field = 0; }
            else if (c == 's' || c == 'g' || c == 'b' || c == 'o' || c == 'p' || c == 'r' || c == 'f' || c == 'm')
                probe_allowed = 0;
        }
        field_parser.probe_permitted = probe_allowed;
        if (!probe_allowed) field_parser.probe_line_permitted = 0;
        if (batch_stop) vision_drive_stop();
#endif
        for (uint32_t i = 0; i < n; ++i) {
            char c = (char)commands[i];
#if VISION_DRIVE_ENABLE
            int consumed = vf_feed(&field_parser, commands[i], timer_get(GPT_TIM_1), field_response);
            if (consumed) {
                if (field_parser.changed) {
                    vd_profile_t current;
                    vision_drive_get_profile(&current);
                    track.model.axis_near_x = current.control.axis_near_x;
                    track.model.axis_far_x = current.control.axis_far_x;
                    track.model.near_y = current.control.near_y;
                    track.model.far_y = current.control.far_y;
                    track.model.band_half_height = current.control.band_half_height;
                    tb_reset(&track);
                    field_parser.changed = 0;
                }
                continue;
            }
#endif
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
#if VISION_DRIVE_ENABLE
            if (c == 's' || c == 'c' || c == 'a' || c == 'h') {
                (void)vision_drive_command(c, timer_get(GPT_TIM_1), mode == 't' && midterm_mode && !batch_stop);
            }
            if (c == 'g' || c == 'b' || c == 'o' || c == 'p' || c == 'r') vision_drive_stop();
#endif
            if (c == 'f' || c == 'm') {
                midterm_mode = (uint8_t)(c == 'm');
                reset_requested = 1; help_requested = 1;
            } else if (c == 't' || c == 'g' || c == 'b' || c == 'o' || c == 'p') {
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
            tb_reset(&track);
            invalidate_result();
            fresh = 0;
        }
        if (mode == 't') {
#if VISION_DRIVE_ENABLE
            if (field_response[0]) printf("%s", field_response);
#endif
            if (help_requested) help();
            if (reset_requested) printf("[VISION2] tracking history cleared\r\n");
        }
        now = timer_get(GPT_TIM_1);
        tb_poll(&track, now);
#if VISION_DRIVE_ENABLE
        vision_drive_set_local_allowed(mode=='t' && midterm_mode && !field_parser.active);
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
                if (track.configured) {
                    tb_extract(&track.model, &image_two_value[0][0], &result, &track_observation);
                    (void)tb_update(&track, &track_observation, frames + 1U, process_start);
                }
                process_ms = (uint32_t)(timer_get(GPT_TIM_1) - process_start);
                memcpy(Left_Line, result.left, sizeof(Left_Line));
                memcpy(Right_Line, result.right, sizeof(Right_Line));
                memcpy(Mid_Line, result.center, sizeof(Mid_Line));
                ++frames;
                last_frame = process_start;
#if VISION_DRIVE_ENABLE
                if (midterm_mode) vision_drive_observe(&result, frames, process_start, timer_get(GPT_TIM_1));
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
            tb_reset(&track);
            invalidate_result();
            fresh = 0;
            timeout_reset = 1;
        }
#if VISION_DRIVE_ENABLE
        /* 行驶时限量输出一行，避免原多行浮点诊断阻塞 UART，挤占 100ms
         * 相机租约。停下后恢复完整视觉/标定诊断，不降低安全中断频率。 */
        if (mode == 't' && vision_drive_output()->state == VC_RUNNING) {
            if (!vision_drive_is_local() && (uint32_t)(now - last_run_text) >= 50U) {
                const vc_output_t *run = vision_drive_output();
                printf("[RUN] n=%lu ms=%lu e=%.3f h=%.3f u=%.3f pwm=%.3f q=%u age=%lu\r\n",
                       (unsigned long)frames,(unsigned long)now,(double)run->lateral,
                       (double)run->heading,(double)run->steering,(double)run->duty,
                       result.confidence,(unsigned long)(uint32_t)(now-last_frame));
                last_run_text = now;
            }
            last_text = now;
        }
#endif
        if (mode == 't' && (uint32_t)(now - last_text) >= 500U) {
#if VISION_DRIVE_ENABLE
            const vc_output_t *drive = vision_drive_output();
            printf("[DRIVE] ready=%d state=%u faults=%lu near=%.3f heading=%.3f steer=%.3f duty=%.3f far_ok=%u\r\n",
                   vision_drive_ready(), (unsigned)drive->state, (unsigned long)drive->faults, (double)drive->lateral,
                   (double)drive->heading, (double)drive->steering, (double)drive->duty, drive->far_valid);
            printf("[CAL] valid=%u far_ok=%u near_x=%.5f far_x=%.5f; confirm centered STRAIGHT before @camera\r\n",
                   drive->observation_valid, drive->far_valid, (double)drive->near_center_x, (double)drive->far_center_x);
#endif
            if (timeout_reset) {
                printf("[VISION2] NO_FRAME n=%lu camera stream stopped\r\n", (unsigned long)frames);
            } else if (frames == 0) {
                printf("[VISION2] waiting for first camera frame\r\n");
            } else {
                printf("[BAYES] mode=%s state=%s candidate=%s p=%.3f margin=%.3f entropy=%.3f accepted=%u trained=%u\r\n",
                       midterm_mode ? "MIDTERM" : "FULL_OBSERVER", tb_state_name(track.result.state),
                       tb_state_name(track.result.candidate), (double)track.result.probability,
                       (double)track.result.margin, (double)track.result.entropy,
                       track.result.accepted, track.result.trained);
                printf("[FEATURE] n=%lu ms=%lu mask=%lu quality=%.3f heading=%.4f curvature=%.4f open_left=%.4f open_right=%.4f split=%.4f stripes=%.4f\r\n",
                       (unsigned long)frames, (unsigned long)last_frame,
                       (unsigned long)track_observation.mask, (double)track_observation.quality,
                       (double)track_observation.x[TB_HEADING], (double)track_observation.x[TB_CURVATURE],
                       (double)track_observation.x[TB_OPEN_LEFT], (double)track_observation.x[TB_OPEN_RIGHT],
                       (double)track_observation.x[TB_SPLIT], (double)track_observation.x[TB_STRIPES]);
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
