#include "main.h"
#include "vision_midterm.h"
#include "app_runtime.h"
#include "app_mode.h"
#include "board_camera.h"
#include "vision_drive.h"
#include "track_bayes.h"
#include "vision_field.h"
#include "vision_storage.h"
#include "app_transport.h"
#include "vision_frame.h"
#include "vision_telemetry.h"
#include "midterm_boot.h"

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
static int training_enabled(void){return !VISION_MIDTERM_AUTONOMOUS;}
static uint8_t gray_frame[VISION_HEIGHT][VISION_WIDTH];
static vision_result_t result;
static tb_filter_t track;
static tb_model_t track_model;
static tb_observation_t track_observation;
#if VISION_DRIVE_ENABLE
static vf_parser_t field_parser;
#endif



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

void app_runtime_run(void)
{
    char mode = 't';
    uint8_t midterm_mode = 1; /* 完整赛道先做概率观测，不能用中期控制强行过环岛。 */
    uint8_t fresh = 0, timeout_reset = 0;
    uint32_t frames = 0, last_text = 0, last_image = 0;
#if VISION_DRIVE_ENABLE
    uint32_t last_run_text = 0;
    uint32_t last_motion_text=0;
    uint8_t reported_motion_phase=0,reported_motion_reason=0;
#endif
    uint32_t now = timer_get(GPT_TIM_1), last_frame = now, fps_start = now;
    uint32_t fps_frames = 0, fps_x10 = 0, dropped_copies = 0;
    uint32_t process_ms = 0, history_resets = 0;
#if VISION_ROUTE_SWITCH
    uint32_t max_process_ms=0,max_frame_gap_ms=0;
#endif
    (void)app_mode_take_change(); /* Initial settled boot position is not a runtime switch. */
    vision_midterm_reset();
    tb_default_model(&track_model);
    (void)tb_init(&track, &track_model);
    invalidate_result();
#if VISION_DRIVE_ENABLE
    vision_drive_init(now);
    {
        vd_profile_t saved;
        /* The isolated route target uses its compiled measured profile.
         * A stale saved profile cannot replace it or schedule auto-start. */
#if !VISION_MIDTERM_ROUTE_TRIAL
        if (vs_load(&saved)) (void)vision_drive_configure(&saved);
#endif
        vision_drive_get_profile(&saved);
        {
            track.model.axis_near_x=saved.control.axis_near_x;
            track.model.axis_far_x=saved.control.axis_far_x;
            track.model.near_y=saved.control.near_y;track.model.far_y=saved.control.far_y;
            track.model.band_half_height=saved.control.band_half_height;
            tb_reset(&track);
        }
    }
    vf_init(&field_parser);
#endif
#if VISION_MIDTERM_AUTONOMOUS
    midterm_boot_init(0U,timer_get(GPT_TIM_1));
#else
    vision_telemetry_help();
#endif
    while (1) {
        int selector_changed=app_mode_take_change();
        if (selector_changed) {
#if VISION_DRIVE_ENABLE
#if VISION_ROUTE_SWITCH
            vd_route_switch_mode_stop();
#else
            vision_drive_stop();
#endif
            {
                uint8_t partial=field_parser.active;
                vf_init(&field_parser);
                if(partial)field_parser.active=field_parser.discarded=1;
            }
#endif
#if !VISION_ROUTE_SWITCH
            vision_midterm_reset();
#else
            vision_midterm_route_reset();
#endif
            tb_reset(&track);invalidate_result();fresh=0;
        }
        uint8_t commands[16];
        uint8_t help_requested = 0, reset_requested = 0;
#if VISION_DRIVE_ENABLE
        vd_probe_info_t link_probe;vd_auto_info_t link_auto;
        vision_drive_get_probe(&link_probe);vision_drive_get_auto(&link_auto);
        app_transport_begin(vision_drive_output()->state==VC_RUNNING || link_probe.active ||
            vision_drive_motion_active() || link_auto.state==VD_AUTO_WAITING);
#else
        app_transport_begin(0);
#endif
        uint32_t n = app_transport_read(commands, sizeof(commands));
#if VISION_MIDTERM_AUTONOMOUS
        /* Optional traffic may cancel, never arm or configure. No traffic is required. */
        if(n || app_transport_activity() || app_transport_stopped()) {
            midterm_boot_cancel(timer_get(GPT_TIM_1));vision_drive_stop();
        }
        n=0;
#endif
        if(training_enabled() && app_transport_changed()) {
            mode='t';midterm_mode=1;help_requested=1;reset_requested=1;
#if VISION_DRIVE_ENABLE
            vf_init(&field_parser);
#endif
        }
#if VISION_DRIVE_ENABLE
        if(app_transport_stopped())vf_init(&field_parser);
        /* Any operator traffic cancels a pending boot countdown. A delayed
         * config/query packet must never coexist with an impending launch. */
        if (n || app_transport_activity()) {
            vd_auto_info_t startup;vision_drive_get_auto(&startup);
            if(startup.state==VD_AUTO_WAITING || (startup.restored && !frames))vision_drive_stop();
        }
        uint8_t batch_stop = (uint8_t)(field_parser.active || app_transport_stopped());
        uint8_t probe_allowed = (uint8_t)(mode == 't' && midterm_mode && !app_transport_stopped()), scan_field = field_parser.active;
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
            if(c=='v') {
                int ok=vision_drive_motion_reverse(timer_get(GPT_TIM_1),mode=='t' && midterm_mode && !batch_stop);
                app_console_printf("[MOTION] reverse %s; requires stopped wait>=2000ms and fresh heartbeat\r\n",ok?"ACCEPTED":"REJECT");
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
        if (training_enabled() && mode == 't') {
#if VISION_DRIVE_ENABLE
            if (field_response[0]) app_console_printf("%s", field_response);
#endif
            if (help_requested) vision_telemetry_help();
            if (reset_requested) app_console_printf("[VISION2] tracking history cleared\r\n");
        }
        now = timer_get(GPT_TIM_1);
#if !VISION_MIDTERM_AUTONOMOUS
        app_transport_summary(now,frames,
#if VISION_DRIVE_ENABLE
            (unsigned)vision_drive_output()->state,(unsigned)vision_drive_output()->faults,
#else
            0,0,
#endif
            result.valid,
#if VISION_DRIVE_ENABLE
            (unsigned)(vision_drive_motion_active()?1U:0U)
#else
            0
#endif
            );
        app_transport_poll();
#endif
        tb_poll(&track, now);
#if VISION_DRIVE_ENABLE
        vision_drive_set_local_allowed(app_mode_allows(APP_MODE_DRIVE) && mode=='t' && midterm_mode && !field_parser.active && !app_transport_stopped() && !vision_drive_motion_active());
        vision_drive_poll(now);
#if VISION_MIDTERM_AUTONOMOUS
        midterm_boot_poll(now);
#endif
#endif
        if (board_camera_poll(app_mode_allows(APP_MODE_OBSERVE) || app_mode_allows(APP_MODE_DRIVE), now) && mt9v03x_finish_flag) {
            if (!vision_frame_copy(&gray_frame[0][0])) {
                ++dropped_copies;
            } else {
                uint32_t process_start = timer_get(GPT_TIM_1);
#if VISION_ROUTE_SWITCH
                if(frames&&(uint32_t)(process_start-last_frame)>max_frame_gap_ms)
                    max_frame_gap_ms=process_start-last_frame;
#endif
                if (frames && (uint32_t)(process_start - last_frame) > HISTORY_GAP_MS) {
                    vision_midterm_reset();
                    ++history_resets;
                }
#if VISION_ROUTE_SWITCH
                vision_midterm_route_enable((uint8_t)app_mode_allows(APP_MODE_DRIVE));
                vision_midterm_route_hint(tb_route_hint(&track,process_start));
#endif
                vision_midterm_process(&gray_frame[0][0], &image_two_value[0][0], &result);
                if (track.configured) {
                    tb_extract(&track.model, &image_two_value[0][0], &result, &track_observation);
                    (void)tb_update(&track, &track_observation, frames + 1U, process_start);
                }
                process_ms = (uint32_t)(timer_get(GPT_TIM_1) - process_start);
#if VISION_ROUTE_SWITCH
                if(process_ms>max_process_ms)max_process_ms=process_ms;
#endif
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
        {
            vd_motion_info_t m;vision_drive_motion_get(&m);
            if(training_enabled() && mode=='t' && m.sequence && (reported_motion_phase!=m.phase || reported_motion_reason!=m.reason ||
                (m.phase && (uint32_t)(now-last_motion_text)>=100U))) {
                app_console_printf("[MOTION] seq=%lu kind=%u phase=%u reason=%u requested=%u actual=%u servo_us=%u ms=%lu\r\n",
                    (unsigned long)m.sequence,m.kind,m.phase,m.reason,m.requested_permille,m.actual_permille,m.servo_us,(unsigned long)now);
                reported_motion_phase=m.phase;reported_motion_reason=m.reason;last_motion_text=now;
                vision_telemetry_speed();
            }
            if(m.phase)last_text=now; /* No multi-line vision report during a motion experiment. */
        }
        /* 行驶时限量输出一行，避免原多行浮点诊断阻塞 UART，挤占 100ms
         * 相机租约。停下后恢复完整视觉/标定诊断，不降低安全中断频率。 */
        if (mode == 't' && vision_drive_output()->state == VC_RUNNING) {
            if (training_enabled() && (!vision_drive_is_local() || VISION_ROUTE_SWITCH) && (uint32_t)(now - last_run_text) >= 100U) {
#if VISION_ROUTE_SWITCH
                vision_telemetry_run(frames,now,now-last_frame,max_process_ms,max_frame_gap_ms,&result,&track);
                max_process_ms=max_frame_gap_ms=0;
#else
                const vc_output_t *run = vision_drive_output();
                app_console_printf("[RUN] n=%lu ms=%lu e=%.3f h=%.3f u=%.3f pwm=%.3f q=%u age=%lu\r\n",
                       (unsigned long)frames,(unsigned long)now,(double)run->lateral,
                       (double)run->heading,(double)run->steering,(double)run->duty,
                       result.confidence,(unsigned long)(uint32_t)(now-last_frame));
                vision_telemetry_speed();
#endif
                last_run_text = now;
            }
            last_text = now;
        }
#endif
        if(mode=='t')vision_telemetry_trace_poll(now);
        if (training_enabled() && mode == 't' && (uint32_t)(now - last_text) >= 500U) {
            vision_report_t report;
            report.lane=&result;report.track=&track;report.observation=&track_observation;
            report.timeout_reset=timeout_reset;report.frames=frames;report.midterm_mode=midterm_mode;
            report.last_frame=last_frame;report.fps_x10=fps_x10;report.dropped_copies=dropped_copies;
            report.process_ms=process_ms;report.history_resets=history_resets;report.now=now;
            vision_telemetry_report(&report);
            last_text = now;
        }
        if ((mode == 'g' || mode == 'b' || mode == 'o') && fresh &&
            (uint32_t)(now - last_image) >= 1000U) {
            vision_telemetry_preview(mode, &gray_frame[0][0], &image_two_value[0][0], &result);
            fresh = 0;
            last_image = timer_get(GPT_TIM_1);
        }
    }
}
