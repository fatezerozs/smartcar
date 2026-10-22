#include "main.h"
#include "vision_drive.h"
#include "vision_field.h"
#include "vision_telemetry.h"
#include "app_transport.h"
#include "board_diagnostics.h"
#include "vision_speed.h"
#include "app_link.h"
#if VISION_ROUTE_SWITCH && VISION_DUAL_UART_ENABLE
static uint32_t trace_dump_run,trace_dump_revision,trace_dump_ms;
static unsigned trace_dump_index,trace_dump_phase;
#endif
void vision_telemetry_trace_poll(uint32_t now)
{
#if VISION_ROUTE_SWITCH && VISION_DUAL_UART_ENABLE
    vd_trace_info_t info;vd_trace_sample_t s;char line[448];int n;
    vision_drive_trace_info(&info);
    if(!info.frozen||!info.count||vision_drive_output()->state==VC_RUNNING)return;
    if(trace_dump_run!=info.run||trace_dump_revision!=info.revision){trace_dump_run=info.run;trace_dump_revision=info.revision;trace_dump_index=trace_dump_phase=0;}
    if(trace_dump_phase==3||(uint32_t)(now-trace_dump_ms)<35U)return;
    trace_dump_ms=now;
    if(!trace_dump_phase)n=snprintf(line,sizeof(line),"[TRACE_BEGIN] run=%lu count=%u dt_ms=60 scale=1000 revision=%lu\r\n",(unsigned long)info.run,info.count,(unsigned long)info.revision);
    else if(trace_dump_index<info.count) {
        if(!vision_drive_trace_read(trace_dump_index,&s))return;
        n=snprintf(line,sizeof(line),"[TRACE] run=%lu i=%u ms=%lu frame=%lu e=%d h=%d u=%d duty=%d servo=%u raw=%d/%d target=%d/%d pwm=%u/%u v=%u q=%u f=%u w=%u cx=%u both=%u inferred=%u state=%u source=%u obs=%u faults=%lu junction=%u anchors=%u ring=%u ring_side=%d ring_rows=%u entry_guide=%u\r\n",
            (unsigned long)info.run,trace_dump_index,(unsigned long)s.ms,(unsigned long)s.frame,
            s.lateral,s.heading,s.steer,s.duty,s.servo_us,s.raw3,s.raw4,s.target_right,s.target_left,
            s.pwm_right,s.pwm_left,s.valid,s.coverage,s.row_flags,s.width,s.center,s.measured,s.inferred,s.state,s.source,s.obs_flags,(unsigned long)s.faults,s.junction,s.anchors,s.ring,s.ring_side,s.ring_rows,s.entry_guide_rows);
    } else n=snprintf(line,sizeof(line),"[TRACE_END] run=%lu count=%u\r\n",(unsigned long)info.run,info.count);
    /* The TX queue is nonblocking. If RTS/queue is busy, retain this cursor and
     * retry later. Dump only when stopped, never spend driving time on it. */
    if(n<=0||(unsigned)n>=sizeof(line)||!app_link_send(app_transport_owner(),(const uint8_t *)line,(uint32_t)n))return;
    if(!trace_dump_phase)trace_dump_phase=1;
    else if(trace_dump_index<info.count)++trace_dump_index;
    else trace_dump_phase=3;
#else
    (void)now;
#endif
}
void vision_telemetry_speed(void)
{
#if VISION_ENCODER_ENABLE
    vd_speed_info_t s;vision_drive_speed_get(&s);
    app_console_printf("[SPEED] ms=%lu sample=%lu enabled=%u raw3_L=%d raw4_R=%d target_R/L=%d/%d measured_R/L=%d/%d pwm_R/L=%u/%u faults=%lu; counts/10ms\r\n",
        (unsigned long)timer_get(GPT_TIM_1),(unsigned long)s.samples,s.enabled,s.raw3,s.raw4,s.target_right,s.target_left,s.measured_right,s.measured_left,
        s.pwm_right,s.pwm_left,(unsigned long)s.faults);
#if VISION_ROUTE_SWITCH
    {const vc_output_t *turn=vision_drive_output();
     app_console_printf("[TURN] u=%.3f command_us=%u near_valid=%u far_valid=%u search=%u source=%u speed_mode=%u; commanded PWM, not measured angle\r\n",
        (double)turn->steering,vision_drive_servo_command_us(),turn->observation_valid,turn->far_valid,turn->recovering,turn->search_source,turn->speed_boost);}
#endif
    if(s.faults)app_console_printf("[SPEED_FAULT] detail=%s control_age_ms=%lu sample_age_ms=%lu interval_ms=%lu\r\n",
        vision_drive_speed_fault_name(s.fault_detail),(unsigned long)s.fault_control_age_ms,
        (unsigned long)s.fault_sample_age_ms,(unsigned long)s.fault_interval_ms);
#endif
}
static uint8_t preview[VISION_PREVIEW_WIDTH*VISION_PREVIEW_HEIGHT];
void vision_telemetry_preview(char mode,const uint8_t *gray,const uint8_t *binary,const vision_result_t *result)
{
    uint8_t header[12]={0xAA,0,0x02,0x02,VISION_PREVIEW_WIDTH,0,VISION_PREVIEW_HEIGHT,0,0x01,0,0,0};
    uint8_t checksum=0;unsigned i;
    vision_preview_mode_t kind=mode=='b'?VISION_PREVIEW_BINARY:mode=='o'?VISION_PREVIEW_OVERLAY:VISION_PREVIEW_GRAY;
    vision_midterm_preview(gray,binary,result,preview,kind);
    for(i=0;i<sizeof(header);++i)checksum=(uint8_t)(checksum+header[i]);
    header[1]=checksum;
    app_transport_frame(header,sizeof(header),preview,sizeof(preview));
}

static int coord(uint8_t value) {return value==VISION_INVALID_X?-1:value;}
void vision_telemetry_help(void)
{
#if VISION_ROUTE_SWITCH && VISION_DUAL_UART_ENABLE
    trace_dump_run=trace_dump_revision=trace_dump_ms=0;trace_dump_index=trace_dump_phase=0;
#endif
#if VISION_ENCODER_ENABLE
    app_console_printf("[ENCODER_SPEED] encoder3 LEFT C3/C4, encoder4 RIGHT C5/C25; vendor PULSE_DIR counting; @encoder_status; @encoder_hw; @encoder_sign s3 s4; @speed_cfg max_ticks kp ki; @speed_mode 0/1; PI OFF by default\r\n");
#endif
#if VISION_MOTION_DIAG
    app_console_printf("[MOTION_DIAG] max 150 permille; manual profile required; no auto-start; ramp 100 permille/s; h every 100ms, !=STOP\r\n");
#endif
#if VISION_MOTOR_THRESHOLD_DIAG
    app_console_printf("[THRESHOLD_DIAG] manual single-wheel bench; first <=100, step <=10, max 200 permille / 1500ms; local autostart disabled; !=STOP\r\n");
#endif
#if VISION_DRIVE_ENABLE
    app_console_printf("[MOTION] @motion line/shuttle permille ms; @motion circle permille ms pulse_us; requires hardware calibration, cruise=0; h heartbeat, !=STOP, v=confirm reverse while stopped\r\n");
#if VISION_ROUTE_SWITCH
    app_console_printf("\r\n[VISION2] MIDTERM: camera steering / encoder speed loop; C26 ON may start at boot\r\n");
    app_console_printf("[DRIVE] C26 ON + valid lane=LOCAL run; s/!=STOP; UART a disabled; no heartbeat required; track PWM adaptive PI 0..1000, rise=10/10ms; demand scale=180/1000\r\n");
    app_console_printf("[ROUTE_SPEED] base_ticks=%u fast_ticks=%u corner_ticks=%u edge_ticks=%u per 10ms; mode=0 base/1 paired/2 corner/3 current-edge; junction=forward preference, ring=current inner edge\r\n",
        (unsigned)MT_ROUTE_BASE_TICKS,(unsigned)MT_ROUTE_FAST_TICKS,(unsigned)MT_ROUTE_CORNER_TICKS,(unsigned)MT_ROUTE_EDGE_TICKS);
    app_console_printf("[ROUTE_SERVO] compiled right_us=%u center_us=%u left_us=%u right_sign=%d; operator-confirmed static endpoints\r\n",
        (unsigned)MT_ROUTE_SERVO_RIGHT_US,(unsigned)MT_ROUTE_SERVO_CENTER_US,
        (unsigned)MT_ROUTE_SERVO_LEFT_US,MT_ROUTE_SERVO_RIGHT_SIGN);
#else
    app_console_printf("\r\n[VISION2] MIDTERM: camera steering / open-loop motor; UART arm/stop; physical keys removed; boot is stopped\r\n");
    app_console_printf("[DRIVE] a=REMOTE arm h=REMOTE heartbeat s=STOP c=clear; s/!/image/pause/reset disarms\r\n");
#endif
    app_console_printf("[FIELD] build=%s; @status, @gain kp kd kh, @motor cruise cap rise, @camera near far, @bands near_y far_y half, @servo center min max sign, @motor_dir level1 level2, @duration ms; newline; !=STOP\r\n",VISION_FIELD_BUILD);
    app_console_printf("[OPENLOOP] @balance motor1_scale motor2_scale; 0..1 attenuation; no encoders; PWM is NOT measured speed\r\n");
    app_console_printf("[STARTUP] @startup enabled delay_ms; @save, @load; physical keys removed; boot does not auto-arm; remote a requires h heartbeat\r\n");
    app_console_printf("[PROBE] @probe pulse_us hold_ms; bench only; motors zero; first %uus then <=%uus step; STOP/timeout off\r\n",
        (unsigned)VISION_SERVO_PROBE_INITIAL_US,(unsigned)VISION_SERVO_PROBE_STEP_US);
    app_console_printf("[WHEEL] @wheel channel permille level hold_ms; lifted bench only; one motor; <=%u permille / %ums; NOT forward confirmation\r\n",
        (unsigned)VISION_MOTOR_PROBE_MAX_PERMILLE,(unsigned)VISION_MOTOR_PROBE_MAX_MS);
#else
    app_console_printf("\r\n[VISION2] MIDTERM: straights/bends, actuators OFF\r\n");
#endif
#if VISION_DUAL_UART_ENABLE
    app_console_printf("[LINK] dual UART1/UART8 115200 8N1; x=claim this port while stopped; !=STOP on either port\r\n");
#elif VISION_WIRELESS_UART_ENABLE
    app_console_printf("[VISION2] UART8 wireless %lu 8N1; send ASCII, HEX/auto-send OFF\r\n", (unsigned long)WIRELESS_UART_BUAD_RATE);
#else
    app_console_printf("[VISION2] UART %lu 8N1; send ASCII, HEX/auto-send OFF\r\n", (unsigned long)DEBUG_UART_BAUDRATE);
#endif
#if VISION_ROUTE_SWITCH
    app_console_printf("[MODE] C26 OFF=STOP; ON=continuous; C27 ignored; fresh partial<=1200ms blind<=300ms; reacquire=resume; no UART heartbeat\r\n");
#else
    app_console_printf("[MODE] C26/C27 ON=ground: 00 OBSERVE, 10 SERVO, 01 MOTOR, 11 DRIVE; change=STOP; keys removed; a/h/s via UART\r\n");
#endif
    app_console_printf("[VISION2] t=text g=gray b=binary o=overlay p=pause r=reset ?=help\r\n");
    app_console_printf("[BAYES] f=full-track observer (NO DRIVE), m=midterm control; both stop/reset\r\n");
    app_console_printf("[BAYES] default model UNTRAINED; posterior is not measured accuracy\r\n");
    app_console_printf("[VISION2] image: SeekFree V2, GRAY 94x60, header=12 bytes; no mixed text\r\n");
    app_console_printf("[VISION2] err>0 means lane is RIGHT; valid=0 means DO NOT use err for control\r\n");
}


void vision_telemetry_report(const vision_report_t *d)
{
    board_diagnostics_report();
    vision_telemetry_speed();
#if VISION_DRIVE_ENABLE
            const vc_output_t *drive = vision_drive_output();
#if VISION_ROUTE_SWITCH
            {vd_last_stop_t stop;vision_drive_last_stop(&stop);
             app_console_printf("[LAST_STOP] seq=%lu ms=%lu run_ms=%lu reason=%u faults=%lu speed_detail=%u raw3_L=%d raw4_R=%d target_R/L=%d/%d pwm_R/L=%u/%u prev_e=%.3f prev_h=%.3f prev_u=%.3f prev_servo_us=%u; retained_until_reset\r\n",
                (unsigned long)stop.sequence,(unsigned long)stop.ms,(unsigned long)stop.run_ms,
                stop.reason,(unsigned long)stop.faults,stop.speed_detail,stop.raw3,stop.raw4,
                stop.target_right,stop.target_left,stop.pwm_right,stop.pwm_left,
                (double)stop.previous_lateral,(double)stop.previous_heading,(double)stop.previous_steering,stop.previous_servo_us);}
#endif
            app_console_printf("[DRIVE] ready=%d state=%u faults=%lu near=%.3f heading=%.3f steer=%.3f duty=%.3f far_ok=%u\r\n",
                   vision_drive_ready(), (unsigned)drive->state, (unsigned long)drive->faults, (double)drive->lateral,
                   (double)drive->heading, (double)drive->steering, (double)drive->duty, drive->far_valid);
            app_console_printf("[CAL] valid=%u far_ok=%u near_x=%.5f far_x=%.5f; confirm centered STRAIGHT before @camera\r\n",
                   drive->observation_valid, drive->far_valid, (double)drive->near_center_x, (double)drive->far_center_x);
#endif
            if (d->timeout_reset) {
                app_console_printf("[VISION2] NO_FRAME n=%lu camera stream stopped\r\n", (unsigned long)d->frames);
            } else if (d->frames == 0) {
                app_console_printf("[VISION2] waiting for first camera frame\r\n");
            } else {
                app_console_printf("[BAYES] mode=%s state=%s candidate=%s p=%.3f margin=%.3f entropy=%.3f accepted=%u trained=%u\r\n",
                       d->midterm_mode ? "MIDTERM" : "FULL_OBSERVER", tb_state_name(d->track->result.state),
                       tb_state_name(d->track->result.candidate), (double)d->track->result.probability,
                       (double)d->track->result.margin, (double)d->track->result.entropy,
                       d->track->result.accepted, d->track->result.trained);
                app_console_printf("[FEATURE] n=%lu ms=%lu mask=%lu quality=%.3f heading=%.4f curvature=%.4f open_left=%.4f open_right=%.4f split=%.4f stripes=%.4f\r\n",
                       (unsigned long)d->frames, (unsigned long)d->last_frame,
                       (unsigned long)d->observation->mask, (double)d->observation->quality,
                       (double)d->observation->x[TB_HEADING], (double)d->observation->x[TB_CURVATURE],
                       (double)d->observation->x[TB_OPEN_LEFT], (double)d->observation->x[TB_OPEN_RIGHT],
                       (double)d->observation->x[TB_SPLIT], (double)d->observation->x[TB_STRIPES]);
                app_console_printf("[VISION2] n=%lu fps=%lu.%lu valid=%u state=%s q=%u thr=%u contrast=%u white=%u\r\n",
                       (unsigned long)d->frames, (unsigned long)(d->fps_x10 / 10), (unsigned long)(d->fps_x10 % 10),
                       d->lane->valid, vision_midterm_status_name(d->lane->status), d->lane->confidence,
                       d->lane->threshold, d->lane->contrast, d->lane->white_percent);
                app_console_printf("[VISION2] both=%u inferred=%u err=%.1f raw=%.1f bend=%.1f bend_ok=%u row90 L/M/R=%d/%d/%d width=%u flags=%u drop=%lu junction=%u anchors=%u\r\n",
                       d->lane->measured_rows, d->lane->inferred_rows, (double)d->lane->error_px,
                       (double)d->lane->raw_error_px, (double)d->lane->bend_px, d->lane->bend_valid,
                       coord(d->lane->left[90]), coord(d->lane->center[90]), coord(d->lane->right[90]),
                       d->lane->row_width[90],d->lane->row_flags[90],
                        (unsigned long)d->dropped_copies,d->lane->junction_rows,d->lane->junction_anchors);
                app_console_printf("[VISION2] age_ms=%lu proc_ms=%lu history_reset=%lu row90_thr=%u local_rows=%u ambiguous=%u ring=%u ring_side=%d ring_rows=%u path_fallback=%u entry_guide=%u\r\n",
                       (unsigned long)(uint32_t)(d->now - d->last_frame), (unsigned long)d->process_ms,
                       (unsigned long)d->history_resets, d->lane->row_threshold[90], d->lane->adaptive_rows,
                       d->lane->ambiguous_rows,d->lane->roundabout_state,d->lane->roundabout_side,d->lane->roundabout_rows,d->lane->path_fallback,d->lane->entry_guide_rows);
            }
}
